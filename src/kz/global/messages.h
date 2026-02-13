#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "api.h"

namespace KZ::api::messages
{
	namespace handshake
	{
		struct Hello
		{
			struct PlayerInfo
			{
				u64 id;
				std::string_view name;

				bool ToJson(Json &json) const;
			};

			std::string_view checksum;
			std::string_view currentMapName;
			std::unordered_map<u64, PlayerInfo> players;

			Hello(std::string_view checksum, std::string_view currentMapName) : checksum(checksum), currentMapName(currentMapName) {}

			void AddPlayer(u64 id, std::string_view name)
			{
				this->players[id] = {id, name};
			}

			bool ToJson(Json &json) const;
		};

		struct HelloAck
		{
			struct ModeInfo
			{
				KZ::api::Mode mode;
				std::string linuxChecksum {};
				std::string windowsChecksum {};

				bool FromJson(const Json &json);
			};

			struct StyleInfo
			{
				std::string style {};
				std::string linuxChecksum {};
				std::string windowsChecksum {};

				bool FromJson(const Json &json);
			};

			struct Announcement
			{
				u64 id;
				std::string title;
				std::string body;
				u64 startsAt;
				u64 expiresAt;

				bool FromJson(const Json &json);
			};

			std::optional<KZ::api::Map> mapInfo {};
			std::vector<ModeInfo> modes {};
			std::vector<StyleInfo> styles {};
			std::vector<Announcement> announcements {};

			bool FromJson(const Json &json);
		};

	}; // namespace handshake

	struct Error
	{
		std::string message;

		bool FromJson(const Json &json);
	};

	struct MapChange
	{
		std::string_view name;

		inline static const char *Name()
		{
			return "map-change";
		}

		bool ToJson(Json &json) const;
	};

	struct PlayerJoin
	{
		std::string_view id;
		std::string_view name;
		std::string_view ipAddress;

		inline static const char *Name()
		{
			return "player-join";
		}

		bool ToJson(Json &json) const;
	};

	struct PlayerJoinAck
	{
		Json preferences;
		bool isBanned;

		bool FromJson(const Json &json);
	};

	struct PlayerLeave
	{
		std::string_view id;
		std::string_view name;
		std::optional<Json> preferences;

		inline static const char *Name()
		{
			return "player-leave";
		}

		bool ToJson(Json &json) const;
	};

	struct WantWorldRecordsForCache
	{
		u16 mapID;

		inline static const char *Name()
		{
			return "want-world-records-for-cache";
		}

		bool ToJson(Json &json) const;
	};

	struct WorldRecordsForCache
	{
		std::vector<KZ::api::Record> records {};

		bool FromJson(const Json &json);
	};

	struct NewRecord
	{
		struct StyleInfo
		{
			std::string_view name;
			std::string_view checksum;

			bool ToJson(Json &json) const;
		};

		u64 playerID;
		u16 filterID;
		std::string_view modeChecksum;
		u32 teleports;
		f64 time;
		std::vector<StyleInfo> styles;
		std::string_view metadata;

		inline static const char *Name()
		{
			return "new-record";
		}

		bool ToJson(Json &json) const;
	};

	struct NewRecordAck
	{
		struct RecordData
		{
			u32 rank {};
			f64 points = -1;
			u64 leaderboardSize {};
		};

		std::string recordId {};
		RecordData overallData {};
		RecordData proData {};

		bool FromJson(const Json &json);
	};

	struct MapDetails
	{
		u16 id;
		std::string name;

		bool FromJson(const Json &json);
	};

	struct CourseDetails
	{
		u16 id;
		std::string name;
		KZ::api::Map::Course::Filter::Tier nubTier;
		KZ::api::Map::Course::Filter::Tier proTier;

		bool FromJson(const Json &json);
	};

	struct WantPersonalBest
	{
		std::string_view player;
		std::string_view map;
		std::string_view course;
		Mode mode;
		std::vector<std::string_view> styles;

		inline static const char *Name()
		{
			return "want-personal-best";
		}

		bool ToJson(Json &json) const;
	};

	struct PersonalBest
	{
		std::optional<Player> player;
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::optional<KZ::api::Record> overall;
		std::optional<KZ::api::Record> pro;

		bool FromJson(const Json &json);
	};

	struct WantCourseTop
	{
		std::string_view map;
		std::string_view course;
		KZ::api::Mode mode;
		u32 limit;
		u32 offset;

		inline static const char *Name()
		{
			return "want-course-top";
		}

		bool ToJson(Json &json) const;
	};

	struct CourseTop
	{
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::vector<KZ::api::Record> overall;
		std::vector<KZ::api::Record> pro;

		bool FromJson(const Json &json);
	};

	struct WantWorldRecords
	{
		std::string_view map;
		std::string_view course;
		Mode mode;

		inline static const char *Name()
		{
			return "want-world-records";
		}

		bool ToJson(Json &json) const;
	};

	struct WorldRecords
	{
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::optional<KZ::api::Record> overall;
		std::optional<KZ::api::Record> pro;

		bool FromJson(const Json &json);
	};

	struct WantPlayerRecords
	{
		u16 mapID;
		u64 playerID;

		inline static const char *Name()
		{
			return "want-player-records";
		}

		bool ToJson(Json &json) const;
	};

	struct PlayerRecords
	{
		std::vector<Record> records {};

		bool FromJson(const Json &json);
	};
}; // namespace KZ::api::messages
