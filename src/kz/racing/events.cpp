#include "kz_racing.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"

void KZRacingService::OnChatMessage(const KZ::racing::events::ChatMessage &message)
{
	// If the message starts with a '/' or '!', ignore it.
	if (!message.content.empty() && (message.content[0] == '/' || message.content[0] == '!'))
	{
		return;
	}
	utils::CPrintChatAll("{yellow}%s{default}: %s", message.player.c_str(), message.content.c_str());
}

void KZRacingService::OnRaceConfigured(const KZ::racing::events::RaceConfigured &message)
{
	KZRacingService::currentRace.state = RaceInfo::State::Init;
	KZRacingService::currentRace.conf = message.conf;
	KZRacingService::currentRace.earliestStartTick = {};

	KZRacingService::CheckMap();

	if (g_pKZUtils->GetCurrentMapWorkshopID() == KZRacingService::currentRace.conf.workshopID)
	{
		// Broadcast the race info to all players.
		for (i32 i = 0; i < MAXPLAYERS + 1; i++)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
			if (player)
			{
				std::string timeLimitString;
				if (KZRacingService::currentRace.conf.maxDurationSeconds.has_value())
				{
					auto timeStr = utils::FormatTime(*KZRacingService::currentRace.conf.maxDurationSeconds, false);
					timeLimitString = player->languageService->PrepareMessage("Racing - Time Limit", timeStr.Get());
				}
				else
				{
					timeLimitString = player->languageService->PrepareMessage("Racing - No Time Limit");
				}
				if (KZRacingService::currentRace.conf.maxTeleports.has_value())
				{
					player->languageService->PrintChat(
						true, false, "Racing - Race Info - Teleport Limit (Chat)", KZRacingService::currentRace.conf.courseName.c_str(),
						KZRacingService::currentRace.conf.modeName.c_str(), *KZRacingService::currentRace.conf.maxTeleports, timeLimitString.c_str());
				}
				else
				{
					player->languageService->PrintChat(true, false, "Racing - Race Info - No Teleport Limit (Chat)",
													   KZRacingService::currentRace.conf.courseName.c_str(),
													   KZRacingService::currentRace.conf.modeName.c_str(), timeLimitString.c_str());
				}
			}
		}
	}
}

void KZRacingService::OnRaceStarting(const KZ::racing::events::RaceStarting &message)
{
	KZRacingService::currentRace.state = RaceInfo::State::Ongoing;
	KZRacingService::currentRace.earliestStartTick = g_pKZUtils->GetServerGlobals()->tickcount + message.countdownSeconds * ENGINE_FIXED_TICK_RATE;
	KZLanguageService::PrintChatAll(true, "Racing - Race Countdown", message.countdownSeconds);
	for (u64 steamID : KZRacingService::currentRace.localParticipants)
	{
		KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID);
		if (player)
		{
			player->timerService->TimerStop();
		}
	}
}

void KZRacingService::OnRaceCancelled(const KZ::racing::events::RaceCancelled &message)
{
	KZRacingService::currentRace = {};
	KZLanguageService::PrintChatAll(true, "Racing - Race Cancelled");
}

void KZRacingService::OnRaceCompleted(const KZ::racing::events::RaceCompleted &message)
{
	KZLanguageService::PrintChatAll(true, "Racing - End Results Header");

	u64 finishersCount = 0;

	for (const KZ::racing::RaceResult &result : message.results)
	{
		if (result.status == KZ::racing::RaceResult::Status::Finished)
		{
			finishersCount++;
		}
	}

	// Print first place to last place, then non-finishers.
	u32 position = 1;
	for (const KZ::racing::RaceResult &result : message.results)
	{
		if (result.status != KZ::racing::RaceResult::Status::Finished)
		{
			continue;
		}

		CUtlString timeStr = utils::FormatTime(*result.timeSeconds);

		if (position == 1)
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results First Place", result.playerName.c_str(), timeStr.Get());
		}
		else if (position == finishersCount)
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results Last Place", result.playerName.c_str(), timeStr.Get());
		}
		else
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results Finisher", position, result.playerName.c_str(), timeStr.Get());
		}

		position++;
	}

	for (const KZ::racing::RaceResult &result : message.results)
	{
		if (result.status != KZ::racing::RaceResult::Status::Finished)
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results Non-Finisher", result.playerName.c_str());
		}
	}

	KZRacingService::currentRace = {};
}

void KZRacingService::OnPlayerReady(const KZ::racing::events::PlayerReady &message)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Accepted", message.player.c_str());
}

void KZRacingService::OnPlayerFinished(const KZ::racing::events::PlayerFinished &message)
{
	CUtlString timeStr = utils::FormatTime(message.timeSeconds);

	if (message.teleports > 1)
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (2+ Teleports)", message.player.c_str(), timeStr.Get(), message.teleports);
	}
	else if (message.teleports == 1)
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (1 Teleport)", message.player.c_str(), timeStr.Get(), message.teleports);
	}
	else
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (PRO)", message.player.c_str(), timeStr.Get(), message.teleports);
	}
}

void KZRacingService::OnPlayerDisconnected(const KZ::racing::events::PlayerDisconnected &message)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Disconnect", message.player.c_str());
}

void KZRacingService::OnPlayerSurrendered(const KZ::racing::events::PlayerSurrendered &message)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Surrender", message.player.c_str());
}
