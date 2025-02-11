#include "maps.h"

bool KZ::API::Map::DecodeStateString(const std::string &stateString, KZ::API::Map::State &state)
{
	if (stateString == "invalid")
	{
		state = KZ::API::Map::State::Invalid;
	}
	else if (stateString == "in-testing")
	{
		state = KZ::API::Map::State::InTesting;
	}
	else if (stateString == "approved")
	{
		state = KZ::API::Map::State::Approved;
	}
	else
	{
		META_CONPRINTF("[KZ::Global] `state` field has an unknown value.\n");
		return false;
	}

	return true;
}

bool KZ::API::Map::Course::Filter::DecodeTierString(const std::string &tierString, KZ::API::Map::Course::Filter::Tier &tier)
{
	if (tierString == "very-easy")
	{
		tier = KZ::API::Map::Course::Filter::Tier::VeryEasy;
	}
	else if (tierString == "easy")
	{
		tier = KZ::API::Map::Course::Filter::Tier::Easy;
	}
	else if (tierString == "medium")
	{
		tier = KZ::API::Map::Course::Filter::Tier::Medium;
	}
	else if (tierString == "advanced")
	{
		tier = KZ::API::Map::Course::Filter::Tier::Advanced;
	}
	else if (tierString == "hard")
	{
		tier = KZ::API::Map::Course::Filter::Tier::Hard;
	}
	else if (tierString == "very-hard")
	{
		tier = KZ::API::Map::Course::Filter::Tier::VeryHard;
	}
	else if (tierString == "extreme")
	{
		tier = KZ::API::Map::Course::Filter::Tier::Extreme;
	}
	else if (tierString == "death")
	{
		tier = KZ::API::Map::Course::Filter::Tier::Death;
	}
	else if (tierString == "unfeasible")
	{
		tier = KZ::API::Map::Course::Filter::Tier::Unfeasible;
	}
	else if (tierString == "impossible")
	{
		tier = KZ::API::Map::Course::Filter::Tier::Impossible;
	}
	else
	{
		META_CONPRINTF("[KZ::Global] `tier` field has an unknown tierString.\n");
		return false;
	}

	return true;
}

bool KZ::API::Map::Course::Filter::DecodeStateString(const std::string &stateString, KZ::API::Map::Course::Filter::State &state)
{
	if (stateString == "unranked")
	{
		state = KZ::API::Map::Course::Filter::State::Unranked;
	}
	else if (stateString == "pending")
	{
		state = KZ::API::Map::Course::Filter::State::Pending;
	}
	else if (stateString == "ranked")
	{
		state = KZ::API::Map::Course::Filter::State::Ranked;
	}
	else
	{
		META_CONPRINTF("[KZ::Global] `state` field has an unknown value.\n");
		return false;
	}

	return true;
}
