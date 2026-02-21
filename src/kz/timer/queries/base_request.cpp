#include "base_request.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "utils/ctimer.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

BaseRequest::BaseRequest(u64 uid, KZPlayer *player)
	: uid(uid), userID(player->GetClient()->GetUserID()), timestamp(g_pKZUtils->GetServerGlobals()->realtime)
{
}

BaseRequest::~BaseRequest()
{
	// Print out the error before cleanup
	if (!isValid && !this->invalidateReasonLocalized.empty())
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
		if (player)
		{
			player->languageService->PrintChat(true, false, this->invalidateReasonLocalized.c_str());
		}
	}
}

void BaseRequest::Init(u64 features, const CCommand *args, bool queryLocal, bool queryGlobal)
{
	this->features = features;
	KeyValues3 params;
	if (!utils::ParseArgsToKV3(args->ArgS(), params, const_cast<const char **>(paramKeys), KZ_ARRAYSIZE(paramKeys)))
	{
		this->Invalidate("");
		this->PrintInstructions();
		return;
	}
	this->localStatus = (queryLocal && KZDatabaseService::IsReady()) ? ResponseStatus::ENABLED : ResponseStatus::DISABLED;

	this->globalStatus = (queryGlobal && KZGlobalService::IsAvailable() ? ResponseStatus::ENABLED : ResponseStatus::DISABLED);

	KeyValues3 *kv = NULL;

	if (this->HasFeature(RequestFeature::Map))
	{
		kv = params.FindMember("map");
		this->mapName = kv ? kv->GetString() : "";
		// If a map is not specified, use the current map.
		if (this->mapName.IsEmpty())
		{
			bool gotCurrentMap = false;
			CUtlString currentMap = g_pKZUtils->GetCurrentMapName(&gotCurrentMap);
			if (!gotCurrentMap) // Shouldn't happen.
			{
				this->Invalidate("Request - Failed (Invalid Map)");
				return;
			}
			this->mapName = currentMap;
		}
	}
	if (this->HasFeature(RequestFeature::Course))
	{
		kv = params.FindMember("course");
		if (!kv)
		{
			kv = params.FindMember("c");
		}
		this->SetupCourse(kv ? kv->GetString() : "");
	}
	if (this->HasFeature(RequestFeature::Mode))
	{
		kv = params.FindMember("mode");
		this->SetupMode(kv ? kv->GetString() : "");
	}
	if (this->HasFeature(RequestFeature::Offset) && (kv = params.FindMember("offset")) || (kv = params.FindMember("o")))
	{
		this->offset = atoll(kv->GetString());
	}
	if (this->HasFeature(RequestFeature::Limit) && ((kv = params.FindMember("limit")) || (kv = params.FindMember("l"))))
	{
		this->limit = atoll(kv->GetString());
	}
	if (this->HasFeature(RequestFeature::Style) && ((kv = params.FindMember("style")) || (kv = params.FindMember("s"))))
	{
		this->SetupStyles(kv->GetString());
	}
	if (this->HasFeature(RequestFeature::Player))
	{
		kv = params.FindMember("player");
		if (!kv)
		{
			kv = params.FindMember("p");
		}
		this->SetupPlayer(kv ? kv->GetString() : "");
	}
	if (localStatus != ResponseStatus::ENABLED && globalStatus != ResponseStatus::ENABLED)
	{
		this->Invalidate("Request - Failed (Generic)");
	}
}

BaseRequest *BaseRequest::Find(u64 uid)
{
	auto it = std::find_if(instances.begin(), instances.end(), [uid](BaseRequest *instance) { return instance->uid == uid; });
	return (it != instances.end()) ? *it : nullptr;
}

void BaseRequest::Remove(u64 uid)
{
	auto it = std::find_if(instances.begin(), instances.end(), [uid](BaseRequest *instance) { return instance->uid == uid; });
	if (it != instances.end())
	{
		delete *it;
		instances.erase(it);
	}
}

