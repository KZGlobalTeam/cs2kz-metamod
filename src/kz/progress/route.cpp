#include "route.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <float.h>

// A route is the reference run's path without its discarded checkpoint branches. The run is found by its recorded event
// indices, its checkpoint saves and teleports are replayed into a tree, and the branch that reached the end is kept.
// Recorded bhop fails cut their failed attempt where the destination lies on the earlier path.
//
// A player is followed along the route instead of being matched to the nearest geometry every time, since players stray
// from the reference line and routes overlap themselves. The whole route is only searched after a teleport or after
// losing the route. A jump is only displayed once it has lasted, so a fall or a wrong match where the route overlaps
// itself does not move the displayed progress.

namespace KZ::progress
{
	static constexpr f32 ROUTE_CELL_SIZE = 128.0f;
	// A grid segment may extend one cell beyond the cell of its midpoint.
	static constexpr f32 MAX_GRID_SEGMENT_LENGTH = 2 * ROUTE_CELL_SIZE;
	// Leaves room for the search radius when converting to cell coordinates.
	static constexpr f32 MAX_INDEXED_COORDINATE = (f32)(INT_MAX / 4) * ROUTE_CELL_SIZE;
	// Treats floating-point roundoff as the same position.
	static constexpr f32 POSITION_EPSILON = 0.001f;
	// Furthest a player can be from the route and still be placed on it.
	static constexpr f32 MAX_POSITION_ERROR = 256.0f;
	// Further than this from the route, the displayed value is marked as approximate.
	static constexpr f32 APPROXIMATE_POSITION_ERROR = 64.0f;
	// When searching the whole route, parts this much further than the nearest one are still candidates.
	static constexpr f32 NEAREST_ERROR_TOLERANCE = 32.0f;
	// Candidates spread over more route distance than this are different parts of the route.
	static constexpr f64 MAX_UNAMBIGUOUS_ROUTE_SPAN = 256.0;
	static constexpr u32 MAX_PROJECTIONS = 8192;
	static constexpr u32 MAX_CELL_LOOKUPS = 4096;
	// How far a followed player may move along the route per update. Moving ahead allows more, as the reference can take
	// a longer line than the player.
	static constexpr f32 FOLLOW_AHEAD_SCALE = 3.0f;
	static constexpr f32 FOLLOW_AHEAD_SLACK = 128.0f;
	static constexpr f32 FOLLOW_BEHIND_SLACK = 64.0f;
	// Route distance a displayed value may move beyond the distance travelled without counting as a jump.
	static constexpr f64 JUMP_SLACK = 128.0;
	// How long a jump has to last before it is displayed.
	static constexpr f64 DIVERGENCE_TIME = 1.5;
	// How fast the displayed value may move towards its target per update, relative to the distance travelled.
	static constexpr f64 GLIDE_TRAVEL_SCALE = 2.0;
	static constexpr f64 GLIDE_SLACK = 64.0;
	// How long the last value stays displayed after the route is lost.
	static constexpr f64 LOST_DISPLAY_TIME = 1.0;
	static constexpr size_t NO_PATH_POINT = SIZE_MAX;
	// Odd multipliers spread cell coordinates over hash buckets.
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

	// The cells of a box around a position.
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

	// The pre-run buffer can hold other completions of a short course, so the run is taken from the recorded event indices.
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
			if (event.type == RPEVENT_UNKNOWN || event.phase < RPEVENT_BEFORE_PHYSICS || event.phase > RPEVENT_AFTER_PHYSICS)
			{
				return false;
			}
			const auto &previous = replay.events[index - 1];
			// Events of one tick must be ordered by phase to be placed between its movement samples.
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
			// Angle and velocity changes do not move the player along the route.
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
				// Some teleports during movement are recorded with the destination as the previous origin, leaving the source unknown.
				if (source != destination)
				{
					Append(source, false);
				}
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
			// Events cannot be placed between the samples of several simulations in one tick.
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

	// Finds where a failure teleport destination lies on the earlier path, if it lies there exactly once.
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
		if (!FindCompletedRun(replay, courseID, run, cancel))
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

	bool Route::Follow(const Vector &position, f64 from, f64 behind, f64 ahead, f64 &distance, f32 &error) const
	{
		if (!IsIndexablePosition(position))
		{
			return false;
		}
		const f64 low = from - behind, high = from + ahead;
		// Segments are ordered by distance, so the window is the run of segments starting before its end.
		auto last =
			std::upper_bound(segments.begin(), segments.end(), high, [](f64 value, const Segment &segment) { return value < segment.distance; });
		error = FLT_MAX;
		for (auto it = last; it != segments.begin();)
		{
			const Segment &segment = *--it;
			if (segment.distance + segment.length < low)
			{
				break;
			}
			// Only the part of the segment inside the window is a candidate.
			f32 first = (f32)Clamp((low - segment.distance) / segment.length, 0.0, 1.0);
			f32 second = (f32)Clamp((high - segment.distance) / segment.length, 0.0, 1.0);
			Vector delta = segment.end - segment.start;
			Projection projection = ProjectPosition(position, segment.start + delta * first, segment.start + delta * second);
			if (projection.error < error)
			{
				error = projection.error;
				distance = segment.distance + (first + projection.fraction * (second - first)) * segment.length;
			}
		}
		return error <= MAX_POSITION_ERROR;
	}

