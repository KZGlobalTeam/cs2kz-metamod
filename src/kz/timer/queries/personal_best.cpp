#include "base_request.h"
#include "kz/db/kz_db.h"

#include "utils/simplecmds.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

struct PBRequest : public BaseRequest
{
	using BaseRequest::BaseRequest;
	static constexpr u64 pbFeatures =
		RequestFeature::Course | RequestFeature::Map | RequestFeature::Mode | RequestFeature::Player | RequestFeature::Style;

	struct
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
	} pbData, gpbData;

	bool queryLocalRanking = true;

	virtual void Init(u64 features, const CCommand *args, bool queryLocal, bool queryGlobal) override
	{
		BaseRequest::Init(features, args, queryLocal, queryGlobal);
		if (this->styleList.Count() > 0)
		{
			this->queryLocalRanking = false;
		}
		else if (this->targetSteamID64)
		{
			for (u32 i = 1; i < MAXPLAYERS + 1; i++)
			{
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
				if (this->targetSteamID64 == player->GetSteamId64())
				{
					if (player->databaseService->isCheater)
					{
						this->queryLocalRanking = false;
					}
					return;
				}
			}
		}
	}

	virtual void PrintInstructions()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}
		player->languageService->PrintChat(true, false, "PB Command Usage");
		player->languageService->PrintConsole(false, false, "PB Command Usage - Console");
	}

	virtual void QueryLocal()
	{
		if (this->requestingLocalPlayer || this->requestingFirstCourse)
		{
			return;
		}

		KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
		if (!callingPlayer)
		{
			this->Invalidate();
			return;
		}

		if (this->localStatus == ResponseStatus::ENABLED)
		{
			this->localStatus = ResponseStatus::PENDING;
			this->queryLocalRanking ? this->ExecuteStandardLocalQuery() : this->ExecuteRanklessLocalQuery();
		}
	}

	virtual void QueryGlobal()
	{
		if (this->requestingLocalPlayer || this->requestingFirstCourse)
		{
			return;
		}
		// TODO
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
			player->languageService->PrintChat(true, false, "PB Request - Failed (Generic)");
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

		// Player on kz_map (Main) [VNL]
		player->languageService->PrintChat(true, false, "PB Header", targetPlayerName.Get(), mapName.Get(), courseName.Get(),
										   combinedModeStyleText.Get());

		if (this->globalStatus == ResponseStatus::RECEIVED)
		{
			this->ReplyGlobal();
		}
		if (this->localStatus == ResponseStatus::RECEIVED)
		{
			this->ReplyLocal();
		}
	}

	void ReplyLocal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		std::string localTPText;
		if (this->pbData.teleportsUsed > 0)
		{
			localTPText = this->pbData.teleportsUsed == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
														  : player->languageService->PrepareMessage("2+ Teleports Text", this->pbData.teleportsUsed);
		}

		char localOverallTime[32];
		KZTimerService::FormatTime(this->pbData.runTime, localOverallTime, sizeof(localOverallTime));
		char localProTime[32];
		KZTimerService::FormatTime(this->pbData.runTimePro, localProTime, sizeof(localProTime));

		if (queryLocalRanking)
		{
			if (!this->pbData.hasPB)
			{
				player->languageService->PrintChat(true, false, "PB Time - No Times");
			}
			else if (!this->pbData.hasPBPro)
			{
				// KZ | Server: 12.34 (5 TPs) [Overall]
				player->languageService->PrintChat(true, false, "PB Time - Overall (Server)", localOverallTime, localTPText, this->pbData.rank,
												   this->pbData.maxRank);
			}
			// Their MAP PB has 0 teleports, and is therefore also their PRO PB
			else if (this->pbData.teleportsUsed == 0)
			{
				// KZ | Server: 12.34 [#1/24 Overall] [#1/2 PRO]
				player->languageService->PrintChat(true, false, "PB Time - Combined (Server)", localOverallTime, this->pbData.rank,
												   this->pbData.maxRank, this->pbData.rankPro, this->pbData.maxRankPro);
			}
			else
			{
				// KZ | Server: 12.34 (5 TPs) [#1/24 Overall] | 23.45 [#1/2 PRO]
				player->languageService->PrintChat(true, false, "PB Time - Split (Server)", localOverallTime, localTPText, this->pbData.rank,
												   this->pbData.maxRank, localProTime, this->pbData.rankPro, this->pbData.maxRankPro);
			}
		}
		else
		{
			if (!this->pbData.hasPB)
			{
				player->languageService->PrintChat(true, false, "PB Time - No Times");
			}
			else if (!this->pbData.hasPBPro)
			{
				// KZ | Server: 12.34 (5 TPs) [Overall]
				player->languageService->PrintChat(true, false, "PB Time - Overall Rankless (Server)", localOverallTime, localTPText);
			}
			// Their MAP PB has 0 teleports, and is therefore also their PRO PB
			else if (this->pbData.teleportsUsed == 0)
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

	void ReplyGlobal()
	{
		// TODO
	}

	void ExecuteStandardLocalQuery()
	{
		u64 uid = this->uid;

		auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
		{
			PBRequest *req = (PBRequest *)PBRequest::Find(uid);
			if (req)
			{
				ISQLResult *result = queries[0]->GetResultSet();
				if (result && result->GetRowCount() > 0)
				{
					req->pbData.hasPB = true;
					if (result->FetchRow())
					{
						req->pbData.runTime = result->GetFloat(0);
						req->pbData.teleportsUsed = result->GetInt(1);
					}
					if ((result = queries[1]->GetResultSet()) && result->FetchRow())
					{
						req->pbData.rank = result->GetInt(0);
					}
					if ((result = queries[2]->GetResultSet()) && result->FetchRow())
					{
						req->pbData.maxRank = result->GetInt(0);
					}
				}
				if ((result = queries[3]->GetResultSet()) && result->GetRowCount() > 0)
				{
					req->pbData.hasPBPro = true;
					if (result->FetchRow())
					{
						req->pbData.runTimePro = result->GetFloat(0);
					}
					if ((result = queries[4]->GetResultSet()) && result->FetchRow())
					{
						req->pbData.rankPro = result->GetInt(0);
					}
					if ((result = queries[5]->GetResultSet()) && result->FetchRow())
					{
						req->pbData.maxRankPro = result->GetInt(0);
					}
				}
				req->localStatus = ResponseStatus::RECEIVED;
			}
		};

		auto onQueryFailure = [uid](std::string, int)
		{
			PBRequest *req = (PBRequest *)PBRequest::Find(uid);
			if (req)
			{
				req->localStatus = ResponseStatus::DISABLED;
			}
		};

		KZDatabaseService::QueryPB(targetSteamID64, this->mapName, this->courseName, this->localModeID, onQuerySuccess, onQueryFailure);
	}

	void ExecuteRanklessLocalQuery()
	{
		u64 uid = this->uid;
		auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
		{
			PBRequest *req = (PBRequest *)PBRequest::Find(uid);
			if (req)
			{
				ISQLResult *result = queries[0]->GetResultSet();
				if (result && result->GetRowCount() > 0)
				{
					req->pbData.hasPB = true;
					if (result->FetchRow())
					{
						req->pbData.runTime = result->GetFloat(0);
						req->pbData.teleportsUsed = result->GetInt(1);
					}
				}
				if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
				{
					req->pbData.hasPBPro = true;
					if (result->FetchRow())
					{
						req->pbData.runTimePro = result->GetFloat(0);
					}
				}
			}
		};

		auto onQueryFailure = [uid](std::string, int)
		{
			PBRequest *req = (PBRequest *)PBRequest::Find(uid);
			if (req)
			{
				req->localStatus = ResponseStatus::DISABLED;
			}
		};
		KZDatabaseService::QueryPBRankless(targetSteamID64, mapName, courseName, localModeID, localStyleIDs, onQuerySuccess, onQueryFailure);
	}
};

SCMD_CALLBACK(CommandKZPB)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	PBRequest::Create<PBRequest>(player, PBRequest::pbFeatures, true, true, args);
	return MRES_SUPERCEDE;
}

SCMD_CALLBACK(CommandKZSPB)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	PBRequest::Create<PBRequest>(player, PBRequest::pbFeatures, true, false, args);
	return MRES_SUPERCEDE;
}

SCMD_CALLBACK(CommandKZGPB)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	PBRequest::Create<PBRequest>(player, PBRequest::pbFeatures, false, true, args);
	return MRES_SUPERCEDE;
}

void KZTimerService::RegisterPBCommand()
{
	scmd::RegisterCmd("kz_pb", CommandKZPB);
	scmd::RegisterCmd("kz_spb", CommandKZSPB);
	scmd::RegisterCmd("kz_gpb", CommandKZGPB);
}
