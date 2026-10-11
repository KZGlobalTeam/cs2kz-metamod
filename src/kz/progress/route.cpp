#include "route.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <float.h>

// Route construction has five steps: find the completed run, replay checkpoint
// operations into an immutable path tree, take the branch that reached the end,
// remove walk-backs identified by recorded failure teleports, and index segments.
//
// Checkpoint teleports and undo select an earlier saved branch. A known bhop
// failure can remove its failed excursion only when its destination identifies a
// unique earlier position in the same course-zone interval. Ordinary map
// teleports and repeated geometry are not evidence of a failure: their branches
// are preserved, so a legitimate nonlinear course is not shortened by a guess.
//
// Matching projects a player onto nearby segments and converts distance along the
// route into a percentage. A fall can match an earlier segment immediately. At
// overlapping paths, history is used only if it leaves one bounded distance
// range; otherwise no estimate is returned. A teleport invalidates that history
// in the Progress service. The route module only reads replay values.

namespace KZ::progress
{
	// These are engineering bounds, not values calibrated against real runs.
	// Grid size affects lookup cost only; it does not alter the retained geometry.
	static constexpr f32 ROUTE_CELL_SIZE = 128.0f;
	// A midpoint-indexed segment may extend by one cell from its midpoint.
	// Longer segments use a separate index; this bounds empty-cell lookups without
	// subdividing or changing the movement recorded in the replay.
	static constexpr f32 MAX_GRID_SEGMENT_LENGTH = 2 * ROUTE_CELL_SIZE;
	// Leave enough signed-coordinate range for a query radius and its half-length
	// expansion. This is an index overflow guard, not a limit on replay duration.
	static constexpr f32 MAX_INDEXED_COORDINATE = (f32)(INT_MAX / 4) * ROUTE_CELL_SIZE;
	// Raw replay positions are lossless floats. This tolerance only avoids treating
	// floating-point roundoff as a new segment or a different failure destination.
	static constexpr f32 POSITION_EPSILON = 0.001f;
	// Bound the accepted positional error and label estimates beyond the smaller
	// bound as approximate. These choices need in-game evaluation, not a claim of
	// map-independent accuracy.
	static constexpr f32 MAX_POSITION_ERROR = 256.0f;
	static constexpr f32 APPROXIMATE_POSITION_ERROR = 64.0f;
	static constexpr f32 NEAREST_ERROR_TOLERANCE = 8.0f;
	static constexpr f64 MAX_UNAMBIGUOUS_ROUTE_SPAN = 256.0;
	// Budget exhaustion hides a match instead of returning a partial search.
	static constexpr u32 MAX_PLAYER_PROJECTIONS = 4096;
	static constexpr u32 MAX_PLAYER_CELL_LOOKUPS = 4096;
	static constexpr u32 FIRST_ROUTE_REPLAY_VERSION = 6;
	static constexpr size_t NO_PATH_POINT = SIZE_MAX;
	// Independent odd multipliers spread cell coordinates over hash buckets;
	// collisions remain safe because Cell equality compares every coordinate.
	static constexpr u32 CELL_HASH_X = 73856093u;
	static constexpr u32 CELL_HASH_Y = 19349663u;
	static constexpr u32 CELL_HASH_Z = 83492791u;

	size_t CellHash::operator()(const Cell &cell) const
	{
		return (size_t)(u32)cell.x * CELL_HASH_X ^ (size_t)(u32)cell.y * CELL_HASH_Y ^ (size_t)(u32)cell.z * CELL_HASH_Z;
	}

	using TimerEvent = RpEvent::RpEventData::TimerEvent;
	using CheckpointEvent = RpEvent::RpEventData::CheckpointEvent;

	struct RunInterval
	{
		size_t firstEvent {NO_PATH_POINT};
		size_t lastEvent {NO_PATH_POINT};
		u32 firstTick {};
		u32 lastTick {};
	};

	struct PathPoint
	{
		Vector origin;
		bool disconnect;
		u32 milestone;
		bool failureRestart {};
	};

	struct PathNode
	{
		PathPoint point;
		size_t parent;
	};

	struct Projection
	{
		Vector origin;
		f32 fraction;
		f32 error;
	};

