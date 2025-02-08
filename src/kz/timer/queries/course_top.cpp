#include "base_request.h"
#include "kz/timer/kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"

#include "utils/simplecmds.h"

#include "vendor/sql_mm/src/public/sql_mm.h"
// clang-format off
#define COURSE_TOP_TABLE_KEY "Course Top - Table Name (Server Overall)"
static_global const char *columnKeysLocal[] = {
	"#",
	"Course Top Header - Player Alias",
	"Course Top Header - Time",
	"Course Top Header - Teleports",
	"Course Top Header - SteamID64",
	"Course Top Header - Run ID"
};

#define COURSE_TOP_PRO_TABLE_KEY "Course Top - Table Name (Server Pro)"
static_global const char *columnKeysLocalPro[] = {
	"#",
	"Course Top Header - Player Alias",
	"Course Top Header - Time",
	"Course Top Header - SteamID64",
	"Course Top Header - Run ID"
};

#define COURSE_TOP_TABLE_KEY_GLOBAL "Course Top - Table Name (Global Overall)"
static_global const char *columnKeysGlobal[] = {
	"#",
	"Course Top Header - Player Alias",
	"Course Top Header - Time",
	"Course Top Header - Teleports",
	"Course Top Header - SteamID64",
	"Course Top Header - Points",
	"Course Top Header - Run ID"
};

#define COURSE_TOP_PRO_TABLE_KEY_GLOBAL "Course Top - Table Name (Global Pro)"
static_global const char *columnKeysGlobalPro[] = {
	"#",
	"Course Top Header - Player Alias",
	"Course Top Header - Time",
	"Course Top Header - SteamID64",
	"Course Top Header - Points",
	"Course Top Header - Run ID",
};

// clang-format on

struct CourseTopRequest : public BaseRequest
{
	using BaseRequest::BaseRequest;

	static constexpr u64 ctopFeatures =
		RequestFeature::Course | RequestFeature::Map | RequestFeature::Mode | RequestFeature::Offset | RequestFeature::Limit;

	struct RunStats
	{
		u64 runID;
		CUtlString name;
		u64 teleportsUsed;
		f64 time;
		u64 steamid64;
		u64 points {}; // 0 for local database

		CUtlString GetRunID()
		{
			CUtlString fmt;
			fmt.Format("%llu", runID);
			return fmt;
		}

		CUtlString GetTeleportCount()
		{
			CUtlString fmt;
			fmt.Format("%llu", teleportsUsed);
			return fmt;
		}

		CUtlString GetTime()
		{
			return KZTimerService::FormatTime(time);
		}

		CUtlString GetSteamID64()
		{
			CUtlString fmt;
			fmt.Format("%llu", steamid64);
			return fmt;
		}

		CUtlString GetPoints()
		{
			CUtlString fmt;
			fmt.Format("%llu", points);
			return fmt;
		}
	};

	struct CourseTopData
	{
		CUtlVector<RunStats> overallData;
		CUtlVector<RunStats> proData;
	} srData, wrData;

