#pragma once

#include <string_view>

#include "utils/json.h"
#include "kz/global/api.h"

namespace KZ::API::events
{
	struct MapChange
	{
		std::string_view mapName;

		MapChange(std::string_view mapName) : mapName(mapName) {}

		bool ToJson(Json &json) const;
	};

	struct MapInfo
	{
		std::optional<KZ::API::Map> data;

		bool FromJson(const Json &json);
	};

	struct PlayerJoin
	{
		u64 steamID;
		std::string name;
		std::string ipAddress;

		bool ToJson(Json &json) const;
	};

	struct PlayerJoinAck
	{
		bool isBanned;
		Json preferences;

		bool FromJson(const Json &json);
	};

	struct PlayerLeave
	{
		u64 steamID;
		std::string name;
		Json preferences;

		bool ToJson(Json &json) const;
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

		u32 recordId {};
		f64 playerRating {};
		RecordData overallData {};
		RecordData proData {};

		bool FromJson(const Json &json);
	};

	struct WantWorldRecordsForCache
	{
		u16 mapID;

		bool ToJson(Json &json) const;
	};

	struct WorldRecordsForCache
	{
		std::vector<KZ::API::Record> records {};

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
		KZ::API::Map::Course::Filter::Tier nubTier;
		KZ::API::Map::Course::Filter::Tier proTier;

		bool FromJson(const Json &json);
	};

	struct WantCourseTop
	{
		std::string_view mapName;
		std::string_view courseNameOrNumber;
		Mode mode;
		u32 limit;
		u32 offset;

		bool ToJson(Json &json) const;
	};

	struct CourseTop
	{
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::vector<Record> overall;
		std::vector<Record> pro;

		bool FromJson(const Json &json);
	};

	struct WantWorldRecords
	{
		std::string_view mapName;
		std::string_view courseNameOrNumber;
		Mode mode;

		bool ToJson(Json &json) const;
	};

	struct WorldRecords
	{
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::optional<Record> overall;
		std::optional<Record> pro;

		bool FromJson(const Json &json);
	};

	struct WantPersonalBest
	{
		u64 steamid64;
		std::string_view playerName;
		std::string_view mapName;
		std::string_view courseNameOrNumber;
		Mode mode;
		std::vector<std::string> styles;

		bool ToJson(Json &json) const;
	};

	struct PersonalBest
	{
		std::optional<Player> player;
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::optional<Record> overall;
		std::optional<Record> pro;

		bool FromJson(const Json &json);
	};

	struct WantPlayerRecords
	{
		u16 mapID;
		u64 playerID;

		bool ToJson(Json &json) const;
	};

	struct PlayerRecords
	{
		std::vector<Record> records {};

		bool FromJson(const Json &json);
	};
} // namespace KZ::API::events
