#include "replay_route.h"
#include "kz/replays/compression.h"
#include <fstream>
#include <algorithm>
#include <cmath>

namespace KZ::progress
{
	using KZ::replaysystem::compression::CompressedSectionHeader;

	// Preflight the existing decoder's lengths before allocating or passing input to it.
	// Only modern v4/v5 files have the current subtick layout.
	static bool Section(const char *&cursor, const char *end, CompressedSectionHeader &h)
	{
		if ((size_t)(end - cursor) < sizeof(h))
		{
			return false;
		}
		memcpy(&h, cursor, sizeof(h));
		cursor += sizeof(h);
		if (h.compressedSize > (size_t)(end - cursor) || h.uncompressedSize > 256u * 1024u * 1024u)
		{
			return false;
		}
		cursor += h.compressedSize;
		return true;
	}

	std::shared_ptr<Route> ReadReferenceReplay(const ReplayCandidate &c, const std::atomic<bool> &cancel)
	{
		std::ifstream file(c.path, std::ios::binary | std::ios::ate);
		if (!file || cancel)
		{
			return {};
		}
		auto size = file.tellg();
		if (size < 4 || size > 256 * 1024 * 1024)
		{
			return {};
		}
		std::vector<char> bytes((size_t)size);
		file.seekg(0);
		if (!file.read(bytes.data(), (std::streamsize)bytes.size()))
		{
			return {};
		}
		const char *cursor = bytes.data(), *end = cursor + bytes.size();
		u32 headerSize;
		memcpy(&headerSize, cursor, sizeof(headerSize));
		cursor += sizeof(headerSize);
		if (!headerSize || headerSize > 5 * 1024 * 1024 || headerSize > (size_t)(end - cursor))
		{
			return {};
		}
		ReplayHeader header;
		if (!header.ParseFromArray(cursor, (i32)headerSize))
		{
			return {};
		}
		cursor += headerSize;
		if (header.version() < 4 || header.version() > KZ_REPLAY_VERSION || !header.has_run() || header.type() != cs2kz::replay::RP_RUN
			|| header.map().name() != c.mapName || header.map().md5() != c.mapMD5 || c.mapMD5.empty() || header.run().course_name() != c.courseName
			|| header.run().mode().name() != c.mode || header.run().num_teleports() != 0 || header.run().styles_size() != 0
			|| (header.run().has_timer_valid() && !header.run().timer_valid()) || !std::isfinite(header.run().time()) || header.run().time() <= 0)
		{
			return {};
		}

		const char *tickStart = cursor;
		CompressedSectionHeader ticksHeader, subticksHeader, weaponsHeader, jumpsHeader, eventsHeader;
		if (!Section(cursor, end, ticksHeader) || !Section(cursor, end, subticksHeader) || ticksHeader.elementCount < 2
			|| ticksHeader.elementCount > 250000 || subticksHeader.elementCount != ticksHeader.elementCount
			|| subticksHeader.uncompressedSize != (u64)subticksHeader.elementCount * sizeof(SubtickData) || !Section(cursor, end, weaponsHeader)
			|| !Section(cursor, end, jumpsHeader))
		{
			return {};
		}
		const char *eventsStart = cursor;
		if (!Section(cursor, end, eventsHeader) || eventsHeader.elementCount > 500000
			|| eventsHeader.uncompressedSize != (u64)eventsHeader.elementCount * sizeof(RpEvent) || cancel)
		{
			return {};
		}

		std::vector<TickData> ticks;
		std::vector<SubtickData> subticks;
		cursor = tickStart;
		if (!KZ::replaysystem::compression::ReadTickDataCompressed(cursor, end, ticks, subticks, header.version()) || cancel)
		{
			return {};
		}
		// Progress doesn't retain inputs, skins, jumpstats or command frames.
		std::vector<SubtickData>().swap(subticks);
		std::vector<RpEvent> events;
		cursor = eventsStart;
		if (!KZ::replaysystem::compression::ReadEventsCompressed(cursor, end, events))
		{
			return {};
		}
		using TimerEvent = RpEvent::RpEventData::TimerEvent;
		u32 start = 0, finish = 0, activeStart = 0, previousEventTick = 0;
		bool active = false, found = false;
		for (const auto &e : events)
		{
			if (e.serverTick < previousEventTick)
			{
				return {};
			}
			previousEventTick = e.serverTick;
			if (e.type == RPEVENT_MODE_CHANGE || e.type == RPEVENT_STYLE_CHANGE)
			{
				active = false;
				continue;
			}
			if (e.type != RPEVENT_TIMER_EVENT)
			{
				continue;
			}
			if (e.data.timer.type == TimerEvent::TIMER_START)
			{
				activeStart = e.serverTick;
				active = e.data.timer.index == c.courseID;
			}
			else if (e.data.timer.type == TimerEvent::TIMER_STOP)
			{
				active = false;
			}
			else if (e.data.timer.type == TimerEvent::TIMER_END)
			{
				if (active && e.data.timer.index == c.courseID && std::isfinite(e.data.timer.time)
					&& std::abs(e.data.timer.time - header.run().time()) <= 0.001f)
				{
					if (found)
					{
						return {}; // More than one matching run is ambiguous.
					}
					start = activeStart;
					finish = e.serverTick;
					found = true;
				}
				active = false;
			}
		}
		// A RunRecorder includes five seconds of earlier events and a post-run breather.
		// Extract the matching completed interval, not the first TIMER_START in the file.
		if (!found || start >= finish || ticks.front().serverTick > start || ticks.back().serverTick < finish)
		{
			return {};
		}
		std::vector<Point> points;
		points.reserve(std::min<size_t>(ticks.size(), 500000));
		size_t eventIndex = 0;
		u32 previousTick = 0;
		auto add = [&](const Vector &origin, bool discontinuity)
		{
			if (!ValidPosition(origin))
			{
				return false;
			}
			if (!points.empty() && !discontinuity && (points.back().origin - origin).LengthSqr() < 0.01f)
			{
				return true;
			}
			// A large unreported displacement is treated as a break, never route length.
			if (!points.empty() && (points.back().origin - origin).Length() > 1024)
			{
				discontinuity = true;
			}
			if (points.size() >= 500000)
			{
				return false;
			}
			points.push_back({origin, discontinuity});
			return true;
		};
		for (const auto &tick : ticks)
		{
			if (cancel || (previousTick && tick.serverTick <= previousTick))
			{
				return {};
			}
			previousTick = tick.serverTick;
			if (tick.serverTick < start || tick.serverTick > finish)
			{
				continue;
			}
			if (tick.pre.moveType == MOVETYPE_NOCLIP || tick.post.moveType == MOVETYPE_NOCLIP || tick.checkpoint.teleportCount != 0)
			{
				return {};
			}
			while (eventIndex < events.size() && events[eventIndex].serverTick < tick.serverTick)
			{
				const auto &e = events[eventIndex++];
				if (e.serverTick >= start && e.type == RPEVENT_TELEPORT && e.data.teleport.hasOrigin)
				{
					if (!add(Vector(e.data.teleport.origin[0], e.data.teleport.origin[1], e.data.teleport.origin[2]), true))
					{
						return {};
					}
				}
			}
			if (!add(tick.pre.origin, false))
			{
				return {};
			}
			while (eventIndex < events.size() && events[eventIndex].serverTick <= tick.serverTick)
			{
				const auto &e = events[eventIndex++];
				if (e.serverTick < start || e.type != RPEVENT_TELEPORT || !e.data.teleport.hasOrigin)
				{
					continue;
				}
				if (!add(Vector(e.data.teleport.origin[0], e.data.teleport.origin[1], e.data.teleport.origin[2]), true))
				{
					return {};
				}
			}
			if (!add(tick.post.origin, false))
			{
				return {};
			}
		}
		auto route = std::make_shared<Route>();
		route->courseGUID = c.courseGUID;
		route->courseID = c.courseID;
		route->courseName = c.courseName;
		route->mode = c.mode;
		route->source = c.uuid;
		return route->Build(std::move(points)) ? route : nullptr;
	}
} // namespace KZ::progress
