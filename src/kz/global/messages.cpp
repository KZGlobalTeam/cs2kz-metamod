#include "messages.h"
#include "version_gen.h"

bool KZ::api::messages::handshake::Hello::PlayerInfo::ToJson(Json &json) const
{
	return json.Set("id", this->id) && json.Set("name", this->name);
}

bool KZ::api::messages::handshake::Hello::ToJson(Json &json) const
{
	return json.Set("plugin_version", PLUGIN_FULL_VERSION) && json.Set("plugin_version_checksum", this->checksum)
		   && json.Set("map", this->currentMapName) && json.Set("players", this->players);
}

bool KZ::api::messages::handshake::HelloAck::ModeInfo::FromJson(const Json &json)
{
	std::string mode;

	if (!json.Get("mode", mode))
	{
		return false;
	}

	if (!KZ::api::DecodeModeString(mode, this->mode))
	{
		return false;
	}

	return json.Get("linux_checksum", this->linuxChecksum) && json.Get("windows_checksum", this->windowsChecksum);
}

bool KZ::api::messages::handshake::HelloAck::StyleInfo::FromJson(const Json &json)
{
	return json.Get("style", this->style) && json.Get("linux_checksum", this->linuxChecksum) && json.Get("windows_checksum", this->windowsChecksum);
}

bool KZ::api::messages::handshake::HelloAck::Announcement::FromJson(const Json &json)
{
	return json.Get("id", this->id) && json.Get("title", this->title) && json.Get("body", this->body) && json.Get("starts_at", this->startsAt)
		   && json.Get("expires_at", this->expiresAt);
}

bool KZ::api::messages::handshake::HelloAck::FromJson(const Json &json)
{
	return json.Get("map", this->mapInfo) && json.Get("modes", this->modes) && json.Get("styles", this->styles)
		   && json.Get("announcements", this->announcements);
}

bool KZ::api::messages::Error::FromJson(const Json &json)
{
	return json.Get("message", this->message);
}

bool KZ::api::messages::MapChange::ToJson(Json &json) const
{
	return json.Set("new_map", this->name);
}

bool KZ::api::messages::PlayerJoin::ToJson(Json &json) const
{
	return json.Set("id", this->id) && json.Set("name", this->name) && json.Set("ip_address", this->ipAddress);
}

bool KZ::api::messages::PlayerJoinAck::FromJson(const Json &json)
{
	return json.Get("preferences", this->preferences) && json.Get("is_banned", this->isBanned);
}

bool KZ::api::messages::PlayerLeave::ToJson(Json &json) const
{
	return json.Set("id", this->id) && json.Set("name", this->name) && json.Set("preferences", this->preferences);
}

bool KZ::api::messages::WantWorldRecordsForCache::ToJson(Json &json) const
{
	return json.Set("map_id", this->mapID);
}

bool KZ::api::messages::WorldRecordsForCache::FromJson(const Json &json)
{
	return json.Get("records", this->records);
}

bool KZ::api::messages::NewRecord::StyleInfo::ToJson(Json &json) const
{
	return json.Set("style", this->name) && json.Set("checksum", this->checksum);
}

bool KZ::api::messages::NewRecord::ToJson(Json &json) const
{
	// clang-format off
	return json.Set("player_id", this->playerID)
		&& json.Set("filter_id", this->filterID)
		&& json.Set("mode_md5", this->modeChecksum)
		&& json.Set("teleports", this->teleports)
		&& json.Set("time", this->time)
		&& json.Set("styles", this->styles)
		&& json.Set("metadata", this->metadata);
	// clang-format on
}

bool KZ::api::messages::NewRecordAck::FromJson(const Json &json)
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

bool KZ::api::messages::MapDetails::FromJson(const Json &json)
{
	return json.Get("id", this->id) && json.Get("name", this->name);
}

bool KZ::api::messages::CourseDetails::FromJson(const Json &json)
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

	if (!KZ::api::Map::Course::Filter::DecodeTierString(tier, this->nubTier))
	{
		return false;
	}

	if (!json.Get("pro_tier", tier))
	{
		return false;
	}

	if (!KZ::api::Map::Course::Filter::DecodeTierString(tier, this->proTier))
	{
		return false;
	}

	return true;
}

bool KZ::api::messages::WantPersonalBest::ToJson(Json &json) const
{
	return json.Set("player", this->player) && json.Set("map", this->map) && json.Set("course", this->course) && json.Set("mode", (u8)this->mode)
		   && json.Set("styles", this->styles);
}

bool KZ::api::messages::PersonalBest::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("player", this->player)
		&& json.Get("map", this->map)
		&& json.Get("course", this->course)
		&& json.Get("overall", this->overall)
		&& json.Get("pro", this->pro);
	// clang-format on
}

bool KZ::api::messages::WantCourseTop::ToJson(Json &json) const
{
	// clang-format off
	return json.Set("map_name", this->map)
		&& json.Set("course", this->course)
		&& json.Set("mode", (u8)this->mode)
		&& json.Set("limit", this->limit)
		&& json.Set("offset", this->offset);
	// clang-format on
}

bool KZ::api::messages::CourseTop::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("map", this->map)
		&& json.Get("course", this->course)
		&& json.Get("overall", this->overall)
		&& json.Get("pro", this->pro);
	// clang-format on
}

bool KZ::api::messages::WantWorldRecords::ToJson(Json &json) const
{
	// clang-format off
	return json.Set("map", this->map)
		&& json.Set("course", this->course)
		&& json.Set("mode", (u8)this->mode);
	// clang-format on
}

bool KZ::api::messages::WorldRecords::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("map", this->map)
		&& json.Get("course", this->course)
		&& json.Get("overall", this->overall)
		&& json.Get("pro", this->pro);
	// clang-format on
}

bool KZ::api::messages::WantPlayerRecords::ToJson(Json &json) const
{
	return json.Set("map_id", this->mapID) && json.Set("player_id", this->playerID);
}

bool KZ::api::messages::PlayerRecords::FromJson(const Json &json)
{
	return json.Get("records", this->records);
}
