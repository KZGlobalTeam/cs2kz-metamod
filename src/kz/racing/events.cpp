#include "kz_racing.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"

void KZRacingService::OnRaceInit(const KZ::racing::events::RaceInit &raceInit)
{
	KZRacingService::currentRace.state = RaceInfo::RACE_INIT;
	KZRacingService::currentRace.data = raceInit;
	KZRacingService::currentRace.earliestStartTick = {};

	KZRacingService::CheckMap();

	if (g_pKZUtils->GetCurrentMapWorkshopID() == KZRacingService::currentRace.data.raceInfo.workshopID)
	{
		KZRacingService::SendRaceJoin();
		// Broadcast the race info to all players.
		for (i32 i = 0; i < MAXPLAYERS + 1; i++)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
			if (player)
			{
				std::string timeLimitString;
				if (KZRacingService::currentRace.data.raceInfo.duration > 0.0f)
				{
					auto timeStr = utils::FormatTime(KZRacingService::currentRace.data.raceInfo.duration, false);
					timeLimitString = player->languageService->PrepareMessage("Racing - Time Limit", timeStr.Get());
				}
				else
				{
					timeLimitString = player->languageService->PrepareMessage("Racing - No Time Limit");
				}
				// clang-format off
				player->languageService->PrintChat(true, false, "Racing - Race Info (Chat)",
													KZRacingService::currentRace.data.raceInfo.courseName.c_str(),
													KZRacingService::currentRace.data.raceInfo.modeName.c_str(),
													KZRacingService::currentRace.data.raceInfo.maxTeleports,
													timeLimitString.c_str());
				// clang-format on
			}
		}
	}
}

void KZRacingService::OnRaceCancel(const KZ::racing::events::RaceCancel &raceCancel)
{
	KZRacingService::currentRace = {};
	KZLanguageService::PrintChatAll(true, "Racing - Race Cancelled");
}

void KZRacingService::OnRaceStart(const KZ::racing::events::RaceStart &raceStart)
{
	KZRacingService::currentRace.state = RaceInfo::RACE_ONGOING;
	KZRacingService::currentRace.earliestStartTick = g_pKZUtils->GetServerGlobals()->tickcount + raceStart.countdownSeconds * ENGINE_FIXED_TICK_RATE;
	KZLanguageService::PrintChatAll(true, "Racing - Race Countdown", raceStart.countdownSeconds);
	for (const auto &participant : KZRacingService::currentRace.localParticipants)
	{
		KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(participant.id);
		if (player)
		{
			player->timerService->TimerStop();
		}
	}
}

void KZRacingService::OnPlayerAccept(const KZ::racing::events::PlayerAccept &playerAccept)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Accepted", playerAccept.player.name.c_str());
}

void KZRacingService::OnPlayerUnregister(const KZ::racing::events::PlayerUnregister &playerUnregister)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Unregistered", playerUnregister.player.name.c_str());
}

void KZRacingService::OnPlayerForfeit(const KZ::racing::events::PlayerForfeit &playerForfeit)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Forfeit", playerForfeit.player.name.c_str());
	// Add to local finishers list to avoid showing go message.
	for (auto it = KZRacingService::currentRace.localParticipants.begin(); it != KZRacingService::currentRace.localParticipants.end(); ++it)
	{
		if (it->id == playerForfeit.player.id)
		{
			KZRacingService::currentRace.localFinishers.push_back(*it);
			break;
		}
	}
}

void KZRacingService::OnPlayerFinish(const KZ::racing::events::PlayerFinish &playerFinish)
{
	CUtlString timeStr = utils::FormatTime(playerFinish.time);
	if (playerFinish.teleportsUsed > 1)
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (2+ Teleports)", playerFinish.player.name.c_str(), timeStr.Get(),
										playerFinish.teleportsUsed);
	}
	else if (playerFinish.teleportsUsed == 1)
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (1 Teleport)", playerFinish.player.name.c_str(), timeStr.Get(),
										playerFinish.teleportsUsed);
	}
	else
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (PRO)", playerFinish.player.name.c_str(), timeStr.Get(),
										playerFinish.teleportsUsed);
	}
	for (auto it = KZRacingService::currentRace.localParticipants.begin(); it != KZRacingService::currentRace.localParticipants.end(); ++it)
	{
		if (it->id == playerFinish.player.id)
		{
			KZRacingService::currentRace.localFinishers.push_back(*it);
			break;
		}
	}
}

void KZRacingService::OnRaceEnd(const KZ::racing::events::RaceEnd &raceEnd)
{
	KZRacingService::currentRace.state = RaceInfo::RACE_NONE;
}

static_function void GetRaceInfo(const KZ::racing::events::RaceResult &raceResult, std::vector<KZ::racing::PlayerInfo> &finishers,
								 std::vector<KZ::racing::PlayerInfo> &nonFinishers)
{
	for (const auto &finisher : raceResult.finishers)
	{
		if (finisher.completed)
		{
			finishers.push_back(finisher.player);
		}
		else
		{
			nonFinishers.push_back(finisher.player);
		}
	}
}

void KZRacingService::OnRaceResult(const KZ::racing::events::RaceResult &raceResult)
{
	std::vector<KZ::racing::PlayerInfo> finishers;
	std::vector<KZ::racing::PlayerInfo> nonFinishers;
	GetRaceInfo(raceResult, finishers, nonFinishers);
	KZLanguageService::PrintChatAll(true, "Racing - End Results Header");
	// Print first place to last place, then non-finishers.
	u32 position = 1;
	for (const auto &finisher : raceResult.finishers)
	{
		CUtlString timeStr = utils::FormatTime(finisher.time);
		if (finisher.completed)
		{
			if (position == 1)
			{
				KZLanguageService::PrintChatAll(false, "Racing - End Results First Place", finisher.player.name.c_str(), timeStr.Get());
			}
			else if (position == finishers.size())
			{
				KZLanguageService::PrintChatAll(false, "Racing - End Results Last Place", finisher.player.name.c_str(), timeStr.Get());
			}
			else
			{
				KZLanguageService::PrintChatAll(false, "Racing - End Results Finisher", position, finisher.player.name.c_str(), timeStr.Get());
			}
			position++;
		}
	}
	for (const auto &finisher : raceResult.finishers)
	{
		if (!finisher.completed)
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results Non-Finisher", finisher.player.name.c_str());
		}
	}
	KZRacingService::currentRace = {};
}

void KZRacingService::OnChatMessage(const KZ::racing::events::ChatMessage &chatMessage)
{
	// If the message starts with a '/' or '!', ignore it.
	if (!chatMessage.message.empty() && (chatMessage.message[0] == '/' || chatMessage.message[0] == '!'))
	{
		return;
	}
	utils::CPrintChatAll("{yellow}%s{default}: %s", chatMessage.player.name.c_str(), chatMessage.message.c_str());
}
