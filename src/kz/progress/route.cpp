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

	bool Route::Build(const replaysystem::data::ReplayMovement &replay, i32 courseID, const std::atomic<bool> &cancel)
	{
		using TimerEvent = RpEvent::RpEventData::TimerEvent;
		u32 start = 0, end = 0, pendingStart = 0;
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
				pendingStart = event.data.timer.index == courseID ? event.serverTick : 0;
			}
			else if (event.data.timer.type == TimerEvent::TIMER_STOP)
			{
				pendingStart = 0;
			}
			else if (event.data.timer.type == TimerEvent::TIMER_END)
			{
				if (pendingStart && event.data.timer.index == courseID && fabsf(event.data.timer.time - replay.header.run().time()) < 0.001f)
				{
					start = pendingStart;
					end = event.serverTick;
				}
				pendingStart = 0;
			}
		}
		if (!start || end <= start)
		{
			return false;
		}

		// Checkpoint indices identify a particular saved CP, not a visit to a nearby
		// coordinate. Keep a parent chain so CP/undo/next-CP can restore an earlier
		// branch without copying or searching the entire trajectory on every return.
		const size_t none = SIZE_MAX;

		struct Point
		{
			Vector origin;
			size_t parent;
			bool disconnect;
		};

		struct Anchor
		{
			size_t pre, post;
		};

		std::vector<Point> nodes;
		std::unordered_map<i32, Anchor> checkpoints;
		Anchor undo {none, none};
		size_t tail = none;
		auto append = [&](const Vector &origin, bool disconnect)
		{
			if (tail != none && !disconnect && nodes[tail].origin == origin)
			{
				return tail;
			}
			nodes.push_back({origin, tail, disconnect});
			return tail = nodes.size() - 1;
		};
		auto resolve = [&](Anchor anchor, const Vector &origin)
		{
			// The replay has no CP-position/undo event. Accept an exact recorded
			// endpoint only. Never guess a CP identity from a radius search.
			if (anchor.pre != none && nodes[anchor.pre].origin == origin)
			{
				return anchor.pre;
			}
			if (anchor.post != none && nodes[anchor.post].origin == origin)
			{
				return anchor.post;
			}
			return none;
		};
		size_t eventIndex = 0;
		i32 checkpointCount = 0, teleports = 0;
		bool sampled = false;
		for (const auto &tick : replay.samples)
		{
			if (cancel)
			{
				return false;
			}
			if (tick.serverTick < start)
			{
				checkpointCount = tick.checkpointCount;
				teleports = tick.teleportCount;
				continue;
			}
			if (tick.serverTick > end)
			{
				break;
			}
			if (!tick.pre.IsValid() || !tick.post.IsValid() || tick.noclip)
			{
				return false;
			}
			i32 originEvents = 0;
			Vector destination = tick.post;
			while (eventIndex < replay.events.size() && replay.events[eventIndex].serverTick <= tick.serverTick)
			{
				const auto &event = replay.events[eventIndex++];
				if (event.serverTick >= start && event.type == RPEVENT_TELEPORT && event.data.teleport.hasOrigin)
				{
					originEvents++;
					destination = Vector(event.data.teleport.origin[0], event.data.teleport.origin[1], event.data.teleport.origin[2]);
				}
			}
			if (!destination.IsValid())
			{
				return false;
			}
			if (tick.checkpointCount < checkpointCount || tick.teleportCount < teleports)
			{
				checkpoints.clear();
				undo = {none, none};
			}
			size_t before = tail;
			size_t pre = originEvents ? tail : append(tick.pre, false);
			if (originEvents)
			{
				size_t target = none;
				if (sampled && tick.teleportCount == teleports + 1 && originEvents == 1)
				{
					auto cp = checkpoints.find(tick.checkpointIndex);
					size_t checkpoint = cp == checkpoints.end() ? none : resolve(cp->second, destination);
					size_t undone = resolve(undo, destination);
					// Two different visits at the same coordinate cannot identify which
					// operation happened. Preserve both paths instead of deleting one.
					if (checkpoint == none || undone == none || checkpoint == undone)
					{
						target = checkpoint != none ? checkpoint : undone;
					}
					undo = {before, pre};
				}
				else
				{
					undo = {none, none};
				}
				if (target != none)
				{
					tail = target;
				}
				append(destination, target == none);
			}
			size_t post = append(tick.post, false);
			if (sampled && !originEvents && tick.teleportCount == teleports && tick.checkpointCount == checkpointCount + 1
				&& tick.checkpointIndex == tick.checkpointCount)
			{
				checkpoints[tick.checkpointIndex] = {pre, post};
			}
			checkpointCount = tick.checkpointCount;
			teleports = tick.teleportCount;
			sampled = true;
		}
		std::vector<Point> points;
		for (size_t i = tail; i != none; i = nodes[i].parent)
		{
			points.push_back(nodes[i]);
		}
		std::reverse(points.begin(), points.end());

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
