#include "kz/db/kz_db.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"
#include "utils/argparse.h"
#include "utils/simplecmds.h"
#include "iserver.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

static_global const char *paramKeys[] = {"c", "course", "mode", "map", "p", "player", "s", "style"};

struct PBData
{
	bool hasPB {};
	f32 runTime {};
	u32 teleportsUsed {};
	u32 rank {};
	u32 maxRank {};

	bool hasPBPro {};
	f32 runTimePro {};
	u32 rankPro {};
	u32 maxRankPro {};
};

#define PB_WAIT_THRESHOLD 3.0f

/*
	Every time the player calls !pb, a request struct is created and stored in a vector.
	The request queries all the information required in order to make a request to the database,
	waits for responses from local and global database, and print a coherent result message to the player.
*/
// TODO: Error messages
struct PBRequest
{
	PBRequest() : userID(0) {}

	PBRequest(u64 uid, KZPlayer *callingPlayer, CUtlString playerName, CUtlString mapName, CUtlString courseName, CUtlString modeName,
			  CUtlString styleNames)
		: uid(uid), userID(callingPlayer->GetClient()->GetUserID()), targetPlayerName(playerName), mapName(mapName), courseName(courseName),
		  modeName(modeName), styleNames(styleNames)
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
	bool hasValidTarget {};
	CUtlString targetPlayerName;
	u64 targetSteamID64;

	CUtlString mapName;

	bool hasValidCourseName {};
	CUtlString courseName;

	CUtlString modeName;

	// Don't get ranks if styles are involved.
	// ..or target player is banned (TODO)
	bool queryRanking = true;
	CUtlString styleNames;
	CUtlVector<CUtlString> styleList;

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
		u64 styleIDs;

		bool hasValidModeID {};
		bool hasValidStyleIDs {};