	static_function bool IsIndexablePosition(const Vector &position)
	{
		return position.IsValid() && fabsf(position.x) <= MAX_INDEXED_COORDINATE && fabsf(position.y) <= MAX_INDEXED_COORDINATE
			   && fabsf(position.z) <= MAX_INDEXED_COORDINATE;
	}

	static_function Cell GetCell(const Vector &position)
	{
		return {(i32)floorf(position.x / ROUTE_CELL_SIZE), (i32)floorf(position.y / ROUTE_CELL_SIZE), (i32)floorf(position.z / ROUTE_CELL_SIZE)};
	}

	// Iterate a box of cells without exposing three nested coordinate loops to
	// every lookup. The bounds include the whole positional search sphere.
	struct CellRange
	{
		Cell minimum, maximum, current;
		bool finished {};

		CellRange(const Vector &position, f32 radius)
		{
			Vector extent(radius, radius, radius);
			minimum = GetCell(position - extent);
			maximum = GetCell(position + extent);
			current = minimum;
		}

		bool Next(Cell &cell)
		{
			if (finished)
			{
				return false;
			}
			cell = current;
			if (current.z < maximum.z)
			{
				current.z++;
			}
			else if (current.y < maximum.y)
			{
				current.z = minimum.z;
				current.y++;
			}
			else if (current.x < maximum.x)
			{
				current.z = minimum.z;
				current.y = minimum.y;
				current.x++;
			}
			else
			{
				finished = true;
			}
			return true;
		}
	};

	static_function Projection ProjectPosition(const Vector &position, const Vector &start, const Vector &end)
	{
		Vector delta = end - start;
		f32 lengthSquared = delta.LengthSqr();
		f32 fraction = lengthSquared > 0 ? Clamp((position - start).Dot(delta) / lengthSquared, 0.0f, 1.0f) : 0;
		Vector origin = start + delta * fraction;
		return {origin, fraction, (position - origin).Length()};
	}

	// The buffers may contain other completions of a short course. The recorder
	// identifies its own start/end event indices, so neither floating-point timer
	// precision nor guessing the first/last completion affects run selection.
	static_function bool FindCompletedRun(const replaysystem::data::ReplayMovement &replay, i32 courseID, RunInterval &run,
										  const std::atomic<bool> &cancel)
	{
		if (!replay.header.has_run() || !replay.header.run().has_start_event_index() || !replay.header.run().has_end_event_index())
		{
			return false;
		}
		size_t first = replay.header.run().start_event_index();
		size_t last = replay.header.run().end_event_index();
		if (first >= last || last >= replay.events.size())
		{
			return false;
		}
		const auto &start = replay.events[first];
		const auto &end = replay.events[last];
		if (start.type != RPEVENT_TIMER_EVENT || start.data.timer.type != TimerEvent::TIMER_START || start.data.timer.index != courseID
			|| end.type != RPEVENT_TIMER_EVENT || end.data.timer.type != TimerEvent::TIMER_END || end.data.timer.index != courseID
			|| end.serverTick < start.serverTick || start.phase < RPEVENT_BEFORE_PHYSICS || start.phase > RPEVENT_AFTER_PHYSICS)
		{
			return false;
		}
		for (size_t index = first + 1; index <= last; index++)
		{
			if (cancel)
			{
				return false;
			}
			const auto &event = replay.events[index];
			// Playback can skip future events, but their route effects are unknown.
			if (event.type == RPEVENT_UNKNOWN || event.phase < RPEVENT_BEFORE_PHYSICS || event.phase > RPEVENT_AFTER_PHYSICS)
			{
				return false;
			}
			const auto &previous = replay.events[index - 1];
			// Phases identify one simulation within a server tick. A phase restart
			// cannot be assigned to movement samples by this format; only route
			// analysis rejects that interval. Playback keeps the recorded order.
			if (event.serverTick < previous.serverTick || (event.serverTick == previous.serverTick && event.phase < previous.phase))
			{
				return false;
			}
			if (index < last && event.type == RPEVENT_TIMER_EVENT
				&& (event.data.timer.type == TimerEvent::TIMER_START || event.data.timer.type == TimerEvent::TIMER_STOP
					|| event.data.timer.type == TimerEvent::TIMER_END))
			{
				return false;
			}
		}
		run = {first, last, start.serverTick, end.serverTick};
		return true;
	}

