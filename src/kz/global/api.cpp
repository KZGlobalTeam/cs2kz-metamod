#include "common.h"
#include "utils/utils.h"
#include "api.h"

bool KZ::api::DecodeModeString(std::string_view modeString, Mode &mode)
{
	if (KZ_STREQI(modeString.data(), "vanilla") || KZ_STREQI(modeString.data(), "vnl"))
	{
		mode = Mode::Vanilla;
		return true;
	}
	else if (KZ_STREQI(modeString.data(), "classic") || KZ_STREQI(modeString.data(), "ckz"))
	{
		mode = Mode::Classic;
		return true;
	}
	else
	{
		return false;
	}
}

bool KZ::api::PlayerInfo::FromJson(const Json &json)
{
	if (!json.Get("id", this->id))
	{
		std::string id;

		if (!json.Get("id", id))
		{
			return false;
		}

		if (!utils::ParseSteamID2(id, this->id))
		{
			META_CONPRINTF("[KZ::Global] Failed to parse SteamID2.\n");
			return false;
		}
	}

	return json.Get("name", this->name);
}

bool KZ::api::Player::FromJson(const Json &json)
{
	bool success = true;

	if (!json.Get("id", this->id))
	{
		std::string id;

		if (!json.Get("id", id))
		{
			return false;
		}

		if (!utils::ParseSteamID2(id, this->id))
		{
			META_CONPRINTF("[KZ::Global] Failed to parse SteamID2.\n");
			return false;
		}
	}

	success &= json.Get("name", this->name);
	success &= json.Get("is_banned", this->isBanned);

	return success;
}

bool KZ::api::Map::FromJson(const Json &json)
{
	if (!json.Get("id", this->id))
	{
		return false;
	}

	if (!json.Get("workshop_id", this->workshopId))
	{
		return false;
	}

	if (!json.Get("name", this->name))
	{
		return false;
	}

	if (!json.Get("description", this->description))
	{
		return false;
	}

	std::string state;

	if (!json.Get("state", state))
	{
		return false;
	}

	if (!DecodeStateString(state, this->state))
	{
		return false;
	}

	if (!json.Get("vpk_checksum", this->vpkChecksum))
	{
		return false;
	}

	if (!json.Get("mappers", this->mappers))
	{
		return false;
	}

	if (!json.Get("courses", this->courses))
	{
		return false;
	}

	if (!json.Get("approved_at", this->approvedAt))
	{
		return false;
	}

	return true;
}

bool KZ::api::Map::DecodeStateString(std::string_view stateString, State &state)
{
	if (stateString == "invalid")
	{
		state = KZ::api::Map::State::Invalid;
	}
	else if (stateString == "in-testing")
	{
		state = KZ::api::Map::State::InTesting;
	}
	else if (stateString == "approved")
	{
		state = KZ::api::Map::State::Approved;
	}
	else
	{
		META_CONPRINTF("[KZ::Global] `state` field has an unknown value.\n");
		return false;
	}

	return true;
}

bool KZ::api::Map::Course::FromJson(const Json &json)
{
	// clang-format off
	return json.Get("id", this->id)
		&& json.Get("name", this->name)
		&& json.Get("description", this->description)
		&& json.Get("mappers", this->mappers)
		&& json.Get("filters", this->filters);
	// clang-format on
}

bool KZ::api::Map::Course::Filters::FromJson(const Json &json)
{
	return json.Get("vanilla", this->vanilla) && json.Get("classic", this->classic);
}

bool KZ::api::Map::Course::Filter::FromJson(const Json &json)
{
	if (!json.Get("id", this->id))
	{
		return false;
	}

	std::string nubTier;

	if (!json.Get("nub_tier", nubTier))
	{
		return false;
	}

	if (!DecodeTierString(nubTier, this->nubTier))
	{
		return false;
	}

	std::string proTier;

	if (!json.Get("pro_tier", proTier))
	{
		return false;
	}

	if (!DecodeTierString(proTier, this->proTier))
	{
		return false;
	}

	std::string state;

	if (!json.Get("state", state))
	{
		return false;
	}

	if (!DecodeStateString(state, this->state))
	{
		return false;
	}

	if (!json.Get("notes", this->notes))
	{
		return false;
	}

	return true;
}