		bool HasValidParams()
		{
			return hasValidModeID && hasValidStyleIDs;
		}
	} localDBRequestParams;

	PBData localPBData, globalPBData;

	/*
		Get the player's steamID64 for querying.
		1. No player specified -> default to calling player's steamid
		2. Player specified -> find steamid of player with matching name in the server
		3. No server matching player -> find steamid of player name in the local database
		4. No localdb matching player -> find steamid of player name in the api (TODO)
		5. No matching name in the api -> abort
	*/
	void SetupSteamID64(KZPlayer *callingPlayer);

	/*
		Get the player's mode for querying:
		Local:
		- Compute modeID from the list of modes, disable the local query if it fails.
		- If the mode is not global, disable the global query. (TODO)
	*/
	void SetupMode(KZPlayer *callingPlayer);

	/*
		Get the player's styles for querying:
		Local:
		- Compute styleIDs from the list of modes, disable the local query if it fails.
		- If a style is specified, disable ranking queries.
		- If the style is not global, disable the global query. (TODO)
	*/
	void SetupStyles(KZPlayer *callingPlayer);

	// Get the course name and the map name for querying.
	void SetupCourse(KZPlayer *callingPlayer);

	// Returns whether we should query and whether the parameters are ready for querying.
	bool ShouldQueryLocal()
	{
		return localRequestState == LocalRequestState::ENABLED && hasValidTarget && hasValidCourseName && localDBRequestParams.HasValidParams();
	}

	// Execute queries to get pb in the local db.
	void ExecuteLocalRequest();

	// For non styled runs.
	void ExecuteStandardLocalQuery();

	// For styled runs.
	void ExecuteRanklessLocalQuery();

	void UpdateLocalData(PBData data)
	{
		localPBData = data;
		if (localRequestState == LocalRequestState::RUNNING)
		{
			localRequestState = LocalRequestState::FINISHED;
		}
	}

	// Whether this request should still be looked at or not. Once this returns false, it should be deleted off the list.
	// TODO: Implement this for global!
	bool IsValid()
	{
		bool timedOut = g_pKZUtils->GetServerGlobals()->realtime - timestamp > PB_WAIT_THRESHOLD;
		return timedOut || localRequestState >= LocalRequestState::ENABLED;
	}

	bool ShouldReply()
	{
		bool localReady = (localRequestState == LocalRequestState::FINISHED || localRequestState <= LocalRequestState::DISABLED);
		bool globalReady = true;
		bool timedOut = g_pKZUtils->GetServerGlobals()->realtime - timestamp > PB_WAIT_THRESHOLD;
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
				player->languageService->PrintChat(true, false, "PB Request - Failed (Generic)");
			}
			return;
		}
		CUtlString combinedModeStyleText;
		combinedModeStyleText.Format("{purple}%s{grey}", modeName.Get());
		FOR_EACH_VEC(styleList, i)
		{
			combinedModeStyleText += " +{grey2}";
			combinedModeStyleText.Append(styleList[i].Get());
			combinedModeStyleText += "{grey}";
		}

		// Local stuff
		std::string localTPText;
		if (localPBData.teleportsUsed > 0)
		{
			localTPText = localPBData.teleportsUsed == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
														 : player->languageService->PrepareMessage("2+ Teleports Text", localPBData.teleportsUsed);
		}

		char localOverallTime[32];
		KZTimerService::FormatTime(localPBData.runTime, localOverallTime, sizeof(localOverallTime));
		char localProTime[32];
		KZTimerService::FormatTime(localPBData.runTimePro, localProTime, sizeof(localProTime));

		// Player on kz_map (Main) [VNL]
		player->languageService->PrintChat(true, false, "PB Header", targetPlayerName.Get(), mapName.Get(), courseName.Get(),
										   combinedModeStyleText.Get());
		if (queryRanking)
		{
			if (!localPBData.hasPB)
			{
				player->languageService->PrintChat(true, false, "PB Time - No Times");
			}
			else if (!localPBData.hasPBPro)
			{
				// KZ | Server: 12.34 (5 TPs) [Overall]
				player->languageService->PrintChat(true, false, "PB Time - Overall (Server)", localOverallTime, localTPText, localPBData.rank,
												   localPBData.maxRank);
			}
			// Their MAP PB has 0 teleports, and is therefore also their PRO PB
			else if (localPBData.teleportsUsed == 0)
			{
				// KZ | Server: 12.34 [#1/24 Overall] [#1/2 PRO]
				player->languageService->PrintChat(true, false, "PB Time - Combined (Server)", localOverallTime, localPBData.rank,
												   localPBData.maxRank, localPBData.rankPro, localPBData.maxRankPro);
			}
			else
			{
				// KZ | Server: 12.34 (5 TPs) [#1/24 Overall] | 23.45 [#1/2 PRO]
				player->languageService->PrintChat(true, false, "PB Time - Split (Server)", localOverallTime, localTPText, localPBData.rank,
												   localPBData.maxRank, localProTime, localPBData.rankPro, localPBData.maxRankPro);
			}
		}
		else
		{
			if (!localPBData.hasPB)
			{
				player->languageService->PrintChat(true, false, "PB Time - No Times");
			}
			else if (!localPBData.hasPBPro)
			{
				// KZ | Server: 12.34 (5 TPs) [Overall]
				player->languageService->PrintChat(true, false, "PB Time - Overall Rankless (Server)", localOverallTime, localTPText);
			}
			// Their MAP PB has 0 teleports, and is therefore also their PRO PB
			else if (localPBData.teleportsUsed == 0)
			{
				// KZ | Server: 12.34 [Overall/PRO]
				player->languageService->PrintChat(true, false, "PB Time - Combined Rankless (Server)", localOverallTime);
			}
			else
			{
				// KZ | Server: 12.34 (5 TPs) [Overall] | 23.45 [PRO]
				player->languageService->PrintChat(true, false, "PB Time - Split Rankless (Server)", localOverallTime, localTPText, localProTime);
			}
		}
	}
};