	struct CheckpointTree
	{
		const RunInterval &run;
		std::vector<PathNode> nodes;
		std::unordered_map<i32, size_t> checkpoints;
		size_t tail {NO_PATH_POINT};
		size_t undo {NO_PATH_POINT};
		u32 milestone {};
		bool active {};

		size_t Append(const Vector &origin, bool disconnect, bool failureRestart = false)
		{
			if (tail != NO_PATH_POINT && !disconnect && nodes[tail].point.origin == origin && nodes[tail].point.milestone == milestone)
			{
				return tail;
			}
			nodes.push_back({{origin, disconnect, milestone, failureRestart}, tail});
			return tail = nodes.size() - 1;
		}

		bool ApplyTimerEvent(const RpEvent &event, size_t index)
		{
			if (index == run.firstEvent || index == run.lastEvent)
			{
				Vector origin(event.data.timer.origin[0], event.data.timer.origin[1], event.data.timer.origin[2]);
				if (!IsIndexablePosition(origin))
				{
					return false;
				}
				active = index == run.firstEvent;
				Append(origin, false);
			}
			else if (active
					 && (event.data.timer.type == TimerEvent::TIMER_SPLIT || event.data.timer.type == TimerEvent::TIMER_CPZ
						 || event.data.timer.type == TimerEvent::TIMER_STAGE))
			{
				// A failure return cannot erase an ordered course milestone.
				milestone++;
			}
			return true;
		}

		bool ApplyCheckpointEvent(const RpEvent &event)
		{
			const auto &checkpoint = event.data.checkpoint;
			if (checkpoint.type == CheckpointEvent::CHECKPOINT_RESET)
			{
				checkpoints.clear();
				undo = NO_PATH_POINT;
				return true;
			}
			Vector origin(checkpoint.origin[0], checkpoint.origin[1], checkpoint.origin[2]);
			if (checkpoint.type != CheckpointEvent::CHECKPOINT_SAVE || !IsIndexablePosition(origin) || checkpoint.index <= 0)
			{
				return false;
			}
			checkpoints[checkpoint.index] = Append(origin, false);
			return true;
		}

		void RestoreCheckpoint(const RpEvent &event, const Vector &source, const Vector &destination)
		{
			const auto &teleport = event.data.teleport;
			size_t before = Append(source, false);
			size_t target = NO_PATH_POINT;
			if (teleport.reason == RPTELEPORT_CHECKPOINT_UNDO)
			{
				target = undo;
			}
			else
			{
				auto saved = checkpoints.find(teleport.checkpointIndex);
				if (saved != checkpoints.end())
				{
					target = saved->second;
				}
			}
			undo = before;
			if (target != NO_PATH_POINT)
			{
				tail = target;
			}
			Append(destination, target == NO_PATH_POINT);
		}

		bool ApplyTeleportEvent(const RpEvent &event)
		{
			const auto &teleport = event.data.teleport;
			// Angles-only and velocity-only changes have no positional branch. No
			// suppression flag can accidentally swallow the next origin teleport.
			if (!teleport.hasOrigin)
			{
				return true;
			}
			Vector source(teleport.previousOrigin[0], teleport.previousOrigin[1], teleport.previousOrigin[2]);
			Vector destination(teleport.origin[0], teleport.origin[1], teleport.origin[2]);
			if (!IsIndexablePosition(source) || !IsIndexablePosition(destination))
			{
				return false;
			}
			if (teleport.reason == RPTELEPORT_CHECKPOINT || teleport.reason == RPTELEPORT_CHECKPOINT_UNDO)
			{
				RestoreCheckpoint(event, source, destination);
			}
			else
			{
				Append(source, false);
				Append(destination, true, teleport.reason == RPTELEPORT_BHOP_FAIL);
			}
			return true;
		}

