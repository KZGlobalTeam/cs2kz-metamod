#include "kz/db/kz_db.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"
#include "utils/simplecmds.h"
#include "utils/argparse.h"
#include "iserver.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

static_global const char *paramKeys[] = {"c", "course", "mode", "map", "o", "offset", "l", "limit"};

// Worst case scenario: "#123453 123:45:64.752 123456 TPs 01234567890123456789012345678912 <76561197972581267>"
struct RunStats
{
	u64 rank;
	u64 steamid64;
	CUtlString name;
	u64 teleportsUsed;
	f64 time;
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

	CourseTopRequest(u64 uid, KZPlayer *callingPlayer, CUtlString mapName, CUtlString courseName, CUtlString modeName, u64 offset, bool queryLocal,
					 bool queryGlobal)
		: uid(uid), userID(callingPlayer->GetClient()->GetUserID()), mapName(mapName), courseName(courseName), modeName(modeName)
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

	void UpdateSRData(CourseTopData data)
	{
		srData = data;
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
				player->languageService->PrintChat(true, false, "Record Request - Failed (Generic)");
			}
			return;
		}

		// Local stuff
		std::string localTPText;
		if (srData.teleportsUsed > 0)
		{
			localTPText = srData.teleportsUsed == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
													: player->languageService->PrepareMessage("2+ Teleports Text", srData.teleportsUsed);
		}

		char localStandardTime[32];
		KZTimerService::FormatTime(srData.runTime, localStandardTime, sizeof(localStandardTime));
		char localProTime[32];
		KZTimerService::FormatTime(srData.runTimePro, localProTime, sizeof(localProTime));

		// Records on kz_map (Main) [VNL]
		player->languageService->PrintChat(true, false, "SR Header", mapName.Get(), courseName.Get(), modeName.Get());
		if (!srData.hasRecord)
		{
			player->languageService->PrintChat(true, false, "SR - No Times");
		}
		else if (!srData.hasRecordPro)
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "SR - Overall", localStandardTime, srData.holder.Get(), localTPText);
		}
		// Their MAP PB has 0 teleports, and is therefore also their PRO PB
		else if (srData.teleportsUsed == 0)
		{
			// KZ | Overall/PRO Record: 01:23.45 by Bill
			player->languageService->PrintChat(true, false, "SR - Combined", localStandardTime, srData.holder.Get());
		}
		else
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "SR - Overall", localStandardTime, srData.holder.Get(), localTPText);
			// KZ | PRO Record: 23.45 by Player
			player->languageService->PrintChat(true, false, "SR - PRO", localProTime, srData.holderPro.Get());
		}
	}
};

static_global struct
{
	CUtlVector<CourseTopRequest> CourseTopRequests;
	u32 ctopReqCount = 0;

	void AddRequest(KZPlayer *callingPlayer, CUtlString mapName, CUtlString courseName, CUtlString modeName, u64 offset, bool queryLocal,
					bool queryGlobal)
	{
		CourseTopRequest *req = CourseTopRequests.AddToTailGetPtr();
		*req = CourseTopRequest(ctopReqCount, callingPlayer, mapName, courseName, modeName, offset, queryLocal, queryGlobal);
		req->SetupCourse(callingPlayer);
		req->SetupMode(callingPlayer);
		ctopReqCount++;
	}

	template<typename... Args>
	void InvalidLocal(u64 uid, CUtlString reason = "", Args &&...args)
	{
		FOR_EACH_VEC(CourseTopRequests, i)
		{
			if (CourseTopRequests[i].uid == uid)
			{
				CourseTopRequests[i].localRequestState = CourseTopRequest::LocalRequestState::FAILED;
				// TODO: check for global.
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(CourseTopRequests[i].userID);
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
		FOR_EACH_VEC(CourseTopRequests, i)
		{
			if (CourseTopRequests[i].uid == uid)
			{
				CourseTopRequests[i].courseName = name;
				CourseTopRequests[i].hasValidCourseName = true;
				return;
			}
		}
	}

	void UpdateSRData(u64 uid, CourseTopData data)
	{
		FOR_EACH_VEC(CourseTopRequests, i)
		{
			if (CourseTopRequests[i].uid == uid)
			{
				CourseTopRequests[i].UpdateSRData(data);
				return;
			}
		}
	}

	void CheckRequests()
	{
		FOR_EACH_VEC(CourseTopRequests, i)
		{
			if (CourseTopRequests[i].ShouldQueryLocal())
			{
				CourseTopRequests[i].ExecuteLocalRequest();
			}
			if (CourseTopRequests[i].ShouldReply())
			{
				CourseTopRequests[i].Reply();
				CourseTopRequests.Remove(i);
				i--;
				continue;
			}
			if (!CourseTopRequests[i].IsValid())
			{
				CourseTopRequests.Remove(i);
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
					ctopReqQueueManager.InvalidLocal(this->uid, "Record Request - Invalid Course Name", course);
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
		CourseTopData data {};
		ISQLResult *result = queries[0]->GetResultSet();
		if (result && result->GetRowCount() > 0)
		{
			data.hasRecord = true;
			if (result->FetchRow())
			{
				data.holder = result->GetString(2);
				data.runTime = result->GetFloat(3);
				data.teleportsUsed = result->GetInt(4);
			}
		}
		if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
		{
			data.hasRecordPro = true;
			if (result->FetchRow())
			{
				data.holderPro = result->GetString(2);
				data.runTimePro = result->GetFloat(3);
			}
		}
		ctopReqQueueManager.UpdateSRData(uid, data);
	};

	auto onQueryFailure = [uid](std::string, int) { ctopReqQueueManager.InvalidLocal(uid); };

	KZDatabaseService::QueryRecords(mapName, courseName, localDBRequestParams.modeID, onQuerySuccess, onQueryFailure);
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
	u64 offset;

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

	ctopReqQueueManager.AddRequest(player, mapName, courseName, modeName, offset, queryLocal, queryGlobal);
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