static_global struct
{
	CUtlVector<PBRequest> pbRequests;
	u32 pbReqCount = 0;

	void AddRequest(KZPlayer *callingPlayer, CUtlString playerName, CUtlString mapName, CUtlString courseName, CUtlString modeName,
					CUtlString styleNames)
	{
		PBRequest *req = pbRequests.AddToTailGetPtr();
		*req = PBRequest(pbReqCount, callingPlayer, playerName, mapName, courseName, modeName, styleNames);
		req->SetupSteamID64(callingPlayer);
		req->SetupCourse(callingPlayer);
		req->SetupMode(callingPlayer);
		req->SetupStyles(callingPlayer);
		pbReqCount++;
	}

	template<typename... Args>
	void InvalidLocal(u64 uid, CUtlString reason = "", Args &&...args)
	{
		FOR_EACH_VEC(pbRequests, i)
		{
			if (pbRequests[i].uid == uid)
			{
				pbRequests[i].localRequestState = PBRequest::LocalRequestState::FAILED;
				// TODO: check for global.
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(pbRequests[i].userID);
				if (player && player->IsInGame())
				{
					player->languageService->PrintChat(true, false, reason.IsEmpty() ? "PB Request - Failed (Generic)" : reason.Get(), args...);
				}
				return;
			}
		}
	}

	void SetRequestTargetPlayer(u64 uid, u64 steamID64, CUtlString name)
	{
		FOR_EACH_VEC(pbRequests, i)
		{
			if (pbRequests[i].uid == uid)
			{
				pbRequests[i].hasValidTarget = true;
				pbRequests[i].targetSteamID64 = steamID64;
				pbRequests[i].targetPlayerName = name;
				return;
			}
		}
	}

	void UpdateCourseName(u64 uid, CUtlString name)
	{
		FOR_EACH_VEC(pbRequests, i)
		{
			if (pbRequests[i].uid == uid)
			{
				pbRequests[i].courseName = name;
				pbRequests[i].hasValidCourseName = true;
				return;
			}
		}
	}

	void UpdateLocalPBData(u64 uid, PBData data)
	{
		FOR_EACH_VEC(pbRequests, i)
		{
			if (pbRequests[i].uid == uid)
			{
				pbRequests[i].UpdateLocalData(data);
				return;
			}
		}
	}

	void CheckRequests()
	{
		FOR_EACH_VEC(pbRequests, i)
		{
			if (pbRequests[i].ShouldQueryLocal())
			{
				pbRequests[i].ExecuteLocalRequest();
			}
			if (pbRequests[i].ShouldReply())
			{
				pbRequests[i].Reply();
				pbRequests.Remove(i);
				i--;
				continue;
			}
			if (!pbRequests[i].IsValid())
			{
				pbRequests.Remove(i);
				i--;
				continue;
			}
		}
	}
} pbReqQueueManager;

void PBRequest::SetupSteamID64(KZPlayer *callingPlayer)
{
	if (targetPlayerName.IsEmpty())
	{
		hasValidTarget = true;
		targetPlayerName = callingPlayer->GetName();
		targetSteamID64 = callingPlayer->GetSteamId64();
		return;
	}
	for (u32 i = 1; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		CServerSideClient *client = player->GetClient();
		if (!client)
		{
			continue;
		}
		if (targetPlayerName == client->GetClientName())
		{
			hasValidTarget = true;
			targetPlayerName = player->GetName();
			targetSteamID64 = player->GetSteamId64();
			return;
		}
	}
	if (KZDatabaseService::IsReady())
	{
		u64 uid = this->uid;

		auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
		{
			ISQLResult *result = queries[0]->GetResultSet();
			if (result->GetRowCount() > 0 && result->FetchRow())
			{
				pbReqQueueManager.SetRequestTargetPlayer(uid, result->GetInt64(0), result->GetString(1));
			}
			else
			{
				pbReqQueueManager.InvalidLocal(uid);
			}
		};

		auto onQueryFailure = [uid](std::string, int)
		{
			// find steamid of player name in the api (TODO)
			pbReqQueueManager.InvalidLocal(uid);
		};

		KZDatabaseService::FindPlayerByAlias(targetPlayerName, onQuerySuccess, onQueryFailure);
	}
}