		bool ApplyEvent(const RpEvent &event, size_t index)
		{
			if (event.type == RPEVENT_TIMER_EVENT)
			{
				return ApplyTimerEvent(event, index);
			}
			if (!active)
			{
				return true;
			}
			switch (event.type)
			{
				case RPEVENT_CHECKPOINT:
					return ApplyCheckpointEvent(event);
				case RPEVENT_TELEPORT:
					return ApplyTeleportEvent(event);
				default:
					return true;
			}
		}

		bool AppendMovement(const Vector &origin, bool noclip)
		{
			if (!active)
			{
				return true;
			}
			if (noclip || !IsIndexablePosition(origin))
			{
				return false;
			}
			Append(origin, false);
			return true;
		}
	};

	struct EventCursor
	{
		const std::vector<RpEvent> &events;
		size_t next;
		size_t last;
		const std::atomic<bool> &cancel;

		bool ApplyThrough(u32 tick, RpEventPhase phase, CheckpointTree &tree)
		{
			while (next <= last)
			{
				if (cancel)
				{
					return false;
				}
				const auto &event = events[next];
				if (event.serverTick > tick || (event.serverTick == tick && event.phase > phase))
				{
					break;
				}
				if (!tree.ApplyEvent(event, next++))
				{
					return false;
				}
			}
			return true;
		}
	};

	static_function bool ReplayCheckpointsIntoTree(const replaysystem::data::ReplayMovement &replay, CheckpointTree &tree,
												   const std::atomic<bool> &cancel)
	{
		EventCursor cursor {replay.events, tree.run.firstEvent, tree.run.lastEvent, cancel};
		u32 previousTick {};
		bool hasSample {};
		for (const auto &sample : replay.samples)
		{
			if (cancel)
			{
				return false;
			}
			if (sample.serverTick < tree.run.firstTick)
			{
				continue;
			}
			if (sample.serverTick > tree.run.lastTick)
			{
				break;
			}
			// Multiple simulations in a run tick have no per-simulation event
			// identity. Do not guess which pre/post positions surround its events.
			// Duplicate ticks outside this run do not affect the reference.
			if (hasSample && sample.serverTick <= previousTick)
			{
				return false;
			}
			previousTick = sample.serverTick;
			hasSample = true;
			if (!cursor.ApplyThrough(sample.serverTick, RPEVENT_BEFORE_PHYSICS, tree) || !tree.AppendMovement(sample.pre, sample.preNoclip)
				|| !cursor.ApplyThrough(sample.serverTick, RPEVENT_DURING_PHYSICS, tree) || !tree.AppendMovement(sample.post, sample.postNoclip)
				|| !cursor.ApplyThrough(sample.serverTick, RPEVENT_AFTER_PHYSICS, tree))
			{
				return false;
			}
		}
		return cursor.next > cursor.last && !tree.active && tree.tail != NO_PATH_POINT;
	}

	static_function std::vector<PathPoint> TakeFinalPath(const CheckpointTree &tree, const std::atomic<bool> &cancel)
	{
		std::vector<PathPoint> path;
		for (size_t index = tree.tail; index != NO_PATH_POINT; index = tree.nodes[index].parent)
		{
			if (cancel)
			{
				return {};
			}
			path.push_back(tree.nodes[index].point);
		}
		std::reverse(path.begin(), path.end());
		return path;
	}

	struct FailureReturn
	{
		size_t segmentEnd {NO_PATH_POINT};
		Vector origin;
		f64 firstDistance {DBL_MAX};
		f64 lastDistance {-1};
	};

	// Only a recorded failure permits removing a walk-back. Repeated nearby paths
	// without that cause may be intentional and remain in the reference. A failure
	// destination must identify one previous position, not several route branches.
	static_function FailureReturn FindFailureReturn(const std::vector<PathPoint> &path, const PathPoint &restart, const std::atomic<bool> &cancel)
	{
		FailureReturn result;
		f64 distance = 0;
		for (size_t index = 1; index < path.size(); index++)
		{
			if (cancel)
			{
				return {};
			}
			const auto &end = path[index];
			const auto &start = path[index - 1];
			if (end.disconnect)
			{
				continue;
			}
			f32 length = (end.origin - start.origin).Length();
			if (end.milestone == restart.milestone && start.milestone == restart.milestone)
			{
				Projection projection = ProjectPosition(restart.origin, start.origin, end.origin);
				if (projection.error <= POSITION_EPSILON)
				{
					f64 along = distance + projection.fraction * length;
					result.firstDistance = Min(result.firstDistance, along);
					result.lastDistance = Max(result.lastDistance, along);
					result.segmentEnd = index;
					result.origin = projection.origin;
				}
			}
			distance += length;
		}
		if (result.lastDistance - result.firstDistance > POSITION_EPSILON)
		{
			result.segmentEnd = NO_PATH_POINT;
		}
		return result;
	}

