#include "kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
using namespace KZ::timer;
#define ANNOUNCEMENT_WAIT_THRESHOLD 5.0f

struct AnnounceData
{
public:
	AnnounceData(u32 id, KZPlayer *player, CUtlString courseName, f64 time, u64 teleportsUsed)
		: id(id), courseName(courseName), time(time), teleportsUsed(teleportsUsed)
	{
		timestamp = g_pKZUtils->GetServerGlobals()->realtime;
		playerName = player->GetName();
		steamID64 = player->GetSteamId64();
		isCheater = player->databaseService->isCheater;
		modeName = player->modeService->GetModeShortName();
		FOR_EACH_VEC(player->styleServices, i)
		{
			styleNames.AddToTail(player->styleServices[i]->GetStyleShortName());
		}
		if (KZDatabaseService::IsReady())
		{
			KZDatabaseService::SaveTime(id, player, courseName, time, teleportsUsed);
		}
	}

	u32 id;
	f32 timestamp;

	bool ShouldAnnounce()
	{
		// If it's a styled run we don't care about ranking, just announce it right away.
		if (styleNames.Count() != 0 || (timestamp + ANNOUNCEMENT_WAIT_THRESHOLD) < g_pKZUtils->GetServerGlobals()->realtime)
		{
			return true;
		}
		// If there is a database service but the response hasn't arrived yet...
		if (KZDatabaseService::IsReady() && !hasLocalRank)
		{
			return false;
		}
		// If there is a global service but the response hasn't arrived yet...
		if (false /*KZGlobalService::IsReady*/ && !hasGlobalRank)
		{
			return false;
		}
		return true;
	}

	CUtlString playerName;
	u64 steamID64;
	bool isCheater;
	// Should be an reference to course info later?
	CUtlString courseName;
	CUtlString modeName;
	CCopyableUtlVector<CUtlString> styleNames;
	f64 time;
	u64 teleportsUsed;

	// Local ranking variables
	bool hasLocalRank {};

	bool HasValidLocalRank()
	{
		return hasLocalRank && styleNames.Count() == 0;
	}

	LocalRankData localRankData;

	// Global ranking variables
	bool hasGlobalRank {};

	bool HasValidGlobalRank()
	{
		return hasGlobalRank && styleNames.Count() == 0;
	}

	GlobalRankData globalRankData;

