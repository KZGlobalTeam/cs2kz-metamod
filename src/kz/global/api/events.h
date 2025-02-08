#pragma once

#include "common.h"

#include "kz/global/api/maps.h"
#include "kz/global/api/records.h"

namespace KZ::API::events
{
	struct PlayerJoin
	{
		u64 steamId;
		std::string name;
		std::string ipAddress;

		bool ToJson(Json &json) const
		{
			json.Set("id", this->steamId);
			json.Set("name", this->name);
			json.Set("ip_address", this->ipAddress);

			return true;
		}
	};

	struct PlayerJoinAck
	{
		bool isBanned;
		Json preferences;

		bool FromJson(const Json &json)
		{
			if (!json.Get("is_banned", this->isBanned))
			{
				return false;
			}

			if (!json.Get("preferences", this->preferences))
			{
				return false;
			}

			return true;
		}
	};

	struct PlayerLeave
	{
		u64 steamId;
		std::string name;
		Json preferences;

		bool ToJson(Json &json) const
		{
			json.Set("id", this->steamId);
			json.Set("name", this->name);
			json.Set("preferences", this->preferences);

			return true;
		}
	};

	struct MapChange
	{
		std::string newMap;

		MapChange(const char *mapName) : newMap(mapName) {}

		bool ToJson(Json &json) const
		{
			json.Set("new_map", this->newMap);

			return true;
		}
	};

	struct MapInfo
	{
		std::optional<KZ::API::Map> map;

		bool FromJson(const Json &json)
		{
			if (!json.Get("map", this->map))
			{
				return false;
			}

			return true;
		}
	};

	struct NewRecord
	{
		struct StyleInfo
		{
			std::string name;
			std::string md5;

			bool ToJson(Json &json) const
			{
				json.Set("style", name);
				json.Set("checksum", md5);

				return true;
			}
		};

		u64 playerId;
		u16 filterId;
		std::string modeMD5;
		u32 teleports;
		f64 time;
		std::vector<StyleInfo> styles;
		std::string metadata;

		bool ToJson(Json &json) const
		{
			json.Set("player_id", this->playerId);
			json.Set("filter_id", this->filterId);
			json.Set("mode_md5", this->modeMD5);
			json.Set("styles", this->styles);
			json.Set("teleports", this->teleports);
			json.Set("time", this->time);
			json.Set("metadata", this->metadata);
			return true;
		}
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

		bool FromJson(const Json &json)
		{
			if (!json.Get("record_id", this->recordId))
			{
				return false;
			}

			Json pbData;

			if (!json.Get("pb_data", pbData))
			{
				return true;
			}

			if (!pbData.Get("player_rating", this->playerRating))
			{
				return false;
			}

			if (pbData.Get("nub_rank", this->overallData.rank))
			{
				if (!pbData.Get("nub_points", this->overallData.points))
				{
					return false;
				}

				if (!pbData.Get("nub_leaderboard_size", this->overallData.leaderboardSize))
				{
					return false;
				}
			}

			if (pbData.Get("pro_rank", this->proData.rank))
			{
				if (!pbData.Get("pro_points", this->proData.points))
				{
					return false;
				}

				if (!pbData.Get("pro_leaderboard_size", this->proData.leaderboardSize))
				{
					return false;
				}
			}

			return true;
		}
	};

	struct WantPlayerRecords
	{
		u16 mapId;
		u64 playerId;

		bool ToJson(Json &json) const
		{
			json.Set("map_id", this->mapId);
			json.Set("player_id", this->playerId);

			return true;
		}
	};

	struct PlayerRecords
	{
		std::vector<Record> records {};

		bool FromJson(const Json &json)
		{
			return json.Get("records", this->records);
		}
	};

	struct WantWorldRecordsForCache
	{
		u16 mapId;

		WantWorldRecordsForCache(u16 mapId) : mapId(mapId) {}

		bool ToJson(Json &json) const
		{
			return json.Set("map_id", this->mapId);
		}
	};

	struct WorldRecordsForCache
	{
		std::vector<Record> records {};

		bool FromJson(const Json &json)
		{
			return json.Get("records", this->records);
		}
	};

	struct WantCourseTop
	{
		std::string mapName;
		std::string courseNameOrNumber;
		Mode mode;
		u32 limit;
		u32 offset;

		bool ToJson(Json &json) const
		{
			bool success = true;
			success &= json.Set("map_name", mapName);
			success &= json.Set("course", courseNameOrNumber);
			success &= json.Set("mode", (u8)mode);
			success &= json.Set("limit", limit);
			success &= json.Set("offset", offset);
			return success;
		}
	};

	struct MapDetails
	{
		u16 id;
		std::string name;

		bool FromJson(const Json &json)
		{
			return json.Get("id", this->id) && json.Get("name", this->name);
		}
	};

	struct CourseDetails
	{
		u16 id;
		std::string name;
		KZ::API::Map::Course::Filter::Tier nubTier;
		KZ::API::Map::Course::Filter::Tier proTier;

		bool FromJson(const Json &json)
		{
			if (!(json.Get("id", this->id) && json.Get("name", this->name)))
			{
				return false;
			}

			std::string tier;

			if (!json.Get("nub_tier", tier))
			{
				return false;
			}

			if (!KZ::API::Map::Course::Filter::DecodeTierString(tier, this->nubTier))
			{
				return false;
			}

			if (!json.Get("pro_tier", tier))
			{
				return false;
			}

			if (!KZ::API::Map::Course::Filter::DecodeTierString(tier, this->proTier))
			{
				return false;
			}

			return true;
		}
	};

	struct CourseTop
	{
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::vector<Record> overall;
		std::vector<Record> pro;

		bool FromJson(const Json &json)
		{
			return json.Get("map", this->map) && json.Get("course", this->course) && json.Get("overall", this->overall) && json.Get("pro", this->pro);
		}
	};

	struct WantPersonalBest
	{
		u64 steamid64;
		std::string playerName;
		std::string mapName;
		std::string courseNameOrNumber;
		Mode mode;
		std::vector<std::string> styles;

		bool ToJson(Json &json) const
		{
			bool success = true;
			if (steamid64)
			{
				success &= json.Set("player", steamid64);
			}
			else
			{
				success &= json.Set("player", playerName);
			}
			success &= json.Set("map", mapName);
			success &= json.Set("course", courseNameOrNumber);
			success &= json.Set("mode", (u8)mode);
			success &= json.Set("styles", styles);
			return success;
		}
	};

	struct PersonalBest
	{
		std::optional<Player> player;
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::optional<Record> overall;
		std::optional<Record> pro;

		bool FromJson(const Json &json)
		{
			return json.Get("player", this->player) && json.Get("map", this->map) && json.Get("course", this->course)
				   && json.Get("overall", this->overall) && json.Get("pro", this->pro);
		}
	};

	struct WantWorldRecords
	{
		std::string mapName;
		std::string courseNameOrNumber;
		Mode mode;

		bool ToJson(Json &json) const
		{
			bool success = true;
			success &= json.Set("map", mapName);
			success &= json.Set("course", courseNameOrNumber);
			success &= json.Set("mode", (u8)mode);
			return success;
		}
	};

	struct WorldRecords
	{
		std::optional<MapDetails> map;
		std::optional<CourseDetails> course;
		std::optional<Record> overall;
		std::optional<Record> pro;

		bool FromJson(const Json &json)
		{
			return json.Get("map", this->map) && json.Get("course", this->course) && json.Get("overall", this->overall) && json.Get("pro", this->pro);
		}
	};
} // namespace KZ::API::events