	static_function bool RemoveWalkbacks(std::vector<PathPoint> &path, const std::atomic<bool> &cancel)
	{
		std::vector<PathPoint> retained;
		retained.reserve(path.size());
		for (const auto &point : path)
		{
			if (cancel)
			{
				return false;
			}
			if (point.failureRestart)
			{
				FailureReturn restart = FindFailureReturn(retained, point, cancel);
				if (cancel)
				{
					return false;
				}
				if (restart.segmentEnd != NO_PATH_POINT)
				{
					retained.resize(restart.segmentEnd);
					retained.push_back({restart.origin, false, point.milestone});
					retained.push_back({point.origin, false, point.milestone});
					continue;
				}
			}
			retained.push_back(point);
		}
		path = std::move(retained);
		return true;
	}

	// Retain each real movement segment once. Teleport displacement is already
	// disconnected; ordinary ticks do not need artificial subdivision. Short
	// segments use midpoint buckets, and long segments use a separate index.
	static_function bool IndexSegments(const std::vector<PathPoint> &path, Route &route, const std::atomic<bool> &cancel)
	{
		for (size_t index = 1; index < path.size(); index++)
		{
			if (cancel)
			{
				return false;
			}
			const auto &start = path[index - 1];
			const auto &end = path[index];
			if (end.disconnect)
			{
				continue;
			}
			f32 length = (end.origin - start.origin).Length();
			if (!std::isfinite(length))
			{
				return false;
			}
			if (length <= POSITION_EPSILON)
			{
				continue;
			}
			if (length <= MAX_GRID_SEGMENT_LENGTH)
			{
				route.cells[GetCell((start.origin + end.origin) * 0.5f)].push_back(route.segments.size());
			}
			else
			{
				route.longSegments.push_back(route.segments.size());
			}
			route.segments.push_back({start.origin, end.origin, route.length, length});
			route.length += length;
		}
		return route.length > 0 && std::isfinite(route.length);
	}

	bool Route::Build(const replaysystem::data::ReplayMovement &replay, i32 courseID, const std::atomic<bool> &cancel)
	{
		segments.clear();
		cells.clear();
		longSegments.clear();
		length = 0;
		RunInterval run;
		if (replay.header.version() < FIRST_ROUTE_REPLAY_VERSION || !FindCompletedRun(replay, courseID, run, cancel))
		{
			return false;
		}
		CheckpointTree tree {run};
		if (!ReplayCheckpointsIntoTree(replay, tree, cancel))
		{
			return false;
		}
		std::vector<PathPoint> path = TakeFinalPath(tree, cancel);
		return RemoveWalkbacks(path, cancel) && IndexSegments(path, *this, cancel);
	}

	struct ProjectionWork
	{
		u32 projections {};
		u32 cellLookups {};
	};

	struct ProjectionCursor
	{
		const Route &route;
		Vector position;
		CellRange cells;
		const std::vector<size_t> *bucket {};
		size_t next {};
		ProjectionWork &work;
		bool exhausted {};
		bool gridFinished {};
		size_t nextLongSegment {};

		ProjectionCursor(const Route &route, const Vector &position, ProjectionWork &work)
			: route(route), position(position), cells(position, MAX_POSITION_ERROR + MAX_GRID_SEGMENT_LENGTH * 0.5f), work(work)
		{
		}

		bool ProjectSegment(size_t index, const Segment *&segment, Projection &projection)
		{
			if (++work.projections > MAX_PLAYER_PROJECTIONS)
			{
				exhausted = true;
				return false;
			}
			segment = &route.segments[index];
			projection = ProjectPosition(position, segment->start, segment->end);
			return true;
		}