	virtual void PrintInstructions() override
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
		if (!player)
		{
			return;
		}
		player->languageService->PrintChat(true, false, "Course Top Command Usage");
		player->languageService->PrintConsole(false, false, "Course Top Command Usage - Console");
	}

	virtual void QueryLocal()
	{
		if (this->requestingFirstCourse)
		{
			return;
		}
		if (this->localStatus == ResponseStatus::ENABLED)
		{
			this->localStatus = ResponseStatus::PENDING;

			KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
			if (!callingPlayer)
			{
				this->Invalidate();
				return;
			}

			this->localStatus = ResponseStatus::PENDING;

			u64 uid = this->uid;

			auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
			{
				CourseTopRequest *req = (CourseTopRequest *)CourseTopRequest::Find(uid);
				if (!req)
				{
					return;
				}

				req->localStatus = ResponseStatus::RECEIVED;
				ISQLResult *result = queries[0]->GetResultSet();
				if (result && result->GetRowCount() > 0)
				{
					while (result->FetchRow())
					{
						req->srData.overallData.AddToTail({(u64)result->GetInt64(0), result->GetString(2), (u64)result->GetInt64(4),
														   result->GetFloat(3), (u64)result->GetInt64(1)});
					}
				}

				if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
				{
					while (result->FetchRow())
					{
						req->srData.proData.AddToTail(
							{(u64)result->GetInt64(0), result->GetString(2), 0, result->GetFloat(3), (u64)result->GetInt64(1)});
					}
				}
			};

			auto onQueryFailure = [uid](std::string, int)
			{
				CourseTopRequest *req = (CourseTopRequest *)CourseTopRequest::Find(uid);
				if (req)
				{
					req->localStatus = ResponseStatus::DISABLED;
				}
			};

			KZDatabaseService::QueryRecords(this->mapName, this->courseName, this->localModeID, this->limit, this->offset, onQuerySuccess,
											onQueryFailure);
		}
	}

	virtual void QueryGlobal()
	{
		if (this->requestingFirstCourse)
		{
			return;
		}

		if (this->globalStatus == ResponseStatus::ENABLED)
		{
			auto callback = [uid = this->uid](const KZ::API::events::CourseTop &ctops)
			{
				CourseTopRequest *req = (CourseTopRequest *)CourseTopRequest::Find(uid);
				if (!req)
				{
					return;
				}

				if (ctops.map.has_value() && ctops.course.has_value())
				{
					req->mapName = ctops.map->name.c_str();
					req->courseName = ctops.course->name.c_str();
					req->globalStatus = ResponseStatus::RECEIVED;
				}
				else
				{
					req->globalStatus = ResponseStatus::DISABLED;
					return;
				}

				for (const auto &record : ctops.overall)
				{
					u64 steamID64 = 0;

					if (!utils::ParseSteamID2(record.player.id.c_str(), steamID64))
					{
						META_CONPRINTF("[KZ] Failed to parse SteamID :(\n");
						continue;
					}

					req->wrData.overallData.AddToTail(
						{record.id, record.player.name.c_str(), record.teleports, record.time, steamID64, (u64)floor(record.nubPoints)});
				}
				for (const auto &record : ctops.pro)
				{
					u64 steamID64 = 0;

					if (!utils::ParseSteamID2(record.player.id.c_str(), steamID64))
					{
						META_CONPRINTF("[KZ] Failed to parse SteamID :(\n");
						continue;
					}

					req->wrData.proData.AddToTail({record.id, record.player.name.c_str(), 0, record.time, steamID64, (u64)floor(record.proPoints)});
				}
			};
			this->globalStatus = ResponseStatus::PENDING;
			KZGlobalService::QueryCourseTop(this->mapName, this->courseName, this->apiMode, this->limit, this->offset, callback);
		}
	}

	virtual void Reply()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}

		if (localStatus != ResponseStatus::RECEIVED && globalStatus != ResponseStatus::RECEIVED)
		{
			player->languageService->PrintChat(true, false, "Course Top Request - Failed (Generic)");
			return;
		}

		player->languageService->PrintChat(true, false, "Course Top - Check Console");
		if (this->localStatus == ResponseStatus::RECEIVED)
		{
			this->ReplyLocal();
		}
		if (this->globalStatus == ResponseStatus::RECEIVED)
		{
			this->ReplyGlobal();
		}
	}

	void ReplyGlobal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		CUtlString headers[Q_ARRAYSIZE(columnKeysGlobal)];
		for (u32 i = 0; i < Q_ARRAYSIZE(columnKeysGlobal); i++)
		{
			headers[i] = player->languageService->PrepareMessage(columnKeysGlobal[i]).c_str();
		}
		CUtlString headersPro[Q_ARRAYSIZE(columnKeysGlobalPro)];
		for (u32 i = 0; i < Q_ARRAYSIZE(columnKeysGlobalPro); i++)
		{
			headersPro[i] = player->languageService->PrepareMessage(columnKeysGlobalPro[i]).c_str();
		}
		utils::DualTable<Q_ARRAYSIZE(columnKeysGlobal), Q_ARRAYSIZE(columnKeysGlobalPro)> dualTable(
			player->languageService->PrepareMessage(COURSE_TOP_TABLE_KEY_GLOBAL, mapName.Get(), courseName.Get(), modeName.Get()).c_str(), headers,
			player->languageService->PrepareMessage(COURSE_TOP_PRO_TABLE_KEY_GLOBAL, mapName.Get(), courseName.Get(), modeName.Get()).c_str(),
			headersPro);
		CUtlString rank;
		FOR_EACH_VEC(wrData.overallData, i)
		{
			rank.Format("%llu", this->offset + i + 1);
			RunStats stats = wrData.overallData[i];
			dualTable.left.SetRow(i, rank, stats.name, stats.GetTime(), stats.GetTeleportCount(), stats.GetSteamID64(), stats.GetPoints(),
								  stats.GetRunID());
		}
		FOR_EACH_VEC(wrData.proData, i)
		{
			rank.Format("%llu", this->offset + i + 1);
			RunStats stats = wrData.proData[i];
			dualTable.right.SetRow(i, rank, stats.name, stats.GetTime(), stats.GetSteamID64(), stats.GetPoints(), stats.GetRunID());
		}
		player->PrintConsole(false, false, dualTable.GetTitle());
		player->PrintConsole(false, false, dualTable.GetHeader());
		player->PrintConsole(false, false, dualTable.GetSeparator());
		for (u32 i = 0; i < dualTable.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, dualTable.GetLine(i));
		}
	}

	void ReplyLocal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		CUtlString headers[Q_ARRAYSIZE(columnKeysLocal)];
		for (u32 i = 0; i < Q_ARRAYSIZE(columnKeysLocal); i++)
		{
			headers[i] = player->languageService->PrepareMessage(columnKeysLocal[i]).c_str();
		}
		CUtlString headersPro[Q_ARRAYSIZE(columnKeysLocalPro)];
		for (u32 i = 0; i < Q_ARRAYSIZE(columnKeysLocalPro); i++)
		{
			headersPro[i] = player->languageService->PrepareMessage(columnKeysLocalPro[i]).c_str();
		}
		utils::DualTable<Q_ARRAYSIZE(columnKeysLocal), Q_ARRAYSIZE(columnKeysLocalPro)> dualTable(
			player->languageService->PrepareMessage(COURSE_TOP_TABLE_KEY, mapName.Get(), courseName.Get(), modeName.Get()).c_str(), headers,
			player->languageService->PrepareMessage(COURSE_TOP_PRO_TABLE_KEY, mapName.Get(), courseName.Get(), modeName.Get()).c_str(), headersPro);
		CUtlString rank;
		FOR_EACH_VEC(srData.overallData, i)
		{
			rank.Format("%llu", this->offset + i + 1);
			RunStats stats = srData.overallData[i];
			dualTable.left.SetRow(i, rank, stats.name, stats.GetTime(), stats.GetTeleportCount(), stats.GetSteamID64(), stats.GetRunID());
		}
		FOR_EACH_VEC(srData.proData, i)
		{
			rank.Format("%llu", this->offset + i + 1);
			RunStats stats = srData.proData[i];
			dualTable.right.SetRow(i, rank, stats.name, stats.GetTime(), stats.GetSteamID64(), stats.GetRunID());
		}
		player->PrintConsole(false, false, dualTable.GetTitle());
		player->PrintConsole(false, false, dualTable.GetHeader());
		player->PrintConsole(false, false, dualTable.GetSeparator());
		for (u32 i = 0; i < dualTable.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, dualTable.GetLine(i));
		}
	}
};

