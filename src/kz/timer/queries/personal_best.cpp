#include "base_request.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/global/events.h"

#include "utils/utils.h"
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
		u32 points {}; // Global only.

		bool hasPBPro {};
		f32 runTimePro {};
		u32 rankPro {};
		u32 maxRankPro {};
		u32 pointsPro {}; // Global only.
	} pbData, gpbData;

	bool globallyBanned = false;
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
		if (this->requestingLocalPlayer || this->requestingFirstCourse || this->requestingGlobalPlayer)
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
		if (this->globalStatus != ResponseStatus::ENABLED)
		{
			// If the local query is waiting for a response from the global service...
			if (this->requestingGlobalPlayer && this->localStatus == ResponseStatus::ENABLED)
			{
				this->localStatus = ResponseStatus::DISABLED;
				this->requestingGlobalPlayer = false;
			}
			return;
		}
		this->globalStatus = ResponseStatus::PENDING;
		auto callback = [uid = this->uid](KZ::API::events::PersonalBest &pb)
		{
			PBRequest *req = (PBRequest *)PBRequest::Find(uid);
			if (!req)
			{
				return;
			}

			if (req->requestingGlobalPlayer && req->localStatus == ResponseStatus::ENABLED && pb.player.has_value())
			{
				req->requestingGlobalPlayer = false;
				req->queryLocalRanking = !pb.player->isBanned;
			}

			if (pb.map.has_value() && pb.course.has_value())
			{
				req->mapName = pb.map->name.c_str();
				req->courseName = pb.course->name.c_str();
				req->globalStatus = ResponseStatus::RECEIVED;
			}
			else
			{
				req->globalStatus = ResponseStatus::DISABLED;
				return;
			}

			req->gpbData.hasPB = pb.overall.has_value();
			if (req->gpbData.hasPB)
			{
				req->gpbData.runTime = pb.overall->time;
				req->gpbData.rank = pb.overall->nubRank;
				req->gpbData.maxRank = pb.overall->nubMaxRank;
				req->gpbData.teleportsUsed = pb.overall->teleports;
				req->gpbData.points = pb.overall->nubPoints;
			}
			req->gpbData.hasPBPro = pb.pro.has_value();
			if (req->gpbData.hasPBPro)
			{
				req->gpbData.runTimePro = pb.pro->time;
				req->gpbData.rankPro = pb.pro->proRank;
				req->gpbData.maxRankPro = pb.pro->proMaxRank;
				req->gpbData.pointsPro = pb.pro->proPoints;
			}
		};
		KZGlobalService::QueryPB(this->targetSteamID64, std::string_view(this->targetPlayerName.Get(), this->targetPlayerName.Length()),
								 std::string_view(this->mapName.Get(), this->mapName.Length()),
								 std::string_view(this->courseName.Get(), this->courseName.Length()), this->apiMode, this->styleList, callback);
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

		if (!this->pbData.hasPB && !this->gpbData.hasPB)
		{
			player->languageService->PrintChat(true, false, "PB Time - No Times");
		}
		else
		{
			if (this->localStatus == ResponseStatus::RECEIVED)
			{
				this->ReplyLocal();
			}
			if (this->globalStatus == ResponseStatus::RECEIVED)
			{
				this->ReplyGlobal();
			}
		}
	}

	void ReplyGlobal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		std::string tpText;
		if (this->gpbData.teleportsUsed > 0)
		{
			tpText = this->gpbData.teleportsUsed == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
													  : player->languageService->PrepareMessage("2+ Teleports Text", this->gpbData.teleportsUsed);
		}

		char overallTime[32];
		utils::FormatTime(this->gpbData.runTime, overallTime, sizeof(overallTime));
		char proTime[32];
		utils::FormatTime(this->gpbData.runTimePro, proTime, sizeof(proTime));

		if (!globallyBanned)
		{
			if (!this->gpbData.hasPBPro)
			{
				// KZ | Global: 12.34 (5 TPs) [Overall / 10000 pts]
				player->languageService->PrintChat(true, false, "PB Time - Overall (Global)", overallTime, tpText, this->gpbData.rank,
												   this->gpbData.maxRank, this->gpbData.points);
			}
			// Their MAP PB has 0 teleports, and is therefore also their PRO PB
			else if (this->gpbData.teleportsUsed == 0)
			{
				// KZ | Global: 12.34 [#1/24 Overall / 10000 pts] [#1/2 PRO / 10000 pts]
				player->languageService->PrintChat(true, false, "PB Time - Combined (Global)", overallTime, this->gpbData.rank, this->gpbData.maxRank,
												   this->gpbData.rankPro, this->gpbData.maxRankPro, this->gpbData.points, this->gpbData.pointsPro);
			}
			else
			{
				// KZ | Global: 12.34 (5 TPs) [#1/24 Overall / 10000 pts] | 23.45 [#1/2 PRO / 10000 pts]
				player->languageService->PrintChat(true, false, "PB Time - Split (Global)", overallTime, tpText, this->gpbData.rank,
												   this->gpbData.maxRank, proTime, this->gpbData.rankPro, this->gpbData.maxRankPro,
												   this->gpbData.points, this->gpbData.pointsPro);
			}
		}
		else
		{
			if (!this->gpbData.hasPBPro)
			{
				// KZ | Global: 12.34 (5 TPs) [Overall]
				player->languageService->PrintChat(true, false, "PB Time - Overall Rankless (Global)", overallTime, tpText);
			}
			// Their MAP PB has 0 teleports, and is therefore also their PRO PB
			else if (this->gpbData.teleportsUsed == 0)
			{
				// KZ | Global: 12.34 [Overall/PRO]
				player->languageService->PrintChat(true, false, "PB Time - Combined Rankless (Global)", overallTime);
			}
			else
			{
				// KZ | Global: 12.34 (5 TPs) [Overall] | 23.45 [PRO]
				player->languageService->PrintChat(true, false, "PB Time - Split Rankless (Global)", overallTime, tpText, proTime);
			}
		}
	}

	void ReplyLocal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		std::string tpText;
		if (this->pbData.teleportsUsed > 0)
		{
			tpText = this->pbData.teleportsUsed == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
													 : player->languageService->PrepareMessage("2+ Teleports Text", this->pbData.teleportsUsed);
		}

		char overallTime[32];
		utils::FormatTime(this->pbData.runTime, overallTime, sizeof(overallTime));
		char proTime[32];
		utils::FormatTime(this->pbData.runTimePro, proTime, sizeof(proTime));

		if (!globallyBanned)
		{
			if (!this->pbData.hasPB)
			{
				player->languageService->PrintChat(true, false, "PB Time - No Times");
			}
			else if (!this->pbData.hasPBPro)
			{
				// KZ | Server: 12.34 (5 TPs) [Overall]
				player->languageService->PrintChat(true, false, "PB Time - Overall (Server)", overallTime, tpText, this->pbData.rank,
												   this->pbData.maxRank);
			}
			// Their MAP PB has 0 teleports, and is therefore also their PRO PB
			else if (this->pbData.teleportsUsed == 0)
			{
				// KZ | Server: 12.34 [#1/24 Overall] [#1/2 PRO]
				player->languageService->PrintChat(true, false, "PB Time - Combined (Server)", overallTime, this->pbData.rank, this->pbData.maxRank,
												   this->pbData.rankPro, this->pbData.maxRankPro);
			}
			else
			{
				// KZ | Server: 12.34 (5 TPs) [#1/24 Overall] | 23.45 [#1/2 PRO]
				player->languageService->PrintChat(true, false, "PB Time - Split (Server)", overallTime, tpText, this->pbData.rank,
												   this->pbData.maxRank, proTime, this->pbData.rankPro, this->pbData.maxRankPro);
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
				player->languageService->PrintChat(true, false, "PB Time - Overall Rankless (Server)", overallTime, tpText);
			}
			// Their MAP PB has 0 teleports, and is therefore also their PRO PB
			else if (this->pbData.teleportsUsed == 0)
			{
				// KZ | Server: 12.34 [Overall/PRO]
				player->languageService->PrintChat(true, false, "PB Time - Combined Rankless (Server)", overallTime);
			}
			else
			{
				// KZ | Server: 12.34 (5 TPs) [Overall] | 23.45 [PRO]
				player->languageService->PrintChat(true, false, "PB Time - Split Rankless (Server)", overallTime, tpText, proTime);
			}
		}
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

SCMD(kz_pb, SCFL_RECORD | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	PBRequest::Create<PBRequest>(player, PBRequest::pbFeatures, true, true, args);
	return MRES_SUPERCEDE;
}

SCMD(kz_spb, SCFL_RECORD)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	PBRequest::Create<PBRequest>(player, PBRequest::pbFeatures, true, false, args);
	return MRES_SUPERCEDE;
}

SCMD(kz_gpb, SCFL_RECORD | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	PBRequest::Create<PBRequest>(player, PBRequest::pbFeatures, false, true, args);
	return MRES_SUPERCEDE;
}
