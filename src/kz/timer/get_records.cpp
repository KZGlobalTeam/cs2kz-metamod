#include "kz/course/kz_course.h"
#include "kz/db/kz_db.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"
#include "utils/argparse.h"
#include "utils/simplecmds.h"
#include "iserver.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

static_global const char *paramKeys[] = {"c", "course", "mode", "map"};

struct RecordData
{
	bool hasRecord {};
	CUtlString holder;
	f32 runTime {};
	u32 teleportsUsed {};

	bool hasRecordPro {};
	CUtlString holderPro;
	f32 runTimePro {};
};

#define WR_WAIT_THRESHOLD 3.0f

/*
	Every time the player calls !wr/!sr, a request struct is created and stored in a vector.
	The request queries all the information required in order to make a request to the database,
	waits for responses from local and global database, and print a coherent result message to the player.
*/
// TODO: Error messages
struct RecordRequest
{
	RecordRequest() : userID(0) {}

	RecordRequest(u64 uid, KZPlayer *callingPlayer, CUtlString mapName, CUtlString courseName, CUtlString modeName, bool serverOnly)
		: uid(uid), userID(callingPlayer->GetClient()->GetUserID()), mapName(mapName), courseName(courseName), modeName(modeName)
	{
		timestamp = g_pKZUtils->GetServerGlobals()->realtime;
		localRequestState = KZDatabaseService::IsReady() ? LocalRequestState::ENABLED : LocalRequestState::DISABLED;
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

	RecordData srData, wrData;

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

	void UpdateSRData(RecordData data)
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
		bool timedOut = g_pKZUtils->GetServerGlobals()->realtime - timestamp > WR_WAIT_THRESHOLD;
		return timedOut || localRequestState >= LocalRequestState::ENABLED;
	}

	bool ShouldReply()
	{
		bool localReady = (localRequestState == LocalRequestState::FINISHED || localRequestState <= LocalRequestState::DISABLED);
		bool globalReady = true;
		bool timedOut = g_pKZUtils->GetServerGlobals()->realtime - timestamp > WR_WAIT_THRESHOLD;
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
	CUtlVector<RecordRequest> recordRequests;
	u32 pbReqCount = 0;

	void AddRequest(KZPlayer *callingPlayer, CUtlString mapName, CUtlString courseName, CUtlString modeName, bool serverOnly)
	{
		RecordRequest *req = recordRequests.AddToTailGetPtr();
		*req = RecordRequest(pbReqCount, callingPlayer, mapName, courseName, modeName, serverOnly);
		req->SetupCourse(callingPlayer);
		req->SetupMode(callingPlayer);
		pbReqCount++;
	}

	template<typename... Args>
	void InvalidLocal(u64 uid, CUtlString reason = "", Args &&...args)
	{
		FOR_EACH_VEC(recordRequests, i)
		{
			if (recordRequests[i].uid == uid)
			{
				recordRequests[i].localRequestState = RecordRequest::LocalRequestState::FAILED;
				// TODO: check for global.
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(recordRequests[i].userID);
				if (player && player->IsInGame())
				{
					player->languageService->PrintChat(true, false, reason.IsEmpty() ? "Record Request - Failed (Generic)" : reason.Get(), args...);
				}
				return;
			}
		}
	}

	void UpdateCourseName(u64 uid, CUtlString name)
	{
		FOR_EACH_VEC(recordRequests, i)
		{
			if (recordRequests[i].uid == uid)
			{
				recordRequests[i].courseName = name;
				recordRequests[i].hasValidCourseName = true;
				return;
			}
		}
	}

	void UpdateSRData(u64 uid, RecordData data)
	{
		FOR_EACH_VEC(recordRequests, i)
		{
			if (recordRequests[i].uid == uid)
			{
				recordRequests[i].UpdateSRData(data);
				return;
			}
		}
	}

	void CheckRequests()
	{
		FOR_EACH_VEC(recordRequests, i)
		{
			if (recordRequests[i].ShouldQueryLocal())
			{
				recordRequests[i].ExecuteLocalRequest();
			}
			if (recordRequests[i].ShouldReply())
			{
				recordRequests[i].Reply();
				recordRequests.Remove(i);
				i--;
				continue;
			}
			if (!recordRequests[i].IsValid())
			{
				recordRequests.Remove(i);
				i--;
				continue;
			}
		}
	}
} recReqQueueManager;

void RecordRequest::SetupMode(KZPlayer *callingPlayer)
{
	if (this->localRequestState <= LocalRequestState::DISABLED)
	{
		return;
	}

	KZModeManager::ModePluginInfo modeInfo =
		this->modeName.IsEmpty() ? KZ::mode::GetModeInfo(callingPlayer->modeService) : KZ::mode::GetModeInfo(modeName);

	if (modeInfo.databaseID < 0)
	{
		recReqQueueManager.InvalidLocal(this->uid);
		return;
	}

	// Change the mode name to the right one for later usage.
	this->modeName = modeInfo.shortModeName;

	this->localDBRequestParams.hasValidModeID = true;
	this->localDBRequestParams.modeID = modeInfo.databaseID;
}

void RecordRequest::SetupCourse(KZPlayer *callingPlayer)
{
	if (this->localRequestState <= LocalRequestState::DISABLED)
	{
		return;
	}

	bool gotCurrentMap = false;
	CUtlString currentMap = g_pKZUtils->GetCurrentMapName(&gotCurrentMap);
	if (!gotCurrentMap) // Shouldn't happen.
	{
		recReqQueueManager.InvalidLocal(this->uid);
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
			const KZCourse *course = callingPlayer->timerService->GetCourse();
			if (!course)
			{
				// No course? Take the map's first course.
				course = KZ::course::GetFirstCourse();
				if (!course)
				{
					// TODO: use a better message
					recReqQueueManager.InvalidLocal(this->uid, "Record Request - Invalid Course Name", "");
					return;
				}
			}
			courseName = course->GetName();
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
					recReqQueueManager.UpdateCourseName(uid, result->GetString(0));
				}
				else
				{
					recReqQueueManager.InvalidLocal(uid);
				}
			};

