#include "route.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <float.h>

// A route is the reference run's path without its discarded checkpoint branches. The run is found by its recorded event
// indices, its checkpoint saves and teleports are replayed into a tree, and the branch that reached the end is kept.
// Recorded bhop fails cut their failed attempt where the destination lies on the earlier path.

namespace KZ::progress
{
	static constexpr f32 ROUTE_CELL_SIZE = 128.0f;
	// A grid segment may extend one cell beyond the cell of its midpoint.
	static constexpr f32 MAX_GRID_SEGMENT_LENGTH = 2 * ROUTE_CELL_SIZE;
	// Leaves room for the search radius when converting to cell coordinates.
	static constexpr f32 MAX_INDEXED_COORDINATE = (f32)(INT_MAX / 4) * ROUTE_CELL_SIZE;
	// Treats floating-point roundoff as the same position.
	static constexpr f32 POSITION_EPSILON = 0.001f;
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

} // namespace KZ::progress