void PBRequest::SetupMode(KZPlayer *callingPlayer)
{
	if (localRequestState <= LocalRequestState::DISABLED)
	{
		return;
	}

	KZModeManager::ModePluginInfo modeInfo = modeName.IsEmpty() ? KZ::mode::GetModeInfo(callingPlayer->modeService) : KZ::mode::GetModeInfo(modeName);

	if (modeInfo.databaseID < 0)
	{
		pbReqQueueManager.InvalidLocal(this->uid);
		return;
	}

	// Change the mode name to the right one for later usage.
	modeName = modeInfo.shortModeName;

	localDBRequestParams.hasValidModeID = true;
	localDBRequestParams.modeID = modeInfo.databaseID;
}

void PBRequest::SetupStyles(KZPlayer *callingPlayer)
{
	if (localRequestState <= LocalRequestState::DISABLED)
	{
		return;
	}
	// If the style name is empty, take the calling player's styles.
	if (styleNames.IsEmpty())
	{
		localDBRequestParams.hasValidStyleIDs = true;
		localDBRequestParams.styleIDs = 0;
		FOR_EACH_VEC(callingPlayer->styleServices, i)
		{
			KZStyleManager::StylePluginInfo info = KZ::style::GetStyleInfo(callingPlayer->styleServices[i]);
			if (info.databaseID < 0)
			{
				pbReqQueueManager.InvalidLocal(this->uid);
				return;
			}

			styleList.AddToTail(info.shortName);

			localDBRequestParams.styleIDs |= (1ull << info.databaseID);
			queryRanking = false;
		}
	}
	else if (styleNames.IsEqual_FastCaseInsensitive("none"))
	{
		localDBRequestParams.hasValidStyleIDs = true;
		localDBRequestParams.styleIDs = 0;
	}
	// Example: VNL,CKZ
	else
	{
		CSplitString stylesSplit(styleNames.Get(), ",");
		FOR_EACH_VEC(stylesSplit, i)
		{
			KZStyleManager::StylePluginInfo info = KZ::style::GetStyleInfo(stylesSplit[i]);
			if (info.databaseID < 0)
			{
				pbReqQueueManager.InvalidLocal(this->uid);
				return;
			}

			styleList.AddToTail(info.shortName);

			localDBRequestParams.hasValidStyleIDs = true;
			localDBRequestParams.styleIDs |= (1ull << info.databaseID);
			queryRanking = false;
		}
	}
}

void PBRequest::SetupCourse(KZPlayer *callingPlayer)
{
	if (localRequestState <= LocalRequestState::DISABLED)
	{
		return;
	}

	bool gotCurrentMap = false;
	CUtlString currentMap = g_pKZUtils->GetCurrentMapName(&gotCurrentMap);
	if (!gotCurrentMap) // Shouldn't happen.
	{
		pbReqQueueManager.InvalidLocal(this->uid);
		return;
	}

	if (mapName.IsEmpty())
	{
		mapName = currentMap;
	}

	// Map + Course empty => current map, current course (first course if just joined)
	if (courseName.IsEmpty())
	{
		// If it's the current map...
		if (mapName == currentMap)
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
					pbReqQueueManager.InvalidLocal(this->uid, "PB Request - Invalid Course Name", course);
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
					pbReqQueueManager.UpdateCourseName(uid, result->GetString(0));
				}
				else
				{
					pbReqQueueManager.InvalidLocal(uid);
				}
			};

			auto onQueryFailure = [uid](std::string, int) { pbReqQueueManager.InvalidLocal(uid); };
			KZDatabaseService::FindFirstCourseByMapName(mapName, onQuerySuccess, onQueryFailure);
		}
	}
	else
	{
		// No empty field, should be valid.
		hasValidCourseName = true;
	}
}

void PBRequest::ExecuteLocalRequest()
{
	// No player to respond to, don't bother.
	KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
	if (!callingPlayer)
	{
		pbReqQueueManager.InvalidLocal(this->uid);
		return;
	}

	localRequestState = LocalRequestState::RUNNING;
	if (queryRanking)
	{
		ExecuteStandardLocalQuery();
	}
	else
	{
		ExecuteRanklessLocalQuery();
	}
}

