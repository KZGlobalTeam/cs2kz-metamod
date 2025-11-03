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
		CUtlString runID;
		CUtlString name;
		u64 teleportsUsed;
		f64 time;
		u64 steamid64;
		u64 points {}; // 0 for local database

		CUtlString GetName()
		{
			return name;
		}

		CUtlString GetRunID()
		{
			return runID;
		}

		CUtlString GetTeleportCount()
		{
			CUtlString fmt;
			fmt.Format("%llu", teleportsUsed);
			return fmt;
		}

		CUtlString GetTime()
		{
			return utils::FormatTime(time);
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
						req->srData.overallData.AddToTail(
							{result->GetString(0), result->GetString(2), (u64)result->GetInt64(4), result->GetFloat(3), (u64)result->GetInt64(1)});
					}
				}

				if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
				{
					while (result->FetchRow())
					{
						req->srData.proData.AddToTail({result->GetString(0), result->GetString(2), 0, result->GetFloat(3), (u64)result->GetInt64(1)});
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
			auto callback = [uid = this->uid](KZ::API::events::CourseTop &ctops)
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
					CUtlString id;
					id.Format("%llu", record.id);
					req->wrData.overallData.AddToTail(
						{id, record.player.name.c_str(), record.teleports, record.time, record.player.id, (u64)floor(record.nubPoints)});
				}
				for (const auto &record : ctops.pro)
				{
					CUtlString id;
					id.Format("%llu", record.id);
					req->wrData.proData.AddToTail({id, record.player.name.c_str(), 0, record.time, record.player.id, (u64)floor(record.proPoints)});
				}
			};
			this->globalStatus = ResponseStatus::PENDING;
			KZGlobalService::QueryCourseTop(std::string_view(this->mapName.Get(), this->mapName.Length()),
											std::string_view(this->courseName.Get(), this->courseName.Length()), this->apiMode, this->limit,
											this->offset, callback);
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
		CUtlString rank;

		// Overall table
		CUtlString headers[KZ_ARRAYSIZE(columnKeysGlobal)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeysGlobal); i++)
		{
			headers[i] = player->languageService->PrepareMessage(columnKeysGlobal[i]).c_str();
		}
		utils::Table<KZ_ARRAYSIZE(columnKeysGlobal)> table(
			player->languageService->PrepareMessage(COURSE_TOP_TABLE_KEY_GLOBAL, mapName.Get(), courseName.Get(), modeName.Get()).c_str(), headers);
		FOR_EACH_VEC(wrData.overallData, i)
		{
			rank.Format("%llu", this->offset + i + 1);
			RunStats stats = wrData.overallData[i];
			table.SetRow(i, rank, stats.GetName(), stats.GetTime(), stats.GetTeleportCount(), stats.GetSteamID64(), stats.GetPoints(),
						 stats.GetRunID());
		}
		player->PrintConsole(false, false, table.GetSeparator("="));
		player->PrintConsole(false, false, table.GetTitle());
		player->PrintConsole(false, false, table.GetHeader());

		for (u32 i = 0; i < table.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, table.GetLine(i));
		}
		player->PrintConsole(false, false, table.GetSeparator("="));

		// Pro table
		CUtlString headersPro[KZ_ARRAYSIZE(columnKeysGlobalPro)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeysGlobalPro); i++)
		{
			headersPro[i] = player->languageService->PrepareMessage(columnKeysGlobalPro[i]).c_str();
		}
		utils::Table<KZ_ARRAYSIZE(columnKeysGlobalPro)> tablePro(
			player->languageService->PrepareMessage(COURSE_TOP_PRO_TABLE_KEY_GLOBAL, mapName.Get(), courseName.Get(), modeName.Get()).c_str(),
			headersPro);
		FOR_EACH_VEC(wrData.proData, i)
		{
			rank.Format("%llu", this->offset + i + 1);
			RunStats stats = wrData.proData[i];
			tablePro.SetRow(i, rank, stats.GetName(), stats.GetTime(), stats.GetSteamID64(), stats.GetPoints(), stats.GetRunID());
		}
		player->PrintConsole(false, false, tablePro.GetSeparator("="));
		player->PrintConsole(false, false, tablePro.GetTitle());
		player->PrintConsole(false, false, tablePro.GetHeader());
		for (u32 i = 0; i < tablePro.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, tablePro.GetLine(i));
		}
		player->PrintConsole(false, false, tablePro.GetSeparator("="));
	}

	void ReplyLocal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		CUtlString rank;

		// Overall table
		CUtlString headers[KZ_ARRAYSIZE(columnKeysLocal)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeysLocal); i++)
		{
			headers[i] = player->languageService->PrepareMessage(columnKeysLocal[i]).c_str();
		}
		utils::Table<KZ_ARRAYSIZE(columnKeysLocal)> table(
			player->languageService->PrepareMessage(COURSE_TOP_TABLE_KEY, mapName.Get(), courseName.Get(), modeName.Get()).c_str(), headers);
		FOR_EACH_VEC(srData.overallData, i)
		{
			rank.Format("%llu", this->offset + i + 1);
			RunStats stats = srData.overallData[i];
			table.SetRow(i, rank, stats.GetName(), stats.GetTime(), stats.GetTeleportCount(), stats.GetSteamID64(), stats.GetRunID());
		}
		player->PrintConsole(false, false, table.GetSeparator("="));
		player->PrintConsole(false, false, table.GetTitle());
		player->PrintConsole(false, false, table.GetHeader());

		for (u32 i = 0; i < table.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, table.GetLine(i));
		}
		player->PrintConsole(false, false, table.GetSeparator("="));
		// Pro table
		CUtlString headersPro[KZ_ARRAYSIZE(columnKeysLocalPro)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeysLocalPro); i++)
		{
			headersPro[i] = player->languageService->PrepareMessage(columnKeysLocalPro[i]).c_str();
		}
		utils::Table<KZ_ARRAYSIZE(columnKeysLocalPro)> tablePro(
			player->languageService->PrepareMessage(COURSE_TOP_PRO_TABLE_KEY, mapName.Get(), courseName.Get(), modeName.Get()).c_str(), headersPro);
		FOR_EACH_VEC(srData.proData, i)
		{
			rank.Format("%llu", this->offset + i + 1);
			RunStats stats = srData.proData[i];
			tablePro.SetRow(i, rank, stats.GetName(), stats.GetTime(), stats.GetSteamID64(), stats.GetRunID());
		}
		player->PrintConsole(false, false, tablePro.GetSeparator("="));
		player->PrintConsole(false, false, tablePro.GetTitle());
		player->PrintConsole(false, false, tablePro.GetHeader());
		for (u32 i = 0; i < tablePro.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, tablePro.GetLine(i));
		}
		player->PrintConsole(false, false, tablePro.GetSeparator("="));
	}
};

SCMD(kz_ctop, SCFL_RECORD | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	CourseTopRequest::Create<CourseTopRequest>(player, CourseTopRequest::ctopFeatures, true, true, args);
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_coursetop, kz_ctop);
SCMD_LINK(kz_maptop, kz_ctop);

SCMD(kz_gctop, SCFL_RECORD | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	CourseTopRequest::Create<CourseTopRequest>(player, CourseTopRequest::ctopFeatures, false, true, args);
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_gcoursetop, kz_gctop);
SCMD_LINK(kz_gmaptop, kz_gctop);

SCMD(kz_sctop, SCFL_TIMER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	CourseTopRequest::Create<CourseTopRequest>(player, CourseTopRequest::ctopFeatures, true, false, args);
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_scoursetop, kz_sctop);
SCMD_LINK(kz_smaptop, kz_sctop);