SCMD_CALLBACK(CommandKZCourseTop)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	CourseTopRequest::Create<CourseTopRequest>(player, CourseTopRequest::ctopFeatures, true, true, args);
	return MRES_SUPERCEDE;
}

SCMD_CALLBACK(CommandKZGlobalCourseTop)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	CourseTopRequest::Create<CourseTopRequest>(player, CourseTopRequest::ctopFeatures, false, true, args);
	return MRES_SUPERCEDE;
}

SCMD_CALLBACK(CommandKZServerCourseTop)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	CourseTopRequest::Create<CourseTopRequest>(player, CourseTopRequest::ctopFeatures, true, false, args);
	return MRES_SUPERCEDE;
}

void KZTimerService::RegisterCourseTopCommands()
{
	scmd::RegisterCmd("kz_ctop", CommandKZCourseTop);
	scmd::RegisterCmd("kz_coursetop", CommandKZCourseTop);
	scmd::RegisterCmd("kz_maptop", CommandKZCourseTop);
	scmd::RegisterCmd("kz_gctop", CommandKZGlobalCourseTop);
	scmd::RegisterCmd("kz_gcoursetop", CommandKZGlobalCourseTop);
	scmd::RegisterCmd("kz_gmaptop", CommandKZGlobalCourseTop);
	scmd::RegisterCmd("kz_sctop", CommandKZServerCourseTop);
	scmd::RegisterCmd("kz_scoursetop", CommandKZServerCourseTop);
	scmd::RegisterCmd("kz_smaptop", CommandKZServerCourseTop);
}
