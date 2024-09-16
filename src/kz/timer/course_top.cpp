#include "kz/db/kz_db.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"
#include "utils/simplecmds.h"
#include "utils/argparse.h"
#include "utils/tables.h"
#include "iserver.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

#define COURSE_TOP_TABLE_KEY "Course Top - Table Name (Overall)"
static_global const char *columnKeys[] = {"#",
										  "Course Top Header - Player Alias",
										  "Course Top Header - Time",
										  "Course Top Header - Teleports",
										  "Course Top Header - SteamID64",
										  "Course Top Header - Run ID"};

#define COURSE_TOP_PRO_TABLE_KEY "Course Top - Table Name (Pro)"
static_global const char *columnKeysPro[] = {"#", "Course Top Header - Player Alias", "Course Top Header - Time", "Course Top Header - SteamID64",
											 "Course Top Header - Run ID"};

static_global const char *paramKeys[] = {"c", "course", "mode", "map", "o", "offset", "l", "limit"};

#define CTOP_WAIT_THRESHOLD 3.0f

struct RunStats
{
	u64 runID;
	CUtlString name;
	u64 teleportsUsed;
	f64 time;
	u64 steamid64;

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
};

struct CourseTopData
{
	CUtlVector<RunStats> overallData;
	CUtlVector<RunStats> proData;
};

#define CTOP_WAIT_THRESHOLD 3.0f

struct CourseTopRequest
{
	CourseTopRequest() : userID(0) {}

	CourseTopRequest(u64 uid, KZPlayer *callingPlayer, CUtlString mapName, CUtlString courseName, CUtlString modeName, u64 limit, u64 offset,
					 bool queryLocal, bool queryGlobal)
		: uid(uid), userID(callingPlayer->GetClient()->GetUserID()), mapName(mapName), courseName(courseName), modeName(modeName), limit(limit),
		  offset(offset)
	{
		timestamp = g_pKZUtils->GetServerGlobals()->realtime;
		localRequestState = queryLocal && KZDatabaseService::IsReady() ? LocalRequestState::ENABLED : LocalRequestState::DISABLED;
	}

	// Global identifier and timestamp for timeout
	u64 uid;
	f32 timestamp;

	// UserID for callback.
	CPlayerUserId userID;

	// Query parameters
	CUtlString mapName;

	bool hasValidCourseName {};
	CUtlString courseName;

	CUtlString modeName;

	u64 limit;
	u64 offset;

	// Local exclusive stuff.
	enum struct LocalRequestState
	{
		FAILED = -1,
		DISABLED,
		ENABLED,
		RUNNING,
		FINISHED
	} localRequestState;

	struct LocalDBRequestParams
	{
		i32 modeID;

		bool hasValidModeID {};

	} localDBRequestParams;

	CourseTopData srData, wrData;

	/*
		Get the player's mode for querying:
		Local:
		- Compute modeID from the list of modes, disable the local query if it fails.
		- If the mode is not global, disable the global query. (TODO)
	*/
	void SetupMode(KZPlayer *callingPlayer);

	// Get the course name and the map name for querying.
	void SetupCourse(KZPlayer *callingPlayer);

	// Returns whether we should query and whether the parameters are ready for querying.
	bool ShouldQueryLocal()
	{
		return localRequestState == LocalRequestState::ENABLED && hasValidCourseName && localDBRequestParams.hasValidModeID;
	}

	// Execute queries to get pb in the local db.
	void ExecuteLocalRequest();

	void MarkLocalDataReceived()
	{
		if (localRequestState == LocalRequestState::RUNNING)
		{
			localRequestState = LocalRequestState::FINISHED;
		}
	}

	// Whether this request should still be looked at or not. Once this returns false, it should be deleted off the list.
	// TODO: Implement this for global!
	bool IsValid()
	{
		bool timedOut = g_pKZUtils->GetServerGlobals()->realtime - timestamp > CTOP_WAIT_THRESHOLD;
		return timedOut || localRequestState >= LocalRequestState::ENABLED;
	}

