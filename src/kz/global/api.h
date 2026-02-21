#pragma once

#include "utils/json.h"

namespace KZ::api
{
	enum class Mode : u8
	{
		Vanilla = 1,
		Classic = 2,
	};

	bool DecodeModeString(std::string_view modeString, Mode &mode);

	struct PlayerInfo
	{
		u64 id {};
		std::string name {};

		bool FromJson(const Json &json);
	};

	struct Player
	{
		u64 id {};
		std::string name {};
		bool isBanned {};

		bool FromJson(const Json &json);
	};

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

				bool FromJson(const Json &json);
				static bool DecodeTierString(std::string_view tierString, Tier &tier);
				static bool DecodeStateString(std::string_view stateString, State &state);
			};

			struct Filters
			{
				Filter vanilla;
				Filter classic;

				bool FromJson(const Json &json);
			};

			u16 id;
			std::string name;
			std::optional<std::string> description {};
			std::vector<PlayerInfo> mappers {};
			Filters filters;

			bool FromJson(const Json &json);
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

		bool FromJson(const Json &json);
		static bool DecodeStateString(std::string_view stateString, State &state);
	};

	struct Record
	{
		struct MapInfo
		{
			u16 id;
			std::string name;

			bool FromJson(const Json &json);
		};

		struct CourseInfo
		{
			u16 id;
			std::string name;

			bool FromJson(const Json &json);
		};

		std::string id;
		PlayerInfo player;
		MapInfo map;
		CourseInfo course;
		Mode mode;
		u32 teleports;
		f64 time;
		u32 nubRank = 0;
		f64 nubPoints = -1;
		u32 nubMaxRank = 0;
		u32 proRank = 0;
		f64 proPoints = -1;
		u32 proMaxRank = 0;

		bool FromJson(const Json &json);
	};
}; // namespace KZ::api