void BaseRequest::CheckRequests()
{
	// clang-format off
	instances.erase(std::remove_if(instances.begin(), instances.end(),
		[](BaseRequest *instance)
		{
			instance->QueryLocal();
			instance->QueryGlobal();
			instance->CheckReply();
			if (!instance->isValid)
			{
				delete instance;
				return true;
			}
			else if (instance->timestamp + instance->timeout < g_pKZUtils->GetServerGlobals()->realtime)
			{
				// Force a reply with whatever data the instance has obtained.
				instance->Reply();
				delete instance;
				return true;
			}
			return false;
		}),
		instances.end());
	// clang-format on
}

/*
	A few scenarios can happen here, this depends on:
	1. If the specified map is the current map,
	2. If the player specified a course (and whether the player specifies a course name or a course number if yes),
	3. If the player has a current course,
	4. If the local database exists and whether the course exists in the local database.
	When specifying a course number, it is possible that the global API will return a different course from the course
	determined in the local database. In that case, we show both leaderboards and warns the player of potential mismatch.
*/
void BaseRequest::SetupCourse(CUtlString courseName)
{
	if (!isValid || !HasFeature(RequestFeature::Course))
	{
		return;
	}
	if (!courseName.IsEmpty())
	{
		this->courseName = courseName;
		return;
	}

	KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
	if (!callingPlayer) // Shouldn't happen, but just to be sure...
	{
		this->Invalidate();
		return;
	}

	bool gotCurrentMap = false;
	CUtlString currentMap = g_pKZUtils->GetCurrentMapName(&gotCurrentMap);
	if (!gotCurrentMap) // Shouldn't happen.
	{
		this->Invalidate("Request - Failed (Invalid Map)");
		return;
	}

	// If it's the current map...
	if (this->mapName == currentMap)
	{
		// Try to get the player's current course.
		const KZCourseDescriptor *course = callingPlayer->timerService->GetCourse();
		if (!course)
		{
			// No course? Take the map's first course.
			course = KZ::course::GetFirstCourse();
			if (!course)
			{
				this->Invalidate("Request - Failed (No Course in Current Map)", currentMap.Get());
				return;
			}
		}
		this->courseName = course->GetName();
	}
	// The caller specified a map name but not a course name. Try to find the first course locally.
	// If that fails as well, we just query course "1".
	else if (this->localStatus == ResponseStatus::ENABLED)
	{
		this->requestingFirstCourse = true;
		u64 uid = this->uid;
		auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
		{
			BaseRequest *req = BaseRequest::Find(uid);
			if (req)
			{
				req->requestingFirstCourse = false;
				ISQLResult *result = queries[0]->GetResultSet();
				if (result->GetRowCount() > 0 && result->FetchRow())
				{
					req->courseName = result->GetString(0);
				}
				else
				{
					req->courseName = "1";
					req->localStatus = ResponseStatus::DISABLED;
				}
			}
		};

		auto onQueryFailure = [uid](std::string, int)
		{
			BaseRequest *req = BaseRequest::Find(uid);
			if (req)
			{
				req->requestingFirstCourse = false;
				req->courseName = "1";
				req->localStatus = ResponseStatus::DISABLED;
			}
		};
		KZDatabaseService::FindFirstCourseByMapName(mapName, onQuerySuccess, onQueryFailure);
	}
	else
	{
		courseName = "1";
	}
}

