#include "kz/db/kz_db.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "utils/simplecmds.h"
#include "iserver.h"

#include "kz/db/queries/general.h"
#include "kz/db/queries/players.h"
#include "kz/db/queries/courses.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#define PB_WAIT_THRESHOLD 3.0f

static_function void UpdatePBRequestTargetPlayer(u64 uid, u64 steamID64, CUtlString name);
static_function void UpdatePBRequestLocalCourseID(u64 uid, i32 courseID);
static_function void DisableLocalPBRequest(u64 uid);

// Every time the player calls !pb, a request struct is created and stored in a vector.
// The request queries all the information required in order to make a request to the database,
// waits for responses from local and global database, and print a coherent result message to the player.
struct PBRequest
{
	PBRequest(KZPlayer *callingPlayer) : callerUserID(callingPlayer->GetClient()->GetUserID()), targetSteamID64(callingPlayer->GetSteamId64())
	{
		timestamp = g_pKZUtils->GetServerGlobals()->realtime;
		shouldRequestLocal = KZDatabaseService::IsReady();
		shouldRequestGlobal = false;
	}

	u64 uid;
	f32 timestamp;

	CPlayerUserId callerUserID;

	bool hasValidTarget {};
	u64 targetSteamID64;
	CUtlString targetPlayerName;

