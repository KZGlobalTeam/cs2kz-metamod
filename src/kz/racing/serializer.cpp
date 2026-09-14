#include "kz_racing.h"

// The coordinator identifies modes by its own names instead of what CS2KZ uses.
static_function void TranslateModeName(std::string &modeName)
{
	if (modeName == "cs2kz_classic")
	{
		modeName = "Classic";
	}
	else if (modeName == "cs2kz_vanilla")
	{
		modeName = "Vanilla";
	}
}

bool KZ::racing::RaceConfig::FromJson(const Json &json)
{
	// clang-format off
	if (!(json.Get("map_workshop_id", this->workshopID)
		&& json.Get("map_name", this->mapName)
		&& json.Get("course_name", this->courseName)
		&& json.Get("mode", this->modeName)
		&& json.Get("max_duration", this->maxDurationSeconds)
		&& json.Get("max_teleports", this->maxTeleports)))
	{
		return false;
	}
	// clang-format on

	TranslateModeName(this->modeName);
	return true;
}

bool KZ::racing::RaceResult::FromJson(const Json &json)
{
	std::string status;

	if (!json.Get("status", status))
	{
		return false;
	}

	if (status == "finished")
	{
		this->status = KZ::racing::RaceResult::Status::Finished;
	}
	else if (status == "surrendered")
	{
		this->status = KZ::racing::RaceResult::Status::Surrendered;
	}
	else if (status == "disconnected")
	{
		this->status = KZ::racing::RaceResult::Status::Disconnected;
	}
	else if (status == "did_not_finish")
	{
		this->status = KZ::racing::RaceResult::Status::DidNotFinish;
	}
	else
	{
		return false;
	}

	return json.Get("player_name", this->playerName) && json.Get("time", this->timeSeconds) && json.Get("teleports", this->teleports);
}

// ===== Events =====

bool KZ::racing::events::ChatMessage::ToJson(Json &json) const
{
	return json.Set("content", this->content) && json.Set("player", this->player);
}

bool KZ::racing::events::ChatMessage::FromJson(const Json &json)
{
	return json.Get("content", this->content) && json.Get("player", this->player);
}

bool KZ::racing::events::RaceConfigured::FromJson(const Json &json)
{
	return this->conf.FromJson(json);
}

bool KZ::racing::events::RaceStarting::FromJson(const Json &json)
{
	return json.Get("countdown_duration", this->countdownSeconds);
}

bool KZ::racing::events::RaceCancelled::FromJson(const Json &json)
{
	return true;
}

bool KZ::racing::events::RaceCompleted::FromJson(const Json &json)
{
	return json.Get("results", this->results);
}

bool KZ::racing::events::PlayerReady::ToJson(Json &json) const
{
	return json.Set("player", this->player);
}

bool KZ::racing::events::PlayerReady::FromJson(const Json &json)
{
	return json.Get("player", this->player);
}

bool KZ::racing::events::PlayerFinished::ToJson(Json &json) const
{
	return json.Set("player", this->player) && json.Set("time", this->timeSeconds) && json.Set("teleports", this->teleports);
}

bool KZ::racing::events::PlayerFinished::FromJson(const Json &json)
{
	return json.Get("player", this->player) && json.Get("time", this->timeSeconds) && json.Get("teleports", this->teleports);
}

bool KZ::racing::events::PlayerSurrendered::ToJson(Json &json) const
{
	return json.Set("player", this->player);
}

bool KZ::racing::events::PlayerSurrendered::FromJson(const Json &json)
{
	return json.Get("player", this->player);
}

bool KZ::racing::events::PlayerDisconnected::ToJson(Json &json) const
{
	return json.Set("player", this->player);
}

bool KZ::racing::events::PlayerDisconnected::FromJson(const Json &json)
{
	return json.Get("player", this->player);
}
