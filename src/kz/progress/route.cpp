#include "route.h"
#include <cmath>
#include <float.h>
#include <algorithm>

namespace KZ::progress
{
	static_function Cell GetCell(const Vector &position)
	{
		return {(i32)floorf(position.x / 128), (i32)floorf(position.y / 128), (i32)floorf(position.z / 128)};
	}

	struct PathPoint
	{
		Vector origin;
		bool disconnect;
		u32 milestone;
	};

	// Remove a failed excursion only after the new path follows the earlier path
	// for 64 units. A lone crossing or a reverse walk is not enough to erase a branch.
	static_function bool EraseWalkbacks(std::vector<PathPoint> &path, const std::atomic<bool> &cancel)
	{
		if (path.size() < 2)
		{
			return true;
		}
		std::vector<f64> originalDistance(path.size());
		std::vector<size_t> blockEnd(path.size());
		for (size_t i = 1; i < path.size(); i++)
		{
			if (cancel)
			{
				return false;
			}
			originalDistance[i] = originalDistance[i - 1];
			if (!path[i].disconnect && path[i].milestone == path[i - 1].milestone)
			{
				originalDistance[i] += (path[i].origin - path[i - 1].origin).Length();
			}
		}
		blockEnd.back() = path.size() - 1;
		for (size_t i = path.size() - 1; i-- > 0;)
		{
			blockEnd[i] = !path[i + 1].disconnect && path[i + 1].milestone == path[i].milestone ? blockEnd[i + 1] : i;
		}
		auto sample = [](const std::vector<PathPoint> &points, const std::vector<f64> &distances, size_t first, size_t last, f64 along,
						 Vector &position, Vector &direction)
		{
			if (first >= last || along < distances[first] || along > distances[last])
			{
				return false;
			}
			auto next = std::upper_bound(distances.begin() + first, distances.begin() + last + 1, along);
			size_t j = next == distances.begin() + last + 1 ? last : (size_t)(next - distances.begin());
			while (j > first && distances[j] == distances[j - 1])
			{
				j--;
			}
			if (j <= first)
			{
				return false;
			}
			Vector delta = points[j].origin - points[j - 1].origin;
			f32 length = delta.Length();
			if (length <= 0.001f)
			{
				return false;
			}
			direction = delta / length;
			position = points[j - 1].origin + direction * (f32)(along - distances[j - 1]);
			return true;
		};
		auto cellFor = [](const Vector &position)
		{ return Cell {(i32)floorf(position.x / 64), (i32)floorf(position.y / 64), (i32)floorf(position.z / 64)}; };
		std::vector<PathPoint> result;
		std::vector<f64> distance;
		std::vector<Cell> indexedCell;
		std::vector<bool> indexed;
		std::unordered_map<Cell, std::vector<size_t>, CellHash> grid;
		size_t blockStart = 0;
		auto append = [&](const PathPoint &point)
		{
			bool connected = !result.empty() && !point.disconnect && point.milestone == result.back().milestone;
			if (connected && point.origin == result.back().origin)
			{
				return;
			}
			f32 length = connected ? (point.origin - result.back().origin).Length() : 0;
			Cell cell {};
			bool index = connected && length > 0.001f && length <= 64;
			if (!connected)
			{
				grid.clear();
				blockStart = result.size();
			}
			if (index)
			{
				cell = cellFor((point.origin + result.back().origin) * 0.5f);
				grid[cell].push_back(result.size());
			}
			distance.push_back((distance.empty() ? 0 : distance.back()) + length);
			result.push_back(point);
			indexedCell.push_back(cell);
			indexed.push_back(index);
		};
		auto pop = [&]()
		{
			if (indexed.back())
			{
				auto bucket = grid.find(indexedCell.back());
				bucket->second.pop_back();
				if (bucket->second.empty())
				{
					grid.erase(bucket);
				}
			}
			result.pop_back();
			distance.pop_back();
			indexedCell.pop_back();
			indexed.pop_back();
		};
		for (size_t i = 0; i < path.size(); i++)
		{
			if (cancel)
			{
				return false;
			}
			const auto &point = path[i];
			if (result.empty() || point.disconnect || point.milestone != result.back().milestone
				|| originalDistance[blockEnd[i]] - originalDistance[i] < 64)
			{
				append(point);
				continue;
			}
			Cell center = cellFor(point.origin);
			size_t best = SIZE_MAX;
			f64 bestDistance = DBL_MAX;
			Vector bestPosition;
			u32 candidates = 0;
			bool dense = false;
			for (i32 x = center.x - 1; x <= center.x + 1 && !dense; x++)
			{
				for (i32 y = center.y - 1; y <= center.y + 1 && !dense; y++)
				{
					for (i32 z = center.z - 1; z <= center.z + 1 && !dense; z++)
					{
						auto bucket = grid.find({x, y, z});
						if (bucket == grid.end())
						{
							continue;
						}
						for (size_t j : bucket->second)
						{
							// Bound work on dense jitter without rejecting or truncating the route.
							if (++candidates > 256)
							{
								dense = true;
								break;
							}
							Vector delta = result[j].origin - result[j - 1].origin;
							f32 fraction = Clamp((point.origin - result[j - 1].origin).Dot(delta) / delta.LengthSqr(), 0.0f, 1.0f);
							Vector projected = result[j - 1].origin + delta * fraction;
							f64 along = distance[j - 1] + delta.Length() * fraction;
							if (distance.back() - along < 256 || along >= bestDistance || (point.origin - projected).LengthSqr() > 64)
							{
								continue;
							}
							bool follows = true;
							for (i32 step = 0; step <= 8; step++)
							{
								Vector oldPosition, oldDirection, newPosition, newDirection;
								if (!sample(result, distance, blockStart, result.size() - 1, along + step * 8, oldPosition, oldDirection)
									|| !sample(path, originalDistance, i, blockEnd[i], originalDistance[i] + step * 8, newPosition, newDirection)
									|| (oldPosition - newPosition).LengthSqr() > 64 || oldDirection.Dot(newDirection) < 0.9f)
								{
									follows = false;
									break;
								}
							}
							if (follows)
							{
								best = j;
								bestDistance = along;
								bestPosition = projected;
							}
						}
					}
				}
			}
			if (!dense && best != SIZE_MAX)
			{
				while (result.size() > best)
				{
					pop();
				}
				append({bestPosition, false, point.milestone});
			}
			append(point);
		}
		path = std::move(result);
		return true;
	}

