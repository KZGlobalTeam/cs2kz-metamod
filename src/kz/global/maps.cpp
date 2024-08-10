#include "maps.h"

namespace KZ::API
{
	std::optional<KZ::API::ParseError> Map::Deserialize(const json &json, Map &map)
	{
		if (!json.is_object())
		{
			return KZ::API::ParseError("map is not an object");
		}

		if (!json.contains("id"))
		{
			return KZ::API::ParseError("map object is missing `id` property");
		}

		if (!json["id"].is_number_unsigned())
		{
			return KZ::API::ParseError("map object `id` property is not an integer");
		}

		map.id = json["id"];

		if (!json.contains("name"))
		{
			return KZ::API::ParseError("map object is missing `name` property");
		}

		if (!json["name"].is_string())
		{
			return KZ::API::ParseError("map object `name` property is not a string");
		}

		map.name = json["name"];

		if (json.contains("description"))
		{
			if (!json["description"].is_string())
			{
				return KZ::API::ParseError("map object `description` property is not a string");
			}

			map.description = json["description"];
		}

		if (!json.contains("global_status"))
		{
			return KZ::API::ParseError("map object is missing `global_status` property");
		}

		if (auto error = Map::DeserializeGlobalStatus(json["global_status"], map.globalStatus))
		{
			return error;
		}

		if (!json.contains("workshop_id"))
		{
			return KZ::API::ParseError("map object is missing `workshop_id` property");
		}

		if (!json["workshop_id"].is_number_unsigned())
		{
			return KZ::API::ParseError("map object `workshop_id` property is not an integer");
		}

		map.workshopID = json["workshop_id"];

		if (!json.contains("checksum"))
		{
			return KZ::API::ParseError("map object is missing `checksum` property");
		}

		if (!json["checksum"].is_string())
		{
			return KZ::API::ParseError("map object `checksum` property is not an integer");
		}

		map.checksum = json["checksum"];

		if (json.contains("mappers"))
		{
			if (!json["mappers"].is_array())
			{
				return KZ::API::ParseError("map object `mappers` property is not an array");
			}

			for (const auto &rawMapper : json["mappers"])
			{
				KZ::API::Player mapper;

				if (auto error = KZ::API::Player::Deserialize(rawMapper, mapper))
				{
					return error;
				}

				map.mappers.push_back(mapper);
			}
		}

		if (json.contains("courses"))
		{
			if (!json["courses"].is_array())
			{
				return KZ::API::ParseError("map object `courses` property is not an array");
			}

			for (const auto &rawCourse : json["courses"])
			{
				Course course;

				if (auto error = Course::Deserialize(rawCourse, course))
				{
					return error;
				}

				map.courses.push_back(course);
			}
		}

		if (!json.contains("created_on"))
		{
			return KZ::API::ParseError("map object is missing `created_on` property");
		}

		map.createdOn = json["created_on"];

		return std::nullopt;
	}

	std::optional<KZ::API::ParseError> Map::DeserializeGlobalStatus(const json &json, Map::GlobalStatus &globalStatus)
	{
		if (!json.is_string())
		{
			return KZ::API::ParseError("global status is not a string");
		}

		std::string value = json;

		if (value == "not_global")
		{
			globalStatus = Map::GlobalStatus::NOT_GLOBAL;
		}
		else if (value == "in_testing")
		{
			globalStatus = Map::GlobalStatus::IN_TESTING;
		}
		else if (value == "global")
		{
			globalStatus = Map::GlobalStatus::GLOBAL;
		}
		else
		{
			return KZ::API::ParseError("global status has unknown value");
		}

		return std::nullopt;
	}

