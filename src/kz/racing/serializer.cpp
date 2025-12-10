#include "kz_racing.h"

// ===== PlayerInfo =====

bool KZ::racing::PlayerInfo::ToJson(Json &json) const
{
	std::string id = std::to_string(this->id);
	return json.Set("id", id) && json.Set("name", this->name);
}

bool KZ::racing::PlayerInfo::FromJson(const Json &json)
{
	std::string id;

	if (!json.Get("id", id))
	{
		return false;
	}

	this->id = atoll(id.c_str());

	return json.Get("name", this->name);
}

// ===== RaceInfo =====

bool KZ::racing::RaceInfo::ToJson(Json &json) const
{
	// clang-format off
	return /* json.Set("map_name", this->mapName)
		&& */ json.Set("map_id", this->workshopID)
		&& json.Set("course", this->courseName)
		&& json.Set("mode", this->modeName)
		&& json.Set("max_teleports", this->maxTeleports)
		&& json.Set("duration", this->duration);
	// clang-format on
}

bool KZ::racing::RaceInfo::FromJson(const Json &json)
{
	// clang-format off
	return /* json.Get("map_name", this->mapName)
		&& */ json.Get("map_id", this->workshopID)
		&& json.Get("course", this->courseName)
		&& json.Get("mode", this->modeName)
		&& json.Get("max_teleports", this->maxTeleports)
		&& json.Get("duration", this->duration);
	// clang-format on
}

// ===== Events =====

bool KZ::racing::events::RaceInit::ToJson(Json &json) const
{
	return this->raceInfo.ToJson(json);
}

bool KZ::racing::events::RaceInit::FromJson(const Json &json)
{
	return this->raceInfo.FromJson(json);
}

bool KZ::racing::events::RaceStart::ToJson(Json &json) const
{
	return json.Set("countdown", this->countdownSeconds);
}

bool KZ::racing::events::RaceStart::FromJson(const Json &json)
{
	return json.Get("countdown", this->countdownSeconds);
}

bool KZ::racing::events::PlayerAccept::ToJson(Json &json) const
{
	return json.Set("player", this->player);
}

bool KZ::racing::events::PlayerAccept::FromJson(const Json &json)
{
	return json.Get("player", this->player);
}

bool KZ::racing::events::PlayerUnregister::ToJson(Json &json) const
{
	return json.Set("player", this->player);
}

bool KZ::racing::events::PlayerUnregister::FromJson(const Json &json)
{
	return json.Get("player", this->player);
}

bool KZ::racing::events::PlayerForfeit::ToJson(Json &json) const
{
	return json.Set("player", this->player);
}

bool KZ::racing::events::PlayerForfeit::FromJson(const Json &json)
{
	return json.Get("player", this->player);
}

bool KZ::racing::events::PlayerFinish::ToJson(Json &json) const
{
	// clang-format off
	return json.Set("player", this->player)
		&& json.Set("time", this->time)
		&& json.Set("teleports", this->teleportsUsed);
	// clang-format on
}

bool KZ::racing::events::PlayerFinish::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("player", this->player)
		&& json.Get("time", this->time)
		&& json.Get("teleports", this->teleportsUsed);
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
	if (!json.Get("player", this->player))
	{
		return false;
	}

	if (json.ContainsKey("time"))
	{
		this->completed = true;
		return json.Get("time", this->time) && json.Get("teleports", this->teleportsUsed);
	}

	return true;
}

bool KZ::racing::events::ChatMessage::ToJson(Json &json) const
{
	return json.Set("player", this->player) && json.Set("content", this->message);
}

bool KZ::racing::events::ChatMessage::FromJson(const Json &json)
{
	return json.Get("player", this->player) && json.Get("content", this->message);
}