	bool Route::Build(const replaysystem::data::ReplayMovement &replay, i32 courseID, const std::atomic<bool> &cancel)
	{
		segments.clear();
		cells.clear();
		length = 0;
		using TimerEvent = RpEvent::RpEventData::TimerEvent;
		u32 start = 0, end = 0;
		size_t startEvent = SIZE_MAX, endEvent = SIZE_MAX, pendingStart = SIZE_MAX;
		// Run files include a pre-run buffer and a post-run breather. Use the completed
		// interval for this course, rather than treating the whole recording as a run.
		for (u32 i = 0; i < replay.events.size(); i++)
		{
			const auto &event = replay.events[i];
			if (event.type != RPEVENT_TIMER_EVENT)
			{
				continue;
			}
			if (event.data.timer.type == TimerEvent::TIMER_START)
			{
				pendingStart = event.data.timer.index == courseID ? i : SIZE_MAX;
			}
			else if (event.data.timer.type == TimerEvent::TIMER_STOP)
			{
				pendingStart = SIZE_MAX;
			}
			else if (event.data.timer.type == TimerEvent::TIMER_END)
			{
				if (pendingStart != SIZE_MAX && event.data.timer.index == courseID
					&& fabsf(event.data.timer.time - replay.header.run().time()) < 0.001f)
				{
					startEvent = pendingStart;
					endEvent = i;
					start = replay.events[startEvent].serverTick;
					end = event.serverTick;
				}
				pendingStart = SIZE_MAX;
			}
		}
		if (startEvent == SIZE_MAX || endEvent == SIZE_MAX || end < start)
		{
			return false;
		}

		// CP operations are explicit ordered events, not inferred from a tick's final
		// counters. Every saved CP and undo target keeps an immutable path snapshot.
		// Restoring an old CP removes retries; next-CP and repeated undo can restore
		// a different snapshot even if several operations happened in the same tick.
		const size_t none = SIZE_MAX;

		struct Point
		{
			Vector origin;
			size_t parent;
			bool disconnect;
			u32 milestone;
		};

		std::vector<Point> nodes;
		std::unordered_map<i32, size_t> checkpoints;
		size_t tail = none, undo = none;
		u32 milestone = 0;
		bool checkpointTeleport = false, active = false;
		auto append = [&](const Vector &origin, bool disconnect)
		{
			if (tail != none && !disconnect && nodes[tail].origin == origin && nodes[tail].milestone == milestone)
			{
				return tail;
			}
			nodes.push_back({origin, tail, disconnect, milestone});
			return tail = nodes.size() - 1;
		};
		using CheckpointEvent = RpEvent::RpEventData::CheckpointEvent;
		auto applyEvent = [&](const RpEvent &event, size_t index)
		{
			if (index == startEvent || index == endEvent)
			{
				const auto &timer = event.data.timer;
				Vector origin(timer.origin[0], timer.origin[1], timer.origin[2]);
				if (!origin.IsValid())
				{
					return false;
				}
				active = index == startEvent;
				append(origin, false);
				return true;
			}
			if (!active)
			{
				return true;
			}
			if (event.type == RPEVENT_TIMER_EVENT)
			{
				if (event.data.timer.type == TimerEvent::TIMER_SPLIT || event.data.timer.type == TimerEvent::TIMER_CPZ
					|| event.data.timer.type == TimerEvent::TIMER_STAGE)
				{
					// Geometric retry removal must never bypass an ordered course zone.
					milestone++;
				}
				return true;
			}
			if (event.type == RPEVENT_CHECKPOINT)
			{
				const auto &cp = event.data.checkpoint;
				if (cp.type == CheckpointEvent::CHECKPOINT_RESET)
				{
					checkpoints.clear();
					undo = none;
					return true;
				}
				Vector source(cp.origin[0], cp.origin[1], cp.origin[2]);
				Vector destination(cp.destination[0], cp.destination[1], cp.destination[2]);
				if (!source.IsValid() || !destination.IsValid())
				{
					return false;
				}
				size_t before = append(source, false);
				if (cp.type == CheckpointEvent::CHECKPOINT_SAVE)
				{
					checkpoints[cp.index] = before;
					return true;
				}
				size_t target = none;
				if (cp.type == CheckpointEvent::CHECKPOINT_UNDO)
				{
					target = undo;
				}
				else
				{
					auto saved = checkpoints.find(cp.index);
					if (saved != checkpoints.end())
					{
						target = saved->second;
					}
				}
				undo = before;
				if (target != none)
				{
					tail = target;
				}
				append(destination, target == none);
				// The adjacent generic event records the same operation, including
				// an angles-only teleport when the CP origin already matches.
				checkpointTeleport = true;
				return true;
			}
			if (event.type == RPEVENT_TELEPORT)
			{
				if (checkpointTeleport)
				{
					checkpointTeleport = false;
					return true;
				}
				if (event.data.teleport.hasOrigin)
				{
					Vector source(event.data.teleport.previousOrigin[0], event.data.teleport.previousOrigin[1],
								  event.data.teleport.previousOrigin[2]);
					Vector destination(event.data.teleport.origin[0], event.data.teleport.origin[1], event.data.teleport.origin[2]);
					if (!source.IsValid() || !destination.IsValid())
					{
						return false;
					}
					append(source, false);
					append(destination, true);
				}
			}
			return true;
		};
		size_t eventIndex = 0;
		while (eventIndex < replay.events.size() && replay.events[eventIndex].serverTick < start)
		{
			eventIndex++;
		}
		auto applyThrough = [&](u32 tick, RpEventPhase phase)
		{
			while (eventIndex < replay.events.size())
			{
				const auto &event = replay.events[eventIndex];
				if (event.serverTick > tick || (event.serverTick == tick && event.phase > phase))
				{
					break;
				}
				size_t index = eventIndex++;
				if (!applyEvent(event, index))
				{
					return false;
				}
			}
			return true;
		};
		for (const auto &tick : replay.samples)
		{
			if (cancel)
			{
				return false;
			}
			if (tick.serverTick < start)
			{
				continue;
			}
			if (tick.serverTick > end)
			{
				break;
			}
			if (!applyThrough(tick.serverTick, RPEVENT_BEFORE_PHYSICS))
			{
				return false;
			}
			if (active)
			{
				if (!tick.pre.IsValid() || tick.preNoclip)
				{
					return false;
				}
				append(tick.pre, false);
			}
			if (!applyThrough(tick.serverTick, RPEVENT_DURING_PHYSICS))
			{
				return false;
			}
			if (active)
			{
				if (!tick.post.IsValid() || tick.postNoclip)
				{
					return false;
				}
				append(tick.post, false);
			}
			if (!applyThrough(tick.serverTick, RPEVENT_AFTER_PHYSICS))
			{
				return false;
			}
		}
		if (active || tail == none)
		{
			return false;
		}
		std::vector<PathPoint> points;
		for (size_t i = tail; i != none; i = nodes[i].parent)
		{
			points.push_back({nodes[i].origin, nodes[i].disconnect, nodes[i].milestone});
		}
		std::reverse(points.begin(), points.end());
		if (!EraseWalkbacks(points, cancel))
		{
			return false;
		}

		for (size_t i = 1; i < points.size(); i++)
		{
			if (cancel)
			{
				return false;
			}
			if (points[i].disconnect)
			{
				continue;
			}
			Vector delta = points[i].origin - points[i - 1].origin;
			f32 distance = delta.Length();
			if (!std::isfinite(distance))
			{
				return false;
			}
			if (distance <= 0.001f)
			{
				continue;
			}
			// A segment's midpoint is at most 64 units from any point on it.
			// Store it once; the query expands its search by this half-length.
			size_t count = (size_t)ceilf(distance / 128);
			for (size_t part = 0; part < count; part++)
			{
				Vector a = points[i - 1].origin + delta * ((f32)part / count);
				Vector b = points[i - 1].origin + delta * ((f32)(part + 1) / count);
				cells[GetCell((a + b) * 0.5f)].push_back(segments.size());
				f32 segmentLength = (b - a).Length();
				segments.push_back({a, b, length, segmentLength});
				length += segmentLength;
			}
		}
		return length > 0 && std::isfinite(length);
	}