	std::optional<KZ::API::ParseError> Map::Course::Deserialize(const json &json, Map::Course &course)
	{
		if (!json.is_object())
		{
			return KZ::API::ParseError("map course is not an object");
		}

		if (!json.contains("id"))
		{
			return KZ::API::ParseError("course object is missing `id` property");
		}

		if (!json["id"].is_number_unsigned())
		{
			return KZ::API::ParseError("course object `id` property is not an integer");
		}

		course.id = json["id"];

		if (json.contains("name"))
		{
			if (!json["name"].is_string())
			{
				return KZ::API::ParseError("course object `name` property is not a string");
			}

			course.name = json["name"];
		}

		if (json.contains("description"))
		{
			if (!json["description"].is_string())
			{
				return KZ::API::ParseError("course object `description` property is not a string");
			}

			course.description = json["description"];
		}

		if (json.contains("mappers"))
		{
			if (!json["mappers"].is_array())
			{
				return KZ::API::ParseError("map object `mappers` property is not an array");
			}

			for (const auto &rawMapper : json["mappers"])
			{
				KZ::API::Player mapper;

				if (auto error = KZ::API::Player::Deserialize(rawMapper, mapper))
				{
					return error;
				}

				course.mappers.push_back(mapper);
			}
		}

		if (json.contains("filters"))
		{
			if (!json["filters"].is_array())
			{
				return KZ::API::ParseError("map object `filters` property is not an array");
			}

			for (const auto &rawfilter : json["filters"])
			{
				Map::Course::Filter filter;

				if (auto error = Map::Course::Filter::Deserialize(rawfilter, filter))
				{
					return error;
				}

				course.filters.push_back(filter);
			}
		}

		return std::nullopt;
	}

	std::optional<KZ::API::ParseError> Map::Course::Filter::Deserialize(const json &json, Map::Course::Filter &filter)
	{
		if (!json.is_object())
		{
			return KZ::API::ParseError("course filter is not an object");
		}

		if (!json.contains("id"))
		{
			return KZ::API::ParseError("filter object is missing `id` property");
		}

		if (!json["id"].is_number_unsigned())
		{
			return KZ::API::ParseError("filter object `id` property is not an integer");
		}

		filter.id = json["id"];

		if (!json.contains("mode"))
		{
			return KZ::API::ParseError("filter object is missing `mode` property");
		}

		if (auto error = DeserializeMode(json["mode"], filter.mode))
		{
			return error;
		}

		if (!json.contains("tier"))
		{
			return KZ::API::ParseError("filter object is missing `tier` property");
		}

		if (auto error = Map::Course::Filter::DeserializeTier(json["tier"], filter.tier))
		{
			return error;
		}

		if (!json.contains("ranked_status"))
		{
			return KZ::API::ParseError("filter object is missing `ranked_status` property");
		}

		if (auto error = Map::Course::Filter::DeserializeRankedStatus(json["ranked_status"], filter.rankedStatus))
		{
			return error;
		}

		if (json.contains("notes"))
		{
			if (!json["notes"].is_string())
			{
				return KZ::API::ParseError("filter object `notes` property is not a string");
			}

			filter.notes = json["notes"];
		}

		return std::nullopt;
	}

	std::optional<KZ::API::ParseError> Map::Course::Filter::DeserializeRankedStatus(const json &json, Map::Course::Filter::RankedStatus &rankedStatus)
	{
		if (!json.is_string())
		{
			return KZ::API::ParseError("ranked status is not a string");
		}

		std::string value = json;

		if (value == "never")
		{
			rankedStatus = Map::Course::Filter::RankedStatus::NEVER;
		}
		else if (value == "unranked")
		{
			rankedStatus = Map::Course::Filter::RankedStatus::UNRANKED;
		}
		else if (value == "ranked")
		{
			rankedStatus = Map::Course::Filter::RankedStatus::RANKED;
		}
		else
		{
			return KZ::API::ParseError("ranked status has unknown value");
		}

		return std::nullopt;
	}

	std::optional<KZ::API::ParseError> Map::Course::Filter::DeserializeTier(const json &json, Map::Course::Filter::Tier &tier)
	{
		if (!json.is_string())
		{
			return KZ::API::ParseError("tier is not a string");
		}

		std::string value = json;

		if (value == "very_easy")
		{
			tier = Tier::VERY_EASY;
		}
		else if (value == "easy")
		{
			tier = Tier::EASY;
		}
		else if (value == "medium")
		{
			tier = Tier::MEDIUM;
		}
		else if (value == "advanced")
		{
			tier = Tier::ADVANCED;
		}
		else if (value == "hard")
		{
			tier = Tier::HARD;
		}
		else if (value == "very_hard")
		{
			tier = Tier::VERY_HARD;
		}
		else if (value == "extreme")
		{
			tier = Tier::EXTREME;
		}
		else if (value == "death")
		{
			tier = Tier::DEATH;
		}
		else if (value == "unfeasible")
		{
			tier = Tier::UNFEASIBLE;
		}
		else if (value == "impossible")
		{
			tier = Tier::IMPOSSIBLE;
		}
		else
		{
			return KZ::API::ParseError("tier has unknown value");
		}

		return std::nullopt;
	}
} // namespace KZ::API