bool KZ::api::Map::Course::Filter::DecodeTierString(std::string_view tierString, Tier &tier)
{
	if (tierString == "very-easy")
	{
		tier = KZ::api::Map::Course::Filter::Tier::VeryEasy;
	}
	else if (tierString == "easy")
	{
		tier = KZ::api::Map::Course::Filter::Tier::Easy;
	}
	else if (tierString == "medium")
	{
		tier = KZ::api::Map::Course::Filter::Tier::Medium;
	}
	else if (tierString == "advanced")
	{
		tier = KZ::api::Map::Course::Filter::Tier::Advanced;
	}
	else if (tierString == "hard")
	{
		tier = KZ::api::Map::Course::Filter::Tier::Hard;
	}
	else if (tierString == "very-hard")
	{
		tier = KZ::api::Map::Course::Filter::Tier::VeryHard;
	}
	else if (tierString == "extreme")
	{
		tier = KZ::api::Map::Course::Filter::Tier::Extreme;
	}
	else if (tierString == "death")
	{
		tier = KZ::api::Map::Course::Filter::Tier::Death;
	}
	else if (tierString == "unfeasible")
	{
		tier = KZ::api::Map::Course::Filter::Tier::Unfeasible;
	}
	else if (tierString == "impossible")
	{
		tier = KZ::api::Map::Course::Filter::Tier::Impossible;
	}
	else
	{
		META_CONPRINTF("[KZ::Global] `tier` field has an unknown tierString.\n");
		return false;
	}

	return true;
}

bool KZ::api::Map::Course::Filter::DecodeStateString(std::string_view stateString, State &state)
{
	if (stateString == "unranked")
	{
		state = KZ::api::Map::Course::Filter::State::Unranked;
	}
	else if (stateString == "pending")
	{
		state = KZ::api::Map::Course::Filter::State::Pending;
	}
	else if (stateString == "ranked")
	{
		state = KZ::api::Map::Course::Filter::State::Ranked;
	}
	else
	{
		META_CONPRINTF("[KZ::Global] `state` field has an unknown value.\n");
		return false;
	}

	return true;
}

bool KZ::api::Record::FromJson(const Json &json)
{
	if (!json.Get("id", this->id))
	{
		return false;
	}

	if (!json.Get("player", this->player))
	{
		return false;
	}

	if (!json.Get("map", this->map))
	{
		return false;
	}

	if (!json.Get("course", this->course))
	{
		return false;
	}

	std::string mode = "";

	if (!json.Get("mode", mode))
	{
		return false;
	}

	if (!DecodeModeString(mode, this->mode))
	{
		return false;
	}

	if (!json.Get("teleports", this->teleports))
	{
		return false;
	}

	if (!json.Get("time", this->time))
	{
		return false;
	}

	if (json.Get("nub_rank", this->nubRank))
	{
		if (!json.Get("nub_points", this->nubPoints))
		{
			return false;
		}
		if (!json.Get("nub_max_rank", this->nubMaxRank))
		{
			return false;
		}
	}

	if (json.Get("pro_rank", this->proRank))
	{
		if (!json.Get("pro_points", this->proPoints))
		{
			return false;
		}
		if (!json.Get("pro_max_rank", this->proMaxRank))
		{
			return false;
		}
	}

	return true;
}

bool KZ::api::Record::MapInfo::FromJson(const Json &json)
{
	return json.Get("id", this->id) && json.Get("name", this->name);
}

bool KZ::api::Record::CourseInfo::FromJson(const Json &json)
{
	return json.Get("id", this->id) && json.Get("name", this->name);
}