	bool ShouldReply()
	{
		bool localReady = (localRequestState == LocalRequestState::FINISHED || localRequestState <= LocalRequestState::DISABLED);
		bool globalReady = true;
		bool timedOut = g_pKZUtils->GetServerGlobals()->realtime - timestamp > CTOP_WAIT_THRESHOLD;
		return timedOut || (localReady && globalReady);
	}

	void Reply()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}
		// TODO: How does this work with global?
		if (localRequestState != LocalRequestState::FINISHED)
		{
			// If the request failed, then the error message is already sent in InvalidLocal.
			if (localRequestState != LocalRequestState::FAILED)
			{
				player->languageService->PrintChat(true, false, "Course Top Request - Failed (Generic)");
			}
			return;
		}

		CUtlString headers[Q_ARRAYSIZE(columnKeys)];
		for (u32 i = 0; i < Q_ARRAYSIZE(columnKeys); i++)
		{
			headers[i] = player->languageService->PrepareMessage(columnKeys[i]).c_str();
		}
		CUtlString headersPro[Q_ARRAYSIZE(columnKeysPro)];
		for (u32 i = 0; i < Q_ARRAYSIZE(columnKeysPro); i++)
		{
			headersPro[i] = player->languageService->PrepareMessage(columnKeysPro[i]).c_str();
		}
		utils::DualTable<Q_ARRAYSIZE(columnKeys), Q_ARRAYSIZE(columnKeysPro)> dualTable(
			player->languageService->PrepareMessage(COURSE_TOP_TABLE_KEY, mapName.Get(), courseName.Get(), modeName.Get()).c_str(), headers,
			player->languageService->PrepareMessage(COURSE_TOP_PRO_TABLE_KEY, mapName.Get(), courseName.Get(), modeName.Get()).c_str(), headersPro);
		CUtlString rank;
		FOR_EACH_VEC(srData.overallData, i)
		{
			rank.Format("%i", i + 1);
			RunStats stats = srData.overallData[i];
			dualTable.left.SetRow(i, rank, stats.name, stats.GetTime(), stats.GetTeleportCount(), stats.GetSteamID64(), stats.GetRunID());
		}
		FOR_EACH_VEC(srData.proData, i)
		{
			rank.Format("%i", i + 1);
			RunStats stats = srData.proData[i];
			dualTable.right.SetRow(i, rank, stats.name, stats.GetTime(), stats.GetSteamID64(), stats.GetRunID());
		}
		player->languageService->PrintChat(false, false, "Course Top - Check Console");
		player->PrintConsole(false, false, dualTable.GetTitle());
		player->PrintConsole(false, false, dualTable.GetHeader());
		player->PrintConsole(false, false, dualTable.GetSeparator());
		for (u32 i = 0; i < dualTable.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, dualTable.GetLine(i));
		}
	}
};

