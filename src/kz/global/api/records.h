#pragma once

#include "common.h"

#include "kz/global/api/api.h"
#include "kz/global/api/players.h"
#include "kz/global/api/maps.h"

namespace KZ::API
{
	struct Record
	{
		u32 id;
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

		bool FromJson(const Json &json)
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
	};
} // namespace KZ::API
