#include "parsing.h"
#include <limits>

namespace KZ::replaysystem::parsing
{
	namespace
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

		bool ReadLegacyEvents(const char *bytes, size_t size, u32 count, std::vector<RpEvent> &events)
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

		void ReadVector(const cs2kz::replay::EventVector &source, f32 (&destination)[3])
		{
			destination[0] = source.x();
			destination[1] = source.y();
			destination[2] = source.z();
		}

		void WriteVector(const f32 (&source)[3], cs2kz::replay::EventVector *destination)
		{
			destination->set_x(source[0]);
			destination->set_y(source[1]);
			destination->set_z(source[2]);
		}

		void ReadMode(const cs2kz::replay::ModeStyleInfo &source, RpModeStyleInfo &destination)
		{
			V_strncpy(destination.name, source.name().c_str(), sizeof(destination.name));
			V_strncpy(destination.shortName, source.short_name().c_str(), sizeof(destination.shortName));
			V_strncpy(destination.md5, source.md5().c_str(), sizeof(destination.md5));
		}

		void WriteMode(const RpModeStyleInfo &source, cs2kz::replay::ModeStyleInfo *destination)
		{
			destination->set_name(source.name);
			destination->set_short_name(source.shortName);
			destination->set_md5(source.md5);
		}

		bool ReadEventV6(const cs2kz::replay::ReplayEvent &source, RpEvent &event)
		{
			event = {};
			event.type = static_cast<RpEventType>(source.type());
			event.serverTick = source.server_tick();
			event.phase = static_cast<RpEventPhase>(source.phase());
			if (source.phase() > RPEVENT_AFTER_PHYSICS)
			{
				return false;
			}
			switch (event.type)
			{
				case RPEVENT_TIMER_EVENT:
					if (!source.has_timer() || source.timer().type() > RpEvent::RpEventData::TimerEvent::TIMER_STAGE)
					{
						return false;
					}
					event.data.timer.type = static_cast<RpEvent::RpEventData::TimerEvent::TimerEventType>(source.timer().type());
					event.data.timer.index = source.timer().index();
					event.data.timer.time = source.timer().time();
					ReadVector(source.timer().origin(), event.data.timer.origin);
					return true;
				case RPEVENT_MODE_CHANGE:
					if (!source.has_mode())
					{
						return false;
					}
					ReadMode(source.mode(), event.data.modeChange);
					return true;
				case RPEVENT_STYLE_CHANGE:
					if (!source.has_style())
					{
						return false;
					}
					ReadMode(source.style().style(), event.data.styleChange);
					event.data.styleChange.clearStyles = source.style().clear_styles();
					return true;
				case RPEVENT_TELEPORT:
					if (!source.has_teleport() || source.teleport().checkpoint_index() < 0)
					{
						return false;
					}
					event.data.teleport.reason = static_cast<RpTeleportReason>(source.teleport().reason());
					event.data.teleport.checkpointIndex = source.teleport().checkpoint_index();
					event.data.teleport.hasOrigin = source.teleport().has_origin();
					event.data.teleport.hasAngles = source.teleport().has_angles();
					event.data.teleport.hasVelocity = source.teleport().has_velocity();
					ReadVector(source.teleport().previous_origin(), event.data.teleport.previousOrigin);
					if (source.teleport().has_origin())
					{
						ReadVector(source.teleport().origin(), event.data.teleport.origin);
					}
					if (source.teleport().has_angles())
					{
						ReadVector(source.teleport().angles(), event.data.teleport.angles);
					}
					if (source.teleport().has_velocity())
					{
						ReadVector(source.teleport().velocity(), event.data.teleport.velocity);
					}
					return true;
				case RPEVENT_CHECKPOINT:
					if (!source.has_checkpoint() || source.checkpoint().index() < 0)
					{
						return false;
					}
					if (source.checkpoint().type() != RpEvent::RpEventData::CheckpointEvent::CHECKPOINT_SAVE
						&& source.checkpoint().type() != RpEvent::RpEventData::CheckpointEvent::CHECKPOINT_RESET)
					{
						return false;
					}
					if (source.checkpoint().type() == RpEvent::RpEventData::CheckpointEvent::CHECKPOINT_SAVE
						&& (!source.checkpoint().has_origin() || source.checkpoint().index() == 0))
					{
						return false;
					}
					event.data.checkpoint.type = static_cast<RpEvent::RpEventData::CheckpointEvent::CheckpointEventType>(source.checkpoint().type());
					event.data.checkpoint.index = source.checkpoint().index();
					if (source.checkpoint().has_origin())
					{
						ReadVector(source.checkpoint().origin(), event.data.checkpoint.origin);
					}
					return true;
				default:
					return false;
			}
		}

		bool ReadEventsV6(const char *bytes, size_t size, u32 count, std::vector<RpEvent> &events)
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
				// A future event has its own length, so existing features can ignore it.
				RpEvent event;
				if (encoded.type() > RPEVENT_CHECKPOINT)
				{
					if (encoded.phase() > RPEVENT_AFTER_PHYSICS)
					{
						return false;
					}
					event = {};
					event.type = RPEVENT_UNKNOWN;
					event.serverTick = encoded.server_tick();
					event.phase = static_cast<RpEventPhase>(encoded.phase());
				}
				else if (!ReadEventV6(encoded, event))
				{
					return false;
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
		VersionParser ParserV1()
		{
			return {1ULL << 39, 0, 0, 0, false, false, false, ReadLegacyEvents};
		}

		VersionParser ParserV2()
		{
			return {1ULL << 39, 1ULL << 39, 1ULL << 40, 1ULL << 41, true, false, false, ReadLegacyEvents};
		}

		VersionParser ParserV3()
		{
			return {1ULL << 39, 1ULL << 40, 1ULL << 41, 1ULL << 42, true, false, false, ReadLegacyEvents};
		}

		VersionParser ParserV4()
		{
			return {1ULL << 39, 1ULL << 40, 1ULL << 41, 1ULL << 42, true, true, false, ReadLegacyEvents};
		}

		VersionParser ParserV5()
		{
			return {1ULL << 39, 1ULL << 40, 1ULL << 41, 1ULL << 42, true, true, true, ReadLegacyEvents};
		}

		VersionParser ParserV6()
		{
			return {1ULL << 39, 1ULL << 40, 1ULL << 41, 1ULL << 42, true, true, true, ReadEventsV6};
		}
	} // namespace

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