static_global struct
{
	CUtlVector<CourseTopRequest> courseTopRequests;
	u32 ctopReqCount = 0;

	void AddRequest(KZPlayer *callingPlayer, CUtlString mapName, CUtlString courseName, CUtlString modeName, u64 limit, u64 offset, bool queryLocal,
					bool queryGlobal)
	{
		CourseTopRequest *req = courseTopRequests.AddToTailGetPtr();
		*req = CourseTopRequest(ctopReqCount, callingPlayer, mapName, courseName, modeName, limit, offset, queryLocal, queryGlobal);
		req->SetupCourse(callingPlayer);
		req->SetupMode(callingPlayer);
		ctopReqCount++;
	}

	template<typename... Args>
	void InvalidLocal(u64 uid, CUtlString reason = "", Args &&...args)
	{
		FOR_EACH_VEC(courseTopRequests, i)
		{
			if (courseTopRequests[i].uid == uid)
			{
				courseTopRequests[i].localRequestState = CourseTopRequest::LocalRequestState::FAILED;
				// TODO: check for global.
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(courseTopRequests[i].userID);
				if (player && player->IsInGame())
				{
					player->languageService->PrintChat(true, false, reason.IsEmpty() ? "Course Top Request - Failed (Generic)" : reason.Get(),
													   args...);
				}
				return;
			}
		}
	}

	void UpdateCourseName(u64 uid, CUtlString name)
	{
		FOR_EACH_VEC(courseTopRequests, i)
		{
			if (courseTopRequests[i].uid == uid)
			{
				courseTopRequests[i].courseName = name;
				courseTopRequests[i].hasValidCourseName = true;
				return;
			}
		}
	}

	CourseTopData *GetRecordData(u64 uid)
	{
		FOR_EACH_VEC(courseTopRequests, i)
		{
			if (courseTopRequests[i].uid == uid)
			{
				return &courseTopRequests[i].srData;
			}
		}
		return nullptr;
	}

	void MarkLocalDataReceived(u64 uid)
	{
		FOR_EACH_VEC(courseTopRequests, i)
		{
			if (courseTopRequests[i].uid == uid)
			{
				courseTopRequests[i].MarkLocalDataReceived();
			}
		}
	}

	void CheckRequests()
	{
		FOR_EACH_VEC(courseTopRequests, i)
		{
			if (courseTopRequests[i].ShouldQueryLocal())
			{
				courseTopRequests[i].ExecuteLocalRequest();
			}
			if (courseTopRequests[i].ShouldReply())
			{
				courseTopRequests[i].Reply();
				courseTopRequests.Remove(i);
				i--;
				continue;
			}
			if (!courseTopRequests[i].IsValid())
			{
				courseTopRequests.Remove(i);
				i--;
				continue;
			}
		}
	}
} ctopReqQueueManager;

void CourseTopRequest::SetupMode(KZPlayer *callingPlayer)
{
	if (this->localRequestState <= LocalRequestState::DISABLED)
	{
		return;
	}

	KZModeManager::ModePluginInfo modeInfo =
		this->modeName.IsEmpty() ? KZ::mode::GetModeInfo(callingPlayer->modeService) : KZ::mode::GetModeInfo(modeName);

	if (modeInfo.databaseID < 0)
	{
		ctopReqQueueManager.InvalidLocal(this->uid);
		return;
	}

	// Change the mode name to the right one for later usage.
	this->modeName = modeInfo.shortModeName;

	this->localDBRequestParams.hasValidModeID = true;
	this->localDBRequestParams.modeID = modeInfo.databaseID;
}

void CourseTopRequest::SetupCourse(KZPlayer *callingPlayer)
{
	if (this->localRequestState <= LocalRequestState::DISABLED)
	{
		return;
	}

	bool gotCurrentMap = false;
	CUtlString currentMap = g_pKZUtils->GetCurrentMapName(&gotCurrentMap);
	if (!gotCurrentMap) // Shouldn't happen.
	{
		ctopReqQueueManager.InvalidLocal(this->uid);
		return;
	}

	if (mapName.IsEmpty())
	{
		this->mapName = currentMap;
	}

	// Map + Course empty => current map, current course (first course if just joined)
	if (courseName.IsEmpty())
	{
		// If it's the current map...
		if (this->mapName == currentMap)
		{
			// Try to get the player's current course.
			char course[KZ_MAX_COURSE_NAME_LENGTH];
			callingPlayer->timerService->GetCourse(course, KZ_MAX_COURSE_NAME_LENGTH);
			if (course[0])
			{
				courseName = course;
			}
			else // No course? Take the map's first course.
			{
				KZ::timer::CourseInfo info;
				if (!KZ::timer::GetFirstCourseInformation(info))
				{
					ctopReqQueueManager.InvalidLocal(this->uid, "Course Top Request - Invalid Course Name", course);
					return;
				}
				courseName = info.courseName;
			}
			hasValidCourseName = true;
		}
		else
		{
			u64 uid = this->uid;
			auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
			{
				ISQLResult *result = queries[0]->GetResultSet();
				if (result->GetRowCount() > 0 && result->FetchRow())
				{
					ctopReqQueueManager.UpdateCourseName(uid, result->GetString(0));
				}
				else
				{
					ctopReqQueueManager.InvalidLocal(uid);
				}
			};

			auto onQueryFailure = [uid](std::string, int) { ctopReqQueueManager.InvalidLocal(uid); };
			KZDatabaseService::FindFirstCourseByMapName(mapName, onQuerySuccess, onQueryFailure);
		}
	}
	else
	{
		// No empty field, should be valid.
		hasValidCourseName = true;
	}
}