	bool Route::Match(const Vector &position, f64 previousDistance, f32 travel, f64 &distance, bool &approximate) const
	{
		if (!position.IsValid())
		{
			return false;
		}
		// Search geometry, not a window of replay time: a large skip or a fall can
		// immediately match any earlier/later part of the course. 256 units is the
		// maximum geometric error accepted for an estimate, not a progress clamp.
		const f32 radius = 256;
		Cell center = GetCell(position);

		// Two passes avoid per-player allocations and make the result independent
		// of grid iteration order. Dense overlapping trajectories have a work budget:
		// exhausting it hides the estimate, never uses a partially searched result.
		constexpr u32 projectionBudget = 4096;
		u32 projections = 0;
		f32 nearest = radius + 1;
		f64 low = DBL_MAX, high = -1, reachableLow = DBL_MAX, reachableHigh = -1;
		f32 bestError = radius + 1, reachableError = radius + 1;
		f64 closest = 0, reachable = 0;
		f32 window = Clamp(travel * 3 + 64, 128.0f, 1024.0f);
		for (i32 pass = 0; pass < 2; pass++)
		{
			for (i32 x = center.x - 3; x <= center.x + 3; x++)
			{
				for (i32 y = center.y - 3; y <= center.y + 3; y++)
				{
					for (i32 z = center.z - 3; z <= center.z + 3; z++)
					{
						auto cell = cells.find({x, y, z});
						if (cell == cells.end())
						{
							continue;
						}
						for (size_t index : cell->second)
						{
							if (++projections > projectionBudget)
							{
								return false;
							}
							const auto &segment = segments[index];
							Vector delta = segment.end - segment.start;
							f32 fraction = Clamp((position - segment.start).Dot(delta) / delta.LengthSqr(), 0.0f, 1.0f);
							f32 error = (position - segment.start - delta * fraction).Length();
							if (error > radius)
							{
								continue;
							}
							if (pass == 0)
							{
								nearest = Min(nearest, error);
								continue;
							}
							if (error > nearest + 8)
							{
								continue;
							}
							f64 along = segment.distance + fraction * segment.length;
							low = Min(low, along);
							high = Max(high, along);
							if (error < bestError || (error == bestError && along < closest))
							{
								bestError = error;
								closest = along;
							}
							if (previousDistance >= 0 && fabs(along - previousDistance) <= window)
							{
								reachableLow = Min(reachableLow, along);
								reachableHigh = Max(reachableHigh, along);
								if (error < reachableError || (error == reachableError && along < reachable))
								{
									reachableError = error;
									reachable = along;
								}
							}
						}
					}
				}
			}
		}
		if (high < 0)
		{
			return false;
		}
		// Compare the entire range, not each point against whichever was visited
		// first. History is usable only if the remaining range is itself unambiguous.
		if (high - low > 256)
		{
			if (reachableHigh < 0 || reachableHigh - reachableLow > 256)
			{
				return false;
			}
			distance = reachable;
			approximate = reachableError > 64;
		}
		else
		{
			distance = closest;
			approximate = bestError > 64;
		}
		return true;
	}
} // namespace KZ::progress