	struct Candidate
	{
		f64 distance;
		f32 error;
	};

	static_function bool GatherCandidates(const Route &route, const Vector &position, std::vector<Candidate> &candidates)
	{
		u32 projections = 0, lookups = 0;
		auto project = [&](size_t index)
		{
			const Segment &segment = route.segments[index];
			Projection projection = ProjectPosition(position, segment.start, segment.end);
			if (projection.error <= MAX_POSITION_ERROR)
			{
				candidates.push_back({segment.distance + projection.fraction * segment.length, projection.error});
			}
			return ++projections <= MAX_PROJECTIONS;
		};
		CellRange cells(position, MAX_POSITION_ERROR + MAX_GRID_SEGMENT_LENGTH * 0.5f);
		for (Cell cell; cells.Next(cell);)
		{
			if (++lookups > MAX_CELL_LOOKUPS)
			{
				return false;
			}
			auto found = route.cells.find(cell);
			if (found == route.cells.end())
			{
				continue;
			}
			for (size_t index : found->second)
			{
				if (!project(index))
				{
					return false;
				}
			}
		}
		for (size_t index : route.longSegments)
		{
			if (!project(index))
			{
				return false;
			}
		}
		return true;
	}

	bool Route::Locate(const Vector &position, f64 hint, f64 &distance, f32 &error) const
	{
		std::vector<Candidate> candidates;
		if (!IsIndexablePosition(position) || !GatherCandidates(*this, position, candidates) || candidates.empty())
		{
			return false;
		}
		f32 nearest = FLT_MAX;
		for (const auto &candidate : candidates)
		{
			nearest = Min(nearest, candidate.error);
		}
		f64 minimum = DBL_MAX, maximum = -1;
		const Candidate *closest = nullptr, *closestToHint = nullptr;
		for (const auto &candidate : candidates)
		{
			if (candidate.error > nearest + NEAREST_ERROR_TOLERANCE)
			{
				continue;
			}
			minimum = Min(minimum, candidate.distance);
			maximum = Max(maximum, candidate.distance);
			if (!closest || candidate.error < closest->error)
			{
				closest = &candidate;
			}
			if (!closestToHint || fabs(candidate.distance - hint) < fabs(closestToHint->distance - hint))
			{
				closestToHint = &candidate;
			}
		}
		const Candidate *chosen = maximum - minimum <= MAX_UNAMBIGUOUS_ROUTE_SPAN ? closest : (hint >= 0 ? closestToHint : nullptr);
		if (!chosen)
		{
			return false;
		}
		distance = chosen->distance;
		error = chosen->error;
		return true;
	}

	void Tracker::Reset()
	{
		*this = {};
	}

	void Tracker::Start()
	{
		this->Reset();
		this->following = 0;
		this->target = 0;
		this->shown = 0;
	}

	void Tracker::OnTeleport()
	{
		this->following = -1;
		this->divergedSince = -1;
		this->hasLastPosition = false;
		this->acceptNext = true;
	}

	bool Tracker::Update(const Route &route, const Vector &position, f64 now, f64 &distance, bool &approximate)
	{
		f32 travel = this->hasLastPosition ? (position - this->lastPosition).Length() : 0.0f;
		this->lastPosition = position;
		this->hasLastPosition = true;
		this->travelSinceTarget += travel;

		f64 found;
		f32 error;
		bool followed =
			this->following >= 0
			&& route.Follow(position, this->following, travel + FOLLOW_BEHIND_SLACK, travel * FOLLOW_AHEAD_SCALE + FOLLOW_AHEAD_SLACK, found, error);
		if (!followed && !route.Locate(position, this->target, found, error))
		{
			this->following = -1;
			distance = this->shown;
			approximate = this->approximate;
			return this->shown >= 0 && !this->acceptNext && now - this->lastMatch <= LOST_DISPLAY_TIME;
		}
		this->following = found;
		this->lastMatch = now;
		this->approximate = error > APPROXIMATE_POSITION_ERROR;

		if (this->target < 0 || this->acceptNext)
		{
			// A teleport destination is displayed as is, so a checkpoint shows the progress it was saved with.
			this->target = this->shown = found;
			this->acceptNext = false;
			this->divergedSince = -1;
			this->travelSinceTarget = 0;
		}
		else if ((followed && this->divergedSince < 0) || fabs(found - this->target) <= this->travelSinceTarget + JUMP_SLACK)
		{
			this->target = found;
			this->divergedSince = -1;
			this->travelSinceTarget = 0;
		}
		else if (this->divergedSince < 0)
		{
			this->divergedSince = now;
		}
		else if (now - this->divergedSince >= DIVERGENCE_TIME)
		{
			this->target = found;
			this->divergedSince = -1;
			this->travelSinceTarget = 0;
		}
		// Catching up after a lag or an accepted jump is shown as a short slide rather than a jump.
		f64 step = travel * GLIDE_TRAVEL_SCALE + GLIDE_SLACK;
		this->shown += Clamp(this->target - this->shown, -step, step);
		distance = this->shown;
		approximate = this->approximate;
		return true;
	}
} // namespace KZ::progress