void CourseTopRequest::ExecuteLocalRequest()
{
	// No player to respond to, don't bother.
	KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
	if (!callingPlayer)
	{
		ctopReqQueueManager.InvalidLocal(this->uid);
		return;
	}

	localRequestState = LocalRequestState::RUNNING;

	u64 uid = this->uid;

	auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
	{
		CourseTopData *data = ctopReqQueueManager.GetRecordData(uid);
		if (!data)
		{
			return;
		}
		ISQLResult *result = queries[0]->GetResultSet();
		if (result && result->GetRowCount() > 0)
		{
			while (result->FetchRow())
			{
				data->overallData.AddToTail(
					{(u64)result->GetInt64(0), result->GetString(2), (u64)result->GetInt64(4), result->GetFloat(3), (u64)result->GetInt64(1)});
			}
		}

		if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
		{
			while (result->FetchRow())
			{
				data->proData.AddToTail({(u64)result->GetInt64(0), result->GetString(2), 0, result->GetFloat(3), (u64)result->GetInt64(1)});
			}
		}
		ctopReqQueueManager.MarkLocalDataReceived(uid);
	};

	auto onQueryFailure = [uid](std::string, int) { ctopReqQueueManager.InvalidLocal(uid); };

	KZDatabaseService::QueryRecords(mapName, courseName, localDBRequestParams.modeID, limit, offset, onQuerySuccess, onQueryFailure);
}

void QueryCourseTop(CCSPlayerController *controller, const CCommand *args, bool queryLocal = true, bool queryGlobal = true)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	KeyValues3 params;

	if (!utils::ParseArgsToKV3(args->ArgS(), params, paramKeys, sizeof(paramKeys) / sizeof(paramKeys[0])))
	{
		player->languageService->PrintChat(true, false, "Course Top Command Usage");
		player->languageService->PrintConsole(false, false, "Course Top Command Usage - Console");
		return;
	}

	KeyValues3 *kv = NULL;

	CUtlString mapName;
	CUtlString courseName;
	CUtlString modeName;
	u64 limit = 20;
	u64 offset = 0;

	if (kv = params.FindMember("map"))
	{
		mapName = kv->GetString();
	}
	if ((kv = params.FindMember("course")) || (kv = params.FindMember("c")))
	{
		courseName = kv->GetString();
	}
	if (kv = params.FindMember("mode"))
	{
		modeName = kv->GetString();
	}
	if ((kv = params.FindMember("offset")) || (kv = params.FindMember("o")))
	{
		offset = atoll(kv->GetString());
	}
	if ((kv = params.FindMember("limit")) || (kv = params.FindMember("l")))
	{
		limit = atoll(kv->GetString());
	}

	ctopReqQueueManager.AddRequest(player, mapName, courseName, modeName, limit, offset, queryLocal, queryGlobal);
	return;
}

SCMD_CALLBACK(CommandKZCourseTop)
{
	QueryCourseTop(controller, args);
	return MRES_SUPERCEDE;
}

SCMD_CALLBACK(CommandKZGlobalCourseTop)
{
	QueryCourseTop(controller, args);
	return MRES_SUPERCEDE;
}

SCMD_CALLBACK(CommandKZServerCourseTop)
{
	QueryCourseTop(controller, args);
	return MRES_SUPERCEDE;
}

void KZ::timer::CheckCourseTopRequests()
{
	ctopReqQueueManager.CheckRequests();
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
