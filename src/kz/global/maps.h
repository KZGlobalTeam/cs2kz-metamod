#pragma once

#include <optional>
#include <string>
#include "error.h"
#include "modes.h"
#include "players.h"
#include "utils/json.h"

namespace KZ::API
{
	// A KZ map fetched from the API.
	struct Map
	{
		// The 3 states a map can be in.
		enum class GlobalStatus
		{
			NOT_GLOBAL,
			IN_TESTING,
			GLOBAL,
		};

		// A map course.
		struct Course
		{
			// A course filter.
			struct Filter
			{
				// The 10 official KZ tiers.
				enum class Tier
				{
					VERY_EASY,
					EASY,
					MEDIUM,
					ADVANCED,
					HARD,
					VERY_HARD,
					EXTREME,
					DEATH,
					UNFEASIBLE,
					IMPOSSIBLE,
				};

				// The 3 states a filter can be in.
				enum class RankedStatus
				{
					NEVER,
					UNRANKED,
					RANKED,
				};

				// Deserializes a `Filter` from a JSON object.
				static std::optional<KZ::API::ParseError> Deserialize(const json &json, Filter &filter);

				// Deserializes a `Tier` from a JSON value.
				static std::optional<KZ::API::ParseError> DeserializeTier(const json &json, Tier &tier);

				// Deserializes a `RankedStatus` from a JSON value.
				static std::optional<KZ::API::ParseError> DeserializeRankedStatus(const json &json, RankedStatus &rankedStatus);

				u16 id;
				Mode mode;
				bool teleports;
				Tier tier;
				RankedStatus rankedStatus;
				std::optional<std::string> notes {};
			};

			// Deserializes a `Course` from a JSON object.
			static std::optional<KZ::API::ParseError> Deserialize(const json &json, Course &course);

			u16 id;
			std::optional<std::string> name {};
			std::optional<std::string> description {};
			std::vector<Player> mappers {};
			std::vector<Filter> filters {};
		};

		// Deserializes a `Map` from a JSON object.
		static std::optional<KZ::API::ParseError> Deserialize(const json &json, Map &map);

		// Deserializes a `GlobalStatus` from a JSON value.
		static std::optional<KZ::API::ParseError> DeserializeGlobalStatus(const json &json, GlobalStatus &globalStatus);

		u16 id;
		std::string name;
		std::optional<std::string> description {};
		GlobalStatus globalStatus;
		u32 workshopID;
		std::string checksum;
		std::vector<Player> mappers {};
		std::vector<Course> courses {};
		std::string createdOn;
	};
} // namespace KZ::API
