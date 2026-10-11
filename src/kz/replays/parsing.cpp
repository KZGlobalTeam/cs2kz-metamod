#include "parsing.h"
#include <limits>

namespace KZ::replaysystem::parsing
{
	// This is the original v1-v5 ABI, including its padding. New fields are
	// deliberately absent; legacy playback must not read native v6 RpEvent bytes.
	struct LegacyEvent
	{
		RpEventType type;
		u32 serverTick;

		union
		{
			struct
			{
				RpEvent::RpEventData::TimerEvent::TimerEventType type;
				i32 index;
				f32 time;
			} timer;

			RpModeStyleInfo mode;
			RpStyleChangeInfo style;

			struct
			{
				bool hasOrigin, hasAngles, hasVelocity;
				f32 origin[3], angles[3], velocity[3];
			} teleport;
		} data;
	};

	static_function bool ReadLegacyEvents(const char *bytes, size_t size, u32 count, std::vector<RpEvent> &events)
	{
		if ((u64)count * sizeof(LegacyEvent) != size)
		{
			return false;
		}
		events.clear();
		events.reserve(count);
		for (u32 i = 0; i < count; ++i)
		{
			LegacyEvent legacy;
			memcpy(&legacy, bytes + i * sizeof(legacy), sizeof(legacy));
			RpEvent event {};
			event.type = legacy.type;
			event.serverTick = legacy.serverTick;
			switch (legacy.type)
			{
				case RPEVENT_TIMER_EVENT:
					event.data.timer.type = legacy.data.timer.type;
					event.data.timer.index = legacy.data.timer.index;
					event.data.timer.time = legacy.data.timer.time;
					break;
				case RPEVENT_MODE_CHANGE:
					event.data.modeChange = legacy.data.mode;
					break;
				case RPEVENT_STYLE_CHANGE:
					event.data.styleChange = legacy.data.style;
					break;
				case RPEVENT_TELEPORT:
					event.data.teleport.reason = RPTELEPORT_UNKNOWN;
					event.data.teleport.hasOrigin = legacy.data.teleport.hasOrigin;
					event.data.teleport.hasAngles = legacy.data.teleport.hasAngles;
					event.data.teleport.hasVelocity = legacy.data.teleport.hasVelocity;
					memcpy(event.data.teleport.origin, legacy.data.teleport.origin, sizeof(legacy.data.teleport.origin));
					memcpy(event.data.teleport.angles, legacy.data.teleport.angles, sizeof(legacy.data.teleport.angles));
					memcpy(event.data.teleport.velocity, legacy.data.teleport.velocity, sizeof(legacy.data.teleport.velocity));
					break;
				default:
					return false;
			}
			events.push_back(event);
		}
		return true;
	}

	static_function bool ReadVector(const cs2kz::replay::EventVector &source, f32 (&destination)[3])
	{
		if (!source.has_x() || !source.has_y() || !source.has_z())
		{
			return false;
		}
		destination[0] = source.x();
		destination[1] = source.y();
		destination[2] = source.z();
		return true;
	}

	static_function void WriteVector(const f32 (&source)[3], cs2kz::replay::EventVector *destination)
	{
		destination->set_x(source[0]);
		destination->set_y(source[1]);
		destination->set_z(source[2]);
	}

	static_function void ReadMode(const cs2kz::replay::ModeStyleInfo &source, RpModeStyleInfo &destination)
	{
		V_strncpy(destination.name, source.name().c_str(), sizeof(destination.name));
		V_strncpy(destination.shortName, source.short_name().c_str(), sizeof(destination.shortName));
		V_strncpy(destination.md5, source.md5().c_str(), sizeof(destination.md5));
	}

	static_function void WriteMode(const RpModeStyleInfo &source, cs2kz::replay::ModeStyleInfo *destination)
	{
		destination->set_name(source.name);
		destination->set_short_name(source.shortName);
		destination->set_md5(source.md5);
	}

