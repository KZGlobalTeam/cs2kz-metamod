#pragma once

#include "common.h"
#include "utils/json.h"

#include "kz/global/api/players.h"

namespace KZ::API
{
	struct Map
	{
		enum class State : i8
		{
			Invalid = -1,
			InTesting = 0,
			Approved = 1,
		};

		struct Course
		{
			struct Filter
			{
				enum class Tier : u8
				{
					VeryEasy = 1,
					Easy = 2,
					Medium = 3,
					Advanced = 4,
					Hard = 5,
					VeryHard = 6,
					Extreme = 7,
					Death = 8,
					Unfeasible = 9,
					Impossible = 10,
				};

				enum class State : i8
				{
					Unranked = -1,
					Pending = 0,
					Ranked = 1,
				};

				u16 id;
				Tier nubTier;
				Tier proTier;
				State state;
				std::optional<std::string> notes {};

				bool FromJson(const Json &json)
				{
					if (!json.Get("id", this->id))
					{
						return false;
					}

					std::string nubTier = "";

					if (!json.Get("nub_tier", nubTier))
					{
						return false;
					}

					if (!DecodeTierString(nubTier, this->nubTier))
					{
						return false;
					}

					std::string proTier = "";

					if (!json.Get("pro_tier", proTier))
					{
						return false;
					}

					if (!DecodeTierString(proTier, this->proTier))
					{
						return false;
					}

					std::string state = "";

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

				static bool DecodeTierString(const std::string &tierString, Tier &tier);
				static bool DecodeStateString(const std::string &stateString, State &state);
			};

			struct Filters
			{
				Filter vanilla;
				Filter classic;

				bool FromJson(const Json &json)
				{
					if (!json.Get<Filter>("vanilla", this->vanilla))
					{
						return false;
					}

					if (!json.Get<Filter>("classic", this->classic))
					{
						return false;
					}

					return true;
				}
			};

			u16 id;
			std::string name;
			std::optional<std::string> description {};
			std::vector<PlayerInfo> mappers {};
			Filters filters;

			bool FromJson(const Json &json)
			{
				if (!json.Get("id", this->id))
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

				if (!json.Get("mappers", this->mappers))
				{
					return false;
				}

				if (!json.Get<Filters>("filters", this->filters))
				{
					return false;
				}

				return true;
			}
		};

		u16 id;
		u32 workshopId;
		std::string name;
		std::optional<std::string> description {};
		State state;
		std::string vpkChecksum;
		std::vector<PlayerInfo> mappers {};
		std::vector<Course> courses {};
		std::string approvedAt;

		bool FromJson(const Json &json)
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

			std::string state = "";

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

		static bool DecodeStateString(const std::string &stateString, State &state);
	};

	struct MapInfo
	{
		u16 id;
		std::string name;

		bool FromJson(const Json &json)
		{
			if (!json.Get("id", this->id))
			{
				return false;
			}

			if (!json.Get("name", this->name))
			{
				return false;
			}

			return true;
		}
	};

	struct CourseInfo
	{
		u16 id;
		std::string name;

		bool FromJson(const Json &json)
		{
			if (!json.Get("id", this->id))
			{
				return false;
			}

			if (!json.Get("name", this->name))
			{
				return false;
			}

			return true;
		}
	};
} // namespace KZ::API