void BaseRequest::SetupPlayer(CUtlString playerName)
{
	if (!isValid || !HasFeature(RequestFeature::Player))
	{
		return;
	}
	KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(this->userID);
	if (!callingPlayer)
	{
		this->Invalidate();
	}
	// If the caller doesn't specify a name, then the target is the caller.
	// Otherwise, players that are currently in the server is prioritized.
	// If there is no player matching the name in the server, first query the local database to get the player's SteamID.
	// If there's no local database/player is not found, query the global API to get the player's maptop.
	if (playerName.IsEmpty())
	{
		this->targetPlayerName = callingPlayer->GetName();
		this->targetSteamID64 = callingPlayer->GetSteamId64();
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
			targetPlayerName = player->GetName();
			targetSteamID64 = player->GetSteamId64();
			return;
		}
	}
	if (this->localStatus == ResponseStatus::ENABLED)
	{
		this->requestingLocalPlayer = true;
		auto onQuerySuccess = [uid = this->uid](std::vector<ISQLQuery *> queries)
		{
			BaseRequest *req = BaseRequest::Find(uid);
			if (req)
			{
				ISQLResult *result = queries[0]->GetResultSet();
				if (result->GetRowCount() > 0 && result->FetchRow())
				{
					req->targetSteamID64 = result->GetInt64(0);
					req->targetPlayerName = result->GetString(1);
				}
				else if (req->globalStatus == ResponseStatus::ENABLED)
				{
					req->requestingGlobalPlayer = true;
				}
				else
				{
					req->localStatus = ResponseStatus::DISABLED;
				}
				req->requestingLocalPlayer = false;
			}
		};

		auto onQueryFailure = [uid = this->uid](std::string, int)
		{
			BaseRequest *req = BaseRequest::Find(uid);
			if (req)
			{
				if (req->globalStatus == ResponseStatus::ENABLED)
				{
					req->requestingGlobalPlayer = true;
				}
				else
				{
					req->localStatus = ResponseStatus::DISABLED;
				}
				req->requestingLocalPlayer = false;
			}
		};

		KZDatabaseService::FindPlayerByAlias(playerName, onQuerySuccess, onQueryFailure);
	}
	else if (this->globalStatus != ResponseStatus::ENABLED)
	{
		this->Invalidate("Error Message (Player Not Found)", targetPlayerName.Get());
	}
}

void BaseRequest::SetupMode(CUtlString modeName)
{
	if (!isValid || !HasFeature(RequestFeature::Mode))
	{
		return;
	}

	KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
	if (!callingPlayer) // Shouldn't happen, but just to be sure...
	{
		this->Invalidate();
		return;
	}

	if (modeName.IsEmpty())
	{
		KZModeManager::ModePluginInfo modeInfo = KZ::mode::GetModeInfo(callingPlayer->modeService);

		if (modeInfo.id == -2)
		{
			this->Invalidate();
			return;
		}

		this->modeName = modeInfo.shortModeName;
		this->localModeID = modeInfo.databaseID;
	}
	else
	{
		this->modeName = modeName;

		KZModeManager::ModePluginInfo modeInfo = KZ::mode::GetModeInfo(modeName);

		if (modeInfo.databaseID < 0)
		{
			this->localStatus = ResponseStatus::DISABLED;
		}
		else
		{
			this->localModeID = modeInfo.databaseID;
		}
	}

	if (!KZ::api::DecodeModeString(this->modeName.Get(), this->apiMode))
	{
		this->globalStatus = ResponseStatus::DISABLED;
	}
}

void BaseRequest::SetupStyles(CUtlString styleNames)
{
	if (!isValid || !HasFeature(RequestFeature::Style))
	{
		return;
	}

	KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
	if (!callingPlayer) // Shouldn't happen, but just to be sure...
	{
		this->Invalidate();
		return;
	}

	// If the style name is empty, take the calling player's styles.
	if (styleNames.IsEmpty())
	{
		this->localStyleIDs = 0;
		FOR_EACH_VEC(callingPlayer->styleServices, i)
		{
			KZStyleManager::StylePluginInfo info = KZ::style::GetStyleInfo(callingPlayer->styleServices[i]);
			if (info.databaseID < 0)
			{
				this->localStatus = ResponseStatus::DISABLED;
			}
			if (this->localStatus != ResponseStatus::DISABLED)
			{
				this->localStyleIDs |= (1ull << info.databaseID);
			}
			styleList.AddToTail(info.shortName);
		}
	}
	else if (styleNames.IsEqual_FastCaseInsensitive("none"))
	{
		this->localStyleIDs = 0;
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
				this->localStatus = ResponseStatus::DISABLED;
			}
			if (this->localStatus != ResponseStatus::DISABLED)
			{
				this->localStyleIDs |= (1ull << info.databaseID);
			}
			styleList.AddToTail(info.id != -2 ? info.shortName : stylesSplit[i]);
		}
	}
};