	static_function bool ReadEventV6(const cs2kz::replay::ReplayEvent &source, RpEvent &event)
	{
		event = {};
		event.type = RPEVENT_UNKNOWN;
		event.serverTick = source.server_tick();
		event.phase =
			source.has_phase() && source.phase() <= RPEVENT_AFTER_PHYSICS ? static_cast<RpEventPhase>(source.phase()) : RPEVENT_PHASE_UNKNOWN;
		if (!source.has_type() || source.type() >= RPEVENT_COUNT || !source.has_server_tick())
		{
			return false;
		}
		event.type = static_cast<RpEventType>(source.type());
		switch (event.type)
		{
			case RPEVENT_TIMER_EVENT:
				if (!source.has_timer() || !source.timer().has_type() || !source.timer().has_index() || !source.timer().has_time()
					|| !source.timer().has_origin() || source.timer().type() > RpEvent::RpEventData::TimerEvent::TIMER_STAGE
					|| !ReadVector(source.timer().origin(), event.data.timer.origin))
				{
					return false;
				}
				event.data.timer.type = static_cast<RpEvent::RpEventData::TimerEvent::TimerEventType>(source.timer().type());
				event.data.timer.index = source.timer().index();
				event.data.timer.time = source.timer().time();
				return true;
			case RPEVENT_MODE_CHANGE:
				if (!source.has_mode() || !source.mode().has_name())
				{
					return false;
				}
				ReadMode(source.mode(), event.data.modeChange);
				return true;
			case RPEVENT_STYLE_CHANGE:
				if (!source.has_style() || (!source.style().clear_styles() && (!source.style().has_style() || !source.style().style().has_name())))
				{
					return false;
				}
				ReadMode(source.style().style(), event.data.styleChange);
				event.data.styleChange.clearStyles = source.style().clear_styles();
				return true;
			case RPEVENT_TELEPORT:
				if (!source.has_teleport() || source.teleport().checkpoint_index() < 0 || !source.teleport().has_previous_origin()
					|| !ReadVector(source.teleport().previous_origin(), event.data.teleport.previousOrigin))
				{
					return false;
				}
				event.data.teleport.reason = static_cast<RpTeleportReason>(source.teleport().reason());
				event.data.teleport.checkpointIndex = source.teleport().checkpoint_index();
				event.data.teleport.hasOrigin = source.teleport().has_origin();
				event.data.teleport.hasAngles = source.teleport().has_angles();
				event.data.teleport.hasVelocity = source.teleport().has_velocity();
				if (source.teleport().has_origin() && !ReadVector(source.teleport().origin(), event.data.teleport.origin))
				{
					return false;
				}
				if (source.teleport().has_angles() && !ReadVector(source.teleport().angles(), event.data.teleport.angles))
				{
					return false;
				}
				if (source.teleport().has_velocity() && !ReadVector(source.teleport().velocity(), event.data.teleport.velocity))
				{
					return false;
				}
				return true;
			case RPEVENT_CHECKPOINT:
				if (!source.has_checkpoint() || !source.checkpoint().has_type() || !source.checkpoint().has_index()
					|| source.checkpoint().index() < 0)
				{
					return false;
				}
				if (source.checkpoint().type() != RpEvent::RpEventData::CheckpointEvent::CHECKPOINT_SAVE
					&& source.checkpoint().type() != RpEvent::RpEventData::CheckpointEvent::CHECKPOINT_RESET)
				{
					return false;
				}
				if (source.checkpoint().type() == RpEvent::RpEventData::CheckpointEvent::CHECKPOINT_SAVE
					&& (!source.checkpoint().has_origin() || source.checkpoint().index() == 0
						|| !ReadVector(source.checkpoint().origin(), event.data.checkpoint.origin)))
				{
					return false;
				}
				event.data.checkpoint.type = static_cast<RpEvent::RpEventData::CheckpointEvent::CheckpointEventType>(source.checkpoint().type());
				event.data.checkpoint.index = source.checkpoint().index();
				return true;
			default:
				return false;
		}
	}

	static_function bool ReadEventsV6(const char *bytes, size_t size, u32 count, std::vector<RpEvent> &events)
	{
		events.clear();
		const char *cursor = bytes;
		const char *end = bytes + size;
		for (u32 i = 0; i < count; ++i)
		{
			u32 length;
			if ((size_t)(end - cursor) < sizeof(length))
			{
				return false;
			}
			memcpy(&length, cursor, sizeof(length));
			cursor += sizeof(length);
			if (length > (size_t)(end - cursor) || length > (u32)(std::numeric_limits<int>::max)())
			{
				return false;
			}
			cs2kz::replay::ReplayEvent encoded;
			if (!encoded.ParseFromArray(cursor, (int)length))
			{
				return false;
			}
			cursor += length;
			RpEvent event;
			if (!ReadEventV6(encoded, event))
			{
				// Unknown or incomplete events retain their position in the event stream.
				// Playback ignores the placeholder; consumers requiring its data can decline it.
				event.type = RPEVENT_UNKNOWN;
			}
			if (!encoded.has_server_tick())
			{
				// There is no action to schedule for this placeholder.
				event.serverTick = events.empty() ? 0 : events.back().serverTick;
			}
			if (!events.empty() && event.serverTick < events.back().serverTick)
			{
				return false;
			}
			events.push_back(event);
		}
		return cursor == end;
	}

