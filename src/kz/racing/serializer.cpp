#include "kz_racing.h"

// ===== PlayerInfo =====

bool KZ::racing::PlayerInfo::ToJson(Json &json) const
{
	return json.Set("id", this->id) && json.Set("name", this->name);
}

bool KZ::racing::PlayerInfo::FromJson(const Json &json)
{
	return json.Get("id", this->id) && json.Get("name", this->name);
}

// ===== MapInfo =====

bool KZ::racing::MapInfo::ToJson(Json &json) const
{
	return json.Set("map_name", this->mapName) && json.Set("workshop_id", this->workshopID);
}

// ===== RaceInfo =====

bool KZ::racing::RaceInfo::ToJson(Json &json) const
{
	// clang-format off
	return json.Set("map_name", this->mapName)
		&& json.Set("workshop_id", this->workshopID)
		&& json.Set("course_name", this->courseName)
		&& json.Set("mode_name", this->modeName)
		&& json.Set("max_teleports", this->maxTeleports)
		&& json.Set("duration", this->duration);
	// clang-format on
}

// ===== Handshake =====

bool KZ::racing::handshake::Hello::ToJson(Json &json) const
{
	return this->mapInfo.ToJson(json);
}

bool KZ::racing::handshake::HelloAck::FromJson(const Json &json)
{
	if (!json.Get("heartbeat_interval", this->heartbeatInterval))
	{
		return false;
	}

	Json raceInfoJson;
	if (json.Get("race_info", raceInfoJson))
	{
		KZ::racing::RaceInfo raceInfo;
		if (raceInfoJson.Get("map_name", raceInfo.mapName) && raceInfoJson.Get("workshop_id", raceInfo.workshopID)
			&& raceInfoJson.Get("course_name", raceInfo.courseName) && raceInfoJson.Get("mode_name", raceInfo.modeName)
			&& raceInfoJson.Get("max_teleports", raceInfo.maxTeleports) && raceInfoJson.Get("duration", raceInfo.duration))
		{
			this->raceInfo = raceInfo;
		}
	}

	return true;
}

// ===== Events =====

bool KZ::racing::events::RaceInit::ToJson(Json &json) const
{
	return json.Set("race_info", this->raceInfo);
}

bool KZ::racing::events::RaceInit::FromJson(const Json &json)
{
	Json raceInfoJson;
	if (!json.Get("race_info", raceInfoJson))
	{
		return false;
	}

	// clang-format off
	return raceInfoJson.Get("map_name", this->raceInfo.mapName)
		&& raceInfoJson.Get("workshop_id", this->raceInfo.workshopID)
		&& raceInfoJson.Get("course_name", this->raceInfo.courseName)
		&& raceInfoJson.Get("mode_name", this->raceInfo.modeName)
		&& raceInfoJson.Get("max_teleports", this->raceInfo.maxTeleports)
		&& raceInfoJson.Get("duration", this->raceInfo.duration);
	// clang-format on
}

bool KZ::racing::events::MapUpdated::ToJson(Json &json) const
{
	return json.Set("map_info", this->mapInfo);
}

bool KZ::racing::events::RaceStart::ToJson(Json &json) const
{
	return json.Set("countdown_seconds", this->countdownSeconds);
}

bool KZ::racing::events::RaceStart::FromJson(const Json &json)
{
	return json.Get("countdown_seconds", this->countdownSeconds);
}

bool KZ::racing::events::PlayerForfeit::ToJson(Json &json) const
{
	return json.Set("player", this->player) && json.Set("manual", this->manual);
}

bool KZ::racing::events::PlayerForfeit::FromJson(const Json &json)
{
	return json.Get("player", this->player) && json.Get("manual", this->manual);
}

bool KZ::racing::events::PlayerFinish::ToJson(Json &json) const
{
	// clang-format off
	return json.Set("player", this->player)
		&& json.Set("time", this->time)
		&& json.Set("teleports_used", this->teleportsUsed);
	// clang-format on
}

bool KZ::racing::events::PlayerFinish::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("player", this->player)
		&& json.Get("time", this->time)
		&& json.Get("teleports_used", this->teleportsUsed);
	// clang-format on
}

bool KZ::racing::events::RaceEnd::ToJson(Json &json) const
{
	return json.Set("manual", this->manual);
}

bool KZ::racing::events::RaceEnd::FromJson(const Json &json)
{
	return json.Get("manual", this->manual);
}

bool KZ::racing::events::RaceResult::FromJson(const Json &json)
{
	return json.Get("finishers", this->finishers);
}

bool KZ::racing::events::RaceResult::Finisher::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("player", this->player)
		&& json.Get("time", this->time)
		&& json.Get("teleports_used", this->teleportsUsed)
		&& json.Get("completed", this->completed);
	// clang-format on
}

bool KZ::racing::events::ChatMessage::ToJson(Json &json) const
{
	return json.Set("player", this->player) && json.Set("message", this->message);
}

bool KZ::racing::events::ChatMessage::FromJson(const Json &json)
{
	return json.Get("player", this->player) && json.Get("message", this->message);
}