	void SetupSteamID64(KZPlayer *callingPlayer, CUtlString playerName)
	{
		/*
			1. no player specified -> default to calling player's steamid
			2. player specified -> find steamid of player with matching name in the server
			3. no server matching player -> find steamid of player name in the local database
			4. no localdb matching player -> find steamid of player name in the api (TODO)
			5. no matching name in the api -> abort
		*/
		if (playerName.IsEmpty())
		{
			hasValidTarget = true;
			targetPlayerName = callingPlayer->GetName();
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
			if (playerName == client->GetClientName())
			{
				hasValidTarget = true;
				targetPlayerName = player->GetName();
				targetSteamID64 = player->GetSteamId64();
				return;
			}
		}
		if (KZDatabaseService::IsReady())
		{
			Transaction txn;
			char query[1024];
			V_snprintf(query, sizeof(query), sql_players_searchbyalias, playerName.Get());
			txn.queries.push_back(query);

			u64 uid = this->uid;

			auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
			{
				ISQLResult *result = queries[0]->GetResultSet();
				UpdatePBRequestTargetPlayer(uid, result->GetInt(0), result->GetString(1));
			};

			auto onQueryFailure = [uid](std::string, int)
			{
				// find steamid of player name in the api (TODO)
				DisableLocalPBRequest(uid);
			};

			KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onQuerySuccess, onQueryFailure);
		}
	}

	struct LocalDBRequestParams
	{
		i32 modeID;
		u64 styleIDs;
		i32 courseID;

		bool hasValidModeID {};
		bool hasValidStyleIDs {};
		bool hasValidCourseIDs {};

		bool HasValidParams()
		{
			return hasValidModeID && hasValidStyleIDs && hasValidCourseIDs;
		}
	} localDBRequestParams;

	void SetupMode(KZPlayer *callingPlayer, CUtlString modeName)
	{
		if (!shouldRequestLocal)
		{
			return;
		}
		// TODO: shouldRequestGlobal = false if mode is not global
		KZModeManager::ModePluginInfo modeInfo =
			modeName.IsEmpty() ? KZ::mode::GetModeInfo(callingPlayer->modeService) : KZ::mode::GetModeInfo(modeName);
		if (modeInfo.databaseID < 0)
		{
			callingPlayer->languageService->PrintChat(true, false, "PB Query - Unavailable Mode Name", modeName.Get());
			shouldRequestLocal = false;
			return;
		}
		localDBRequestParams.hasValidModeID = true;
		localDBRequestParams.modeID = modeInfo.databaseID;
	}

	void SetupStyle(KZPlayer *callingPlayer, CUtlString styleNames)
	{
		if (!shouldRequestLocal)
		{
			return;
		}
		// TODO: shouldRequestGlobal = false if styles are not global
		if (styleNames.IsEmpty())
		{
			FOR_EACH_VEC(callingPlayer->styleServices, i)
			{
				KZStyleManager::StylePluginInfo info = KZ::style::GetStyleInfo(callingPlayer->styleServices[i]);
				if (info.databaseID < 0)
				{
					callingPlayer->PrintChat(true, false, "PB Query - Unavailable Style Name", styleNames.Get());
					shouldRequestLocal = false;
					return;
				}
				localDBRequestParams.hasValidStyleIDs = true;
				localDBRequestParams.styleIDs &= (1ull << info.databaseID);
			}
		}
		else
		{
			CSplitString stylesSplit(styleNames.Get(), ",");
			FOR_EACH_VEC(stylesSplit, i)
			{
				KZStyleManager::StylePluginInfo info = KZ::style::GetStyleInfo(stylesSplit[i]);
				if (info.databaseID < 0)
				{
					callingPlayer->PrintChat(true, false, "PB Query - Unavailable Style Name", styleNames.Get());
					shouldRequestLocal = false;
					return;
				}
				localDBRequestParams.hasValidStyleIDs = true;
				localDBRequestParams.styleIDs &= (1ull << info.databaseID);
			}
		}
	}

	void SetupCourse(KZPlayer *callingPlayer, CUtlString mapName, CUtlString courseName)
	{
		if (!shouldRequestLocal)
		{
			return;
		}
		KZ::timer::CourseInfo info;

		// We are querying for the current map, but the current map hasn't finished initializing yet.
		// In that case, we pretend that the current map was manually
		if (!KZDatabaseService::IsMapSetUp() && mapName.IsEmpty())
		{
			CNetworkGameServerBase *networkGameServer = (CNetworkGameServerBase *)g_pNetworkServerService->GetIGameServer();
			if (networkGameServer)
			{
				mapName = networkGameServer->GetMapName();
			}
		}

		// Same for courses.
		if (!KZDatabaseService::AreCoursesSetUp() && courseName.IsEmpty())
		{
			CNetworkGameServerBase *networkGameServer = (CNetworkGameServerBase *)g_pNetworkServerService->GetIGameServer();
			if (networkGameServer)
			{
				mapName = networkGameServer->GetMapName();
			}
		}

		if (mapName.IsEmpty())
		{
			// Map + Course empty => current map, current course (first course if just joined)
			if (courseName.IsEmpty())
			{
				char course[KZ_MAX_COURSE_NAME_LENGTH];
				callingPlayer->timerService->GetCourse(course, KZ_MAX_COURSE_NAME_LENGTH);

				if ((!course[0] && !KZ::timer::GetFirstCourseInformation(info)) || !KZ::timer::GetCourseInformation(course, info))
				{
					callingPlayer->PrintChat(true, false, "PB Query - Invalid Course Name", course);
					shouldRequestLocal = false;
					return;
				}
				localDBRequestParams.hasValidCourseIDs = true;
				localDBRequestParams.courseID = info.databaseID;
			}
			// Map empty, course not empty => current map, check course dbid from course list
			else
			{
				if (!KZ::timer::GetCourseInformation(courseName.Get(), info))
				{
					callingPlayer->PrintChat(true, false, "PB Query - Invalid Course Name", courseName.Get());
					shouldRequestLocal = false;
					return;
				}
				localDBRequestParams.hasValidCourseIDs = true;
				localDBRequestParams.courseID = info.databaseID;
			}
		}
		// Map name isn't empty...
		else
		{
			u64 uid = this->uid;

			auto cleanMapName = KZDatabaseService::GetDatabaseConnection()->Escape(mapName.Get());
			auto cleanCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(courseName.Get());

			auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
			{
				ISQLResult *result = queries[0]->GetResultSet();
				UpdatePBRequestLocalCourseID(uid, result->GetInt(0));
			};

			auto onQueryFailure = [uid](std::string, int) { DisableLocalPBRequest(uid); };

			char query[1024];
			Transaction txn;
			// map not empty, course empty => query courseid of such map, first one sorted by UID
			if (courseName.IsEmpty())
			{
				V_snprintf(query, sizeof(query), sql_mapcourses_findfirst_mapname, cleanMapName.c_str());
			}
			// map not empty, course not empty => query courseid of such map and course name
			else
			{
				V_snprintf(query, sizeof(query), sql_mapcourses_findname_mapname, cleanMapName.c_str(), cleanCourseName.c_str());
			}
			txn.queries.push_back(query);
			KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onQuerySuccess, onQueryFailure);
		}
	}

	bool CanQueryLocalDB()
	{
		return hasValidTarget && localDBRequestParams.HasValidParams();
	}

	void QueryLocalDB()
	{
		if (queryingLocalDB || !CanQueryLocalDB())
		{
			return;
		}
		queryingLocalDB = true;
		Transaction txn;
		char query[1024];
		V_snprintf(query, sizeof(query), sql_getpb, targetSteamID64, localDBRequestParams.courseID, localDBRequestParams.modeID,
				   localDBRequestParams.styleIDs);
	}

	bool shouldRequestLocal {};
	bool queryingLocalDB {};

	bool hasLocalData {};

	struct GlobalRequestParams
	{
		CUtlString modeName;
		CUtlString styleNames;
		CUtlString mapName;
		CUtlString courseName;
	} globalRequestParams;

	bool shouldRequestGlobal {};
	bool hasGlobalData {};

	bool ShouldAnnounce()
	{
		return false;
	}
};