	// Each version has one entry point. Shared layouts reuse shared decoders;
	// adding a new version does not add version checks to field-reading loops.
	static_function VersionParser ParserV1()
	{
		VersionParser parser {};
		parser.weaponFlag = 1ULL << 39;
		parser.readEvents = ReadLegacyEvents;
		return parser;
	}

	static_function VersionParser ParserV2()
	{
		VersionParser parser = ParserV1();
		parser.modernActualFlag = 1ULL << 39;
		parser.modernUsableFlag = 1ULL << 40;
		parser.modernLandedFlag = 1ULL << 41;
		parser.modernJump = true;
		return parser;
	}

	static_function VersionParser ParserV3()
	{
		VersionParser parser = ParserV2();
		parser.modernActualFlag = 1ULL << 40;
		parser.modernUsableFlag = 1ULL << 41;
		parser.modernLandedFlag = 1ULL << 42;
		return parser;
	}

	static_function VersionParser ParserV4()
	{
		VersionParser parser = ParserV3();
		parser.compactSubticks = true;
		return parser;
	}

	static_function VersionParser ParserV5()
	{
		VersionParser parser = ParserV4();
		parser.jumpFractions = true;
		return parser;
	}

	static_function VersionParser ParserV6()
	{
		VersionParser parser = ParserV5();
		parser.readEvents = ReadEventsV6;
		return parser;
	}

	const VersionParser *GetVersionParser(u32 version)
	{
		static const VersionParser versions[] = {ParserV1(), ParserV2(), ParserV3(), ParserV4(), ParserV5(), ParserV6()};
		return version > 0 && version <= sizeof(versions) / sizeof(versions[0]) ? &versions[version - 1] : nullptr;
	}

	bool EncodeEventsV6(const std::vector<RpEvent> &events, std::vector<char> &bytes)
	{
		bytes.clear();
		for (const RpEvent &event : events)
		{
			cs2kz::replay::ReplayEvent encoded;
			encoded.set_type(event.type);
			encoded.set_server_tick(event.serverTick);
			encoded.set_phase(event.phase);
			switch (event.type)
			{
				case RPEVENT_TIMER_EVENT:
					encoded.mutable_timer()->set_type(event.data.timer.type);
					encoded.mutable_timer()->set_index(event.data.timer.index);
					encoded.mutable_timer()->set_time(event.data.timer.time);
					WriteVector(event.data.timer.origin, encoded.mutable_timer()->mutable_origin());
					break;
				case RPEVENT_MODE_CHANGE:
					WriteMode(event.data.modeChange, encoded.mutable_mode());
					break;
				case RPEVENT_STYLE_CHANGE:
					WriteMode(event.data.styleChange, encoded.mutable_style()->mutable_style());
					encoded.mutable_style()->set_clear_styles(event.data.styleChange.clearStyles);
					break;
				case RPEVENT_TELEPORT:
					encoded.mutable_teleport()->set_reason(static_cast<cs2kz::replay::TeleportReason>(event.data.teleport.reason));
					encoded.mutable_teleport()->set_checkpoint_index(event.data.teleport.checkpointIndex);
					WriteVector(event.data.teleport.previousOrigin, encoded.mutable_teleport()->mutable_previous_origin());
					if (event.data.teleport.hasOrigin)
					{
						WriteVector(event.data.teleport.origin, encoded.mutable_teleport()->mutable_origin());
					}
					if (event.data.teleport.hasAngles)
					{
						WriteVector(event.data.teleport.angles, encoded.mutable_teleport()->mutable_angles());
					}
					if (event.data.teleport.hasVelocity)
					{
						WriteVector(event.data.teleport.velocity, encoded.mutable_teleport()->mutable_velocity());
					}
					break;
				case RPEVENT_CHECKPOINT:
					encoded.mutable_checkpoint()->set_type(event.data.checkpoint.type);
					encoded.mutable_checkpoint()->set_index(event.data.checkpoint.index);
					if (event.data.checkpoint.type == RpEvent::RpEventData::CheckpointEvent::CHECKPOINT_SAVE)
					{
						WriteVector(event.data.checkpoint.origin, encoded.mutable_checkpoint()->mutable_origin());
					}
					break;
				default:
					return false;
			}
			std::string serialized;
			if (!encoded.SerializeToString(&serialized) || serialized.size() > (std::numeric_limits<u32>::max)())
			{
				return false;
			}
			u32 length = (u32)serialized.size();
			const char *prefix = reinterpret_cast<const char *>(&length);
			bytes.insert(bytes.end(), prefix, prefix + sizeof(length));
			bytes.insert(bytes.end(), serialized.begin(), serialized.end());
		}
		return true;
	}
} // namespace KZ::replaysystem::parsing
