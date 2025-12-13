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

// ===== Events =====

bool KZ::racing::events::InitRace::ToJson(Json &json) const
{
	// clang-format off
	return json.Set("map_id", this->workshopID)
		&& json.Set("course", this->courseName)
		&& json.Set("mode", this->modeName)
		&& json.Set("max_duration", this->maxDurationSeconds)
		&& json.Set("max_teleports", this->maxTeleports);
	// clang-format on
}

bool KZ::racing::events::InitRace::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("map_id", this->workshopID)
		&& json.Get("course", this->courseName)
		&& json.Get("mode", this->modeName)
		&& json.Get("max_duration", this->maxDurationSeconds)
		&& json.Get("max_teleports", this->maxTeleports);
	// clang-format on
}

bool KZ::racing::events::BeginRace::ToJson(Json &json) const
{
	return json.Set("countdown_duration", this->countdownSeconds);
}

bool KZ::racing::events::BeginRace::FromJson(const Json &json)
{
	return json.Get("countdown_duration", this->countdownSeconds);
}

bool KZ::racing::events::ServerJoinRace::FromJson(const Json &json)
{
	return json.Get("name", this->name);
}

bool KZ::racing::events::ServerLeaveRace::FromJson(const Json &json)
{
	return json.Get("name", this->name);
}

bool KZ::racing::events::PlayerJoinRace::ToJson(Json &json) const
{
	return this->player.ToJson(json);
}

bool KZ::racing::events::PlayerJoinRace::FromJson(const Json &json)
{
	return this->player.FromJson(json);
}

bool KZ::racing::events::PlayerLeaveRace::ToJson(Json &json) const
{
	return this->player.ToJson(json);
}

bool KZ::racing::events::PlayerLeaveRace::FromJson(const Json &json)
{
	return this->player.FromJson(json);
}

bool KZ::racing::events::PlayerFinish::ToJson(Json &json) const
{
	// clang-format off
	return json.Set("player", this->player)
		&& json.Set("teleports", this->teleports)
		&& json.Set("time", this->timeSeconds);
	// clang-format on
}

bool KZ::racing::events::PlayerFinish::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("player", this->player)
		&& json.Get("teleports", this->teleports)
		&& json.Get("time", this->timeSeconds);
	// clang-format on
}

bool KZ::racing::events::PlayerDisconnect::ToJson(Json &json) const
{
	return this->player.ToJson(json);
}

bool KZ::racing::events::PlayerDisconnect::FromJson(const Json &json)
{
	return this->player.FromJson(json);
}

bool KZ::racing::events::PlayerSurrender::ToJson(Json &json) const
{
	return this->player.ToJson(json);
}

bool KZ::racing::events::PlayerSurrender::FromJson(const Json &json)
{
	return this->player.FromJson(json);
}

bool KZ::racing::events::EndRace::ToJson(Json &json) const
{
	switch (this->reason)
	{
		case Reason::Timeout:
			return json.Set("reason", "timeout");

		case Reason::Forced:
			return json.Set("reason", "forced");
	}
}

bool KZ::racing::events::RaceResults::FromJson(const Json &json)
{
	return json.Get("participants", this->participants);
}

bool KZ::racing::events::RaceResults::Participant::FromJson(const Json &json)
{
	std::string id;

	if (!json.Get("id", id))
	{
		return false;
	}

	this->id = atoll(id.c_str());

	if (!json.Get("name", this->name))
	{
		return false;
	}

	std::string state;

	if (!json.Get("state", state))
	{
		return false;
	}

	if (state == "disconnected")
	{
		this->state = State::Disconnected;
	}
	else if (state == "surrendered")
	{
		this->state = State::Surrendered;
	}
	else if (state == "did_not_finish")
	{
		this->state = State::DidNotFinish;
	}
	else if (state == "finished")
	{
		this->state = State::Finished;
		return json.Get("teleports", this->teleports) && json.Get("time", this->timeSeconds);
	}

	return true;
}

bool KZ::racing::events::ChatMessage::ToJson(Json &json) const
{
	return json.Set("player", this->player) && json.Set("content", this->content);
}

bool KZ::racing::events::ChatMessage::FromJson(const Json &json)
{
	return json.Get("player", this->player) && json.Get("content", this->content);
}