	void AnnounceRun()
	{
		char formattedTime[32];
		KZTimerService::FormatTime(time, formattedTime, sizeof(formattedTime));

		CUtlString combinedModeStyleText;
		combinedModeStyleText.Format("{purple}%s{grey}", modeName.Get());
		FOR_EACH_VEC(styleNames, i)
		{
			combinedModeStyleText += " +{grey2}";
			combinedModeStyleText.Append(styleNames[i].Get());
			combinedModeStyleText += "{grey}";
		}

		char formattedDiffTimeLocal[32];
		KZTimerService::FormatDiffTime(localRankData.pbDiff, formattedDiffTimeLocal, sizeof(formattedDiffTimeLocal));

		char formattedDiffTimeLocalPro[32];
		KZTimerService::FormatDiffTime(localRankData.pbDiffPro, formattedDiffTimeLocalPro, sizeof(formattedDiffTimeLocalPro));

		char formattedDiffTimeGlobal[32];
		KZTimerService::FormatDiffTime(globalRankData.pbDiff, formattedDiffTimeGlobal, sizeof(formattedDiffTimeGlobal));

		char formattedDiffTimeGlobalPro[32];
		KZTimerService::FormatDiffTime(globalRankData.pbDiffPro, formattedDiffTimeGlobalPro, sizeof(formattedDiffTimeGlobalPro));

		/*
			KZ | GameChaos finished "blocks2006" in 10:06.84 [VNL | PRO]
			KZ | Server Rank: #1/24 Overall (-1:00.00) | #1/10 PRO (-2:00)
			KZ | Global Rank: #1/123 Overall (-1:00.00) | #1/23 PRO (-2:00)
			KZ | Map Points: 2345 (+512) | Player Rating: 34475
		*/
		for (u32 i = 0; i < MAXPLAYERS + 1; i++)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
			// Print basic information
			// KZ | GameChaos finished "blocks2006" in 10:06.84 [VNL | PRO]
			if (!player->IsInGame())
			{
				continue;
			}
			std::string teleportText = "{blue}PRO{grey}";
			if (teleportsUsed > 0)
			{
				teleportText = teleportsUsed == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
												  : player->languageService->PrepareMessage("2+ Teleports Text", teleportsUsed);
			}
			player->languageService->PrintChat(true, false, "Beat Course Info - Basic", playerName.Get(), courseName.Get(), formattedTime,
											   combinedModeStyleText.Get(), teleportText.c_str());
			// Print server ranking information if available.
			if (HasValidLocalRank())
			{
				// clang-format off
				std::string diffText = localRankData.firstTime ? 
					"" : player->languageService->PrepareMessage("Personal Best Difference", localRankData.pbDiff < 0 ? "{green}" : "{red}", formattedDiffTimeLocal);
				std::string diffTextPro = localRankData.firstTimePro ? 
					"" : player->languageService->PrepareMessage("Personal Best Difference", localRankData.pbDiffPro < 0 ? "{green}" : "{red}", formattedDiffTimeLocalPro);

				// clang-format on
				if (teleportsUsed > 0)
				{
					player->languageService->PrintChat(true, false, "Beat Course Info - Local (TP)", localRankData.rank, localRankData.maxRank,
													   diffText.c_str());
				}
				else
				{
					player->languageService->PrintChat(true, false, "Beat Course Info - Local (PRO)", localRankData.rank, localRankData.maxRank,
													   diffText.c_str(), localRankData.rankPro, localRankData.maxRankPro, diffTextPro.c_str());
				}
			}
			// Print global information if available.
			if (HasValidGlobalRank())
			{
				// clang-format off
				std::string diffText = globalRankData.firstTime ? 
					"" : player->languageService->PrepareMessage("Personal Best Difference", globalRankData.pbDiff < 0 ? "{green}" : "{red}", formattedDiffTimeGlobal);
				std::string diffTextPro = globalRankData.firstTimePro ? 
					"" : player->languageService->PrepareMessage("Personal Best Difference", globalRankData.pbDiffPro < 0 ? "{green}" : "{red}", formattedDiffTimeGlobalPro);

				// clang-format on
				if (teleportsUsed > 0)
				{
					player->languageService->PrintChat(true, false, "Beat Course Info - Global (TP)", globalRankData.rank, globalRankData.maxRank,
													   diffText.c_str());
				}
				else
				{
					player->languageService->PrintChat(true, false, "Beat Course Info - Global (PRO)", globalRankData.rank, globalRankData.maxRank,
													   diffText.c_str(), globalRankData.rankPro, globalRankData.maxRankPro, diffTextPro.c_str());
				}
				player->languageService->PrintChat(true, false, "Beat Course Info - Global Points", globalRankData.mapPointsGained,
												   globalRankData.totalMapPoints, globalRankData.playerRating);
			}
		}
		// TODO: Maybe do something if we have local/global database connection but the information is not available?
	}
};

static_global CUtlVector<AnnounceData> announceQueue;
static_global u32 announceCount = 0;

void KZ::timer::AddRunToAnnounceQueue(KZPlayer *player, CUtlString courseName, f64 time, u64 teleportsUsed)
{
	announceQueue.AddToTail({announceCount++, player, courseName, time, teleportsUsed});
}

void KZ::timer::UpdateLocalRankData(u32 id, LocalRankData data)
{
	FOR_EACH_VEC(announceQueue, i)
	{
		if (announceQueue[i].id == id)
		{
			announceQueue[i].hasLocalRank = true;
			announceQueue[i].localRankData = data;
			return;
		}
	}
}

void KZ::timer::UpdateGlobalRankData(u32 id, GlobalRankData data)
{
	FOR_EACH_VEC(announceQueue, i)
	{
		if (announceQueue[i].id == id)
		{
			announceQueue[i].globalRankData = data;
			return;
		}
	}
}

void KZ::timer::CheckAnnounceQueue()
{
	FOR_EACH_VEC(announceQueue, i)
	{
		AnnounceData &announceData = announceQueue[i];
		if (announceData.ShouldAnnounce())
		{
			announceData.AnnounceRun();
			announceQueue.Remove(i);
			i--;
			continue;
		}
	}
}

void KZ::timer::ClearAnnounceQueue()
{
	announceQueue.RemoveAll();
}