void PBRequest::ExecuteStandardLocalQuery()
{
	u64 uid = this->uid;

	auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
	{
		PBData data {};
		ISQLResult *result = queries[0]->GetResultSet();
		if (result && result->GetRowCount() > 0)
		{
			data.hasPB = true;
			if (result->FetchRow())
			{
				data.runTime = result->GetFloat(0);
				data.teleportsUsed = result->GetInt(1);
			}
			if ((result = queries[1]->GetResultSet()) && result->FetchRow())
			{
				data.rank = result->GetInt(0);
			}
			if ((result = queries[2]->GetResultSet()) && result->FetchRow())
			{
				data.maxRank = result->GetInt(0);
			}
		}
		if ((result = queries[3]->GetResultSet()) && result->GetRowCount() > 0)
		{
			data.hasPBPro = true;
			if (result->FetchRow())
			{
				data.runTimePro = result->GetFloat(0);
			}
			if ((result = queries[4]->GetResultSet()) && result->FetchRow())
			{
				data.rankPro = result->GetInt(0);
			}
			if ((result = queries[5]->GetResultSet()) && result->FetchRow())
			{
				data.maxRankPro = result->GetInt(0);
			}
		}
		pbReqQueueManager.UpdateLocalPBData(uid, data);
	};

	auto onQueryFailure = [uid](std::string, int) { pbReqQueueManager.InvalidLocal(uid); };

	KZDatabaseService::QueryPB(targetSteamID64, mapName, courseName, localDBRequestParams.modeID, onQuerySuccess, onQueryFailure);
}

void PBRequest::ExecuteRanklessLocalQuery()
{
	u64 uid = this->uid;
	auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
	{
		PBData data;
		ISQLResult *result = queries[0]->GetResultSet();
		if (result && result->GetRowCount() > 0)
		{
			data.hasPB = true;
			if (result->FetchRow())
			{
				data.runTime = result->GetFloat(0);
				data.teleportsUsed = result->GetInt(1);
			}
		}
		if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
		{
			data.hasPBPro = true;
			if (result->FetchRow())
			{
				data.runTimePro = result->GetFloat(0);
			}
		}
		pbReqQueueManager.UpdateLocalPBData(uid, data);
	};

	auto onQueryFailure = [uid](std::string, int) { pbReqQueueManager.InvalidLocal(uid); };

	KZDatabaseService::QueryPBRankless(targetSteamID64, mapName, courseName, localDBRequestParams.modeID, localDBRequestParams.styleIDs,
									   onQuerySuccess, onQueryFailure);
}

SCMD_CALLBACK(CommandKZPB)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	KeyValues3 params;

	if (!utils::ParseArgsToKV3(args->ArgS(), params, paramKeys, sizeof(paramKeys) / sizeof(paramKeys[0])))
	{
		player->languageService->PrintChat(true, false, "PB Command Usage");
		player->languageService->PrintConsole(false, false, "PB Command Usage - Console");
		return MRES_SUPERCEDE;
	}

	KeyValues3 *kv = NULL;

	CUtlString playerName;
	CUtlString mapName;
	CUtlString courseName;
	CUtlString modeName;
	CUtlString styleNames;

	if ((kv = params.FindMember("player")) || (kv = params.FindMember("p")))
	{
		playerName = kv->GetString();
	}
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
	if ((kv = params.FindMember("style")) || (kv = params.FindMember("s")))
	{
		styleNames = kv->GetString();
	}

	pbReqQueueManager.AddRequest(player, playerName, mapName, courseName, modeName, styleNames);
	return MRES_SUPERCEDE;
}

void KZ::timer::CheckPBRequests()
{
	pbReqQueueManager.CheckRequests();
}

void KZTimerService::RegisterPBCommand()
{
	scmd::RegisterCmd("kz_pb", CommandKZPB);
}