			auto onQueryFailure = [uid](std::string, int) { recReqQueueManager.InvalidLocal(uid); };
			KZDatabaseService::FindFirstCourseByMapName(mapName, onQuerySuccess, onQueryFailure);
		}
	}
	else
	{
		// No empty field, should be valid.
		hasValidCourseName = true;
	}
}

void RecordRequest::ExecuteLocalRequest()
{
	// No player to respond to, don't bother.
	KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
	if (!callingPlayer)
	{
		recReqQueueManager.InvalidLocal(this->uid);
		return;
	}

	localRequestState = LocalRequestState::RUNNING;

	u64 uid = this->uid;

	auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
	{
		RecordData data {};
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
		recReqQueueManager.UpdateSRData(uid, data);
	};

	auto onQueryFailure = [uid](std::string, int) { recReqQueueManager.InvalidLocal(uid); };

	KZDatabaseService::QueryRecords(mapName, courseName, localDBRequestParams.modeID, 1, 0, onQuerySuccess, onQueryFailure);
}

void QueryRecords(CCSPlayerController *controller, const CCommand *args, bool serverOnly = false)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	KeyValues3 params;

	if (!utils::ParseArgsToKV3(args->ArgS(), params, paramKeys, sizeof(paramKeys) / sizeof(paramKeys[0])))
	{
		player->languageService->PrintChat(true, false, "WR/SR Command Usage");
		player->languageService->PrintConsole(false, false, "WR/SR Command Usage - Console");
		return;
	}

	KeyValues3 *kv = NULL;

	CUtlString mapName;
	CUtlString courseName;
	CUtlString modeName;

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

	recReqQueueManager.AddRequest(player, mapName, courseName, modeName, serverOnly);
	return;
}

SCMD_CALLBACK(CommandKZWR)
{
	QueryRecords(controller, args);
	return MRES_SUPERCEDE;
}

SCMD_CALLBACK(CommandKZSR)
{
	QueryRecords(controller, args, true);
	return MRES_SUPERCEDE;
}

void KZ::timer::CheckRecordRequests()
{
	recReqQueueManager.CheckRequests();
}

void KZTimerService::RegisterRecordCommands()
{
	scmd::RegisterCmd("kz_wr", CommandKZWR);
	scmd::RegisterCmd("kz_sr", CommandKZSR);
}