static_global CUtlVector<PBRequest> pbReqQueue;
static_global u32 pbReqCount = 0;

// TODO: Integrate global service
void RequestPBData(KZPlayer *callingPlayer, CUtlString playerName, CUtlString mapName, CUtlString courseName, CUtlString modeName,
				   CUtlString styleNames)
{
	u64 steamID64 = callingPlayer->GetSteamId64();
	u64 mapID = KZDatabaseService::GetMapID();

	KZ::timer::CourseInfo courseInfo;
	KZ::timer::GetCourseInformation(courseName, courseInfo);
	u64 courseID = courseInfo.databaseID;

	pbReqCount++;

	PBRequest req(callingPlayer);
	req.uid = pbReqCount;
	req.SetupMode(callingPlayer, modeName);
	req.SetupStyle(callingPlayer, styleNames);
	req.SetupCourse(callingPlayer, mapName, courseName);

	if (!req.shouldRequestLocal && !req.shouldRequestGlobal)
	{
		callingPlayer->languageService->PrintChat(true, false, "PB Query - Data Not Available");
	}
	else
	{
		pbReqQueue.AddToTail(req);
	}
	// TODO: Global stuff
}

void UpdatePBRequestTargetPlayer(u64 uid, u64 steamID64, CUtlString name)
{
	FOR_EACH_VEC(pbReqQueue, i)
	{
		if (pbReqQueue[i].uid == uid)
		{
			pbReqQueue[i].hasValidTarget = true;
			pbReqQueue[i].targetSteamID64 = steamID64;
			pbReqQueue[i].targetPlayerName = name;
			return;
		}
	}
}

void UpdatePBRequestLocalCourseID(u64 uid, i32 courseID)
{
	FOR_EACH_VEC(pbReqQueue, i)
	{
		if (pbReqQueue[i].uid == uid)
		{
			pbReqQueue[i].localDBRequestParams.hasValidCourseIDs = true;
			pbReqQueue[i].localDBRequestParams.courseID = courseID;
			return;
		}
	}
}

void DisableLocalPBRequest(u64 uid)
{
	FOR_EACH_VEC(pbReqQueue, i)
	{
		if (pbReqQueue[i].uid == uid)
		{
			pbReqQueue[i].shouldRequestLocal = false;
			return;
		}
	}
}

SCMD_CALLBACK(CommandKZPB)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	CUtlString playerName;
	CUtlString mapName;
	CUtlString courseName;
	CUtlString modeName;
	CUtlString styleNames;

	for (int i = 1; i < args->ArgC(); i++)
	{
		CUtlString arg = args->Arg(i);
		arg.ToLowerFast();
		if (arg.MatchesPattern("c=*"))
		{
			courseName = arg.Get() + 2;
		}
		else if (arg.MatchesPattern("course=*"))
		{
			courseName = arg.Get() + 7;
		}
		else if (arg.MatchesPattern("mode=*"))
		{
			modeName = arg.Get() + 5;
		}
		else if (arg.MatchesPattern("p=*"))
		{
			playerName = arg.Get() + 2;
		}
		else if (arg.MatchesPattern("player=*"))
		{
			playerName = arg.Get() + 7;
		}
		else if (arg.MatchesPattern("s=*"))
		{
			styleNames = arg.Get() + 2;
		}
		else if (arg.MatchesPattern("style=*"))
		{
			styleNames = arg.Get() + 6;
		}
		else
		{
			player->languageService->PrintChat(true, false, "PB Command Usage");
			return MRES_SUPERCEDE;
		}
	}
	RequestPBData(player, playerName, mapName, courseName, modeName, styleNames);
	return MRES_SUPERCEDE;
}

void KZDatabaseService::RegisterPBCommand()
{
	scmd::RegisterCmd("kz_pb", CommandKZPB);
}