		bool Next(const Segment *&segment, Projection &projection)
		{
			while (true)
			{
				if (bucket && next < bucket->size())
				{
					return ProjectSegment((*bucket)[next++], segment, projection);
				}
				if (gridFinished)
				{
					return nextLongSegment < route.longSegments.size() && ProjectSegment(route.longSegments[nextLongSegment++], segment, projection);
				}
				Cell cell;
				if (!cells.Next(cell))
				{
					gridFinished = true;
					continue;
				}
				if (++work.cellLookups > MAX_PLAYER_CELL_LOOKUPS)
				{
					exhausted = true;
					return false;
				}
				auto found = route.cells.find(cell);
				bucket = found == route.cells.end() ? nullptr : &found->second;
				next = 0;
			}
		}
	};

	static_function bool FindNearestProjection(ProjectionCursor &cursor, f32 &nearest)
	{
		const Segment *segment;
		Projection projection;
		while (cursor.Next(segment, projection))
		{
			nearest = Min(nearest, projection.error);
		}
		return !cursor.exhausted && nearest <= MAX_POSITION_ERROR;
	}

	struct ProjectionRange
	{
		f64 minimum {DBL_MAX};
		f64 maximum {-1};
		f64 closest {};
		f32 error {FLT_MAX};

		void Add(f64 distance, f32 positionError)
		{
			minimum = Min(minimum, distance);
			maximum = Max(maximum, distance);
			if (positionError < error || (positionError == error && distance < closest))
			{
				closest = distance;
				error = positionError;
			}
		}

		bool Unambiguous() const
		{
			return maximum >= 0 && maximum - minimum <= MAX_UNAMBIGUOUS_ROUTE_SPAN;
		}
	};

	static_function bool GatherNearbyProjections(ProjectionCursor &cursor, f32 nearest, f64 previousDistance, f32 travel, ProjectionRange &all,
												 ProjectionRange &reachable)
	{
		// History may bridge the observed movement and the positional uncertainty at
		// each end. There is no empirical travel multiplier or monotonic progress cap.
		f32 historyWindow = Max(travel, 0.0f) + 2 * NEAREST_ERROR_TOLERANCE;
		const Segment *segment;
		Projection projection;
		while (cursor.Next(segment, projection))
		{
			if (projection.error > MAX_POSITION_ERROR || projection.error > nearest + NEAREST_ERROR_TOLERANCE)
			{
				continue;
			}
			f64 distance = segment->distance + projection.fraction * segment->length;
			all.Add(distance, projection.error);
			if (previousDistance >= 0 && fabs(distance - previousDistance) <= historyWindow)
			{
				reachable.Add(distance, projection.error);
			}
		}
		return !cursor.exhausted;
	}

	bool Route::Match(const Vector &position, f64 previousDistance, f32 travel, f64 &distance, bool &approximate) const
	{
		if (!IsIndexablePosition(position) || !std::isfinite(travel))
		{
			return false;
		}
		// Nearby short-segment midpoints fit the fixed grid radius. Long segments
		// are projected separately in full, sharing the same total work budget.
		ProjectionWork work;
		ProjectionCursor nearestCursor {*this, position, work};
		f32 nearest = FLT_MAX;
		if (!FindNearestProjection(nearestCursor, nearest))
		{
			return false;
		}
		ProjectionCursor candidateCursor {*this, position, work};
		ProjectionRange all, reachable;
		if (!GatherNearbyProjections(candidateCursor, nearest, previousDistance, travel, all, reachable))
		{
			return false;
		}
		// Distinct distant route positions at the same geometry are ambiguous. Prefer
		// the nearest geometry outright when unique; otherwise constrain by history,
		// and hide progress if even that leaves multiple distant route positions.
		const ProjectionRange &chosen = all.Unambiguous() ? all : reachable;
		if (!chosen.Unambiguous())
		{
			return false;
		}
		distance = chosen.closest;
		approximate = chosen.error > APPROXIMATE_POSITION_ERROR;
		return true;
	}
} // namespace KZ::progress
