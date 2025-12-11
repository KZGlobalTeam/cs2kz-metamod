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
	utils::CPrintChatAll("{yellow}%s{default}: %s", message.player.name.c_str(), message.content.c_str());
}

void KZRacingService::OnInitRace(const KZ::racing::events::InitRace &message)
{
	KZRacingService::currentRace.state = RaceInfo::State::Init;
	KZRacingService::currentRace.data = message;
	KZRacingService::currentRace.earliestStartTick = {};

	KZRacingService::CheckMap();

	if (g_pKZUtils->GetCurrentMapWorkshopID() == KZRacingService::currentRace.data.workshopID)
	{
		KZRacingService::SendReady();
		// Broadcast the race info to all players.
		for (i32 i = 0; i < MAXPLAYERS + 1; i++)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
			if (player)
			{
				std::string timeLimitString;
				if (KZRacingService::currentRace.data.maxDurationSeconds > 0.0f)
				{
					auto timeStr = utils::FormatTime(KZRacingService::currentRace.data.maxDurationSeconds, false);
					timeLimitString = player->languageService->PrepareMessage("Racing - Time Limit", timeStr.Get());
				}
				else
				{
					timeLimitString = player->languageService->PrepareMessage("Racing - No Time Limit");
				}
				// clang-format off
				player->languageService->PrintChat(true, false, "Racing - Race Info (Chat)",
													KZRacingService::currentRace.data.courseName.c_str(),
													KZRacingService::currentRace.data.modeName.c_str(),
													KZRacingService::currentRace.data.maxTeleports,
													timeLimitString.c_str());
				// clang-format on
			}
		}
	}
}

void KZRacingService::OnBeginRace(const KZ::racing::events::BeginRace &message)
{
	KZRacingService::currentRace.state = RaceInfo::State::Ongoing;
	KZRacingService::currentRace.earliestStartTick = g_pKZUtils->GetServerGlobals()->tickcount + message.countdownSeconds * ENGINE_FIXED_TICK_RATE;
	KZLanguageService::PrintChatAll(true, "Racing - Race Countdown", message.countdownSeconds);
	for (const auto &participant : KZRacingService::currentRace.localParticipants)
	{
		KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(participant.id);
		if (player)
		{
			player->timerService->TimerStop();
		}
	}
}

void KZRacingService::OnCancelRace(const KZ::racing::events::CancelRace &message)
{
	KZRacingService::currentRace = {};
	KZLanguageService::PrintChatAll(true, "Racing - Race Cancelled");
}

void KZRacingService::OnRaceResults(const KZ::racing::events::RaceResults &message)
{
	std::vector<KZ::racing::PlayerInfo> finishers;
	std::vector<KZ::racing::PlayerInfo> nonFinishers;

	for (const KZ::racing::events::RaceResults::Participant &participant : message.participants)
	{
		switch (participant.state)
		{
			case KZ::racing::events::RaceResults::Participant::State::Disconnected: // TODO
			case KZ::racing::events::RaceResults::Participant::State::Surrendered:  // TODO
			case KZ::racing::events::RaceResults::Participant::State::DidNotFinish:
			{
				nonFinishers.push_back({participant.id, participant.name});
			}
			break;

			case KZ::racing::events::RaceResults::Participant::State::Finished:
			{
				finishers.push_back({participant.id, participant.name});
			}
			break;
		}
	}

	KZLanguageService::PrintChatAll(true, "Racing - End Results Header");

	// Print first place to last place, then non-finishers.
	u32 position = 1;
	for (const KZ::racing::events::RaceResults::Participant &participant : message.participants)
	{
		if (participant.state != KZ::racing::events::RaceResults::Participant::State::Finished)
		{
			continue;
		}

		CUtlString timeStr = utils::FormatTime(participant.timeSeconds);

		if (position == 1)
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results First Place", participant.name.c_str(), timeStr.Get());
		}
		else if (position == finishers.size())
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results Last Place", participant.name.c_str(), timeStr.Get());
		}
		else
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results Finisher", position, participant.name.c_str(), timeStr.Get());
		}

		position++;
	}

	for (const KZ::racing::events::RaceResults::Participant &participant : message.participants)
	{
		if (participant.state != KZ::racing::events::RaceResults::Participant::State::Finished)
		{
			KZLanguageService::PrintChatAll(false, "Racing - End Results Non-Finisher", participant.name.c_str());
		}
	}

	KZRacingService::currentRace = {};
}

void KZRacingService::OnServerJoinRace(const KZ::racing::events::ServerJoinRace &message)
{
	// TODO
}

void KZRacingService::OnServerLeaveRace(const KZ::racing::events::ServerLeaveRace &message)
{
	// TODO
}

void KZRacingService::OnPlayerJoinRace(const KZ::racing::events::PlayerJoinRace &message)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Accepted", message.player.name.c_str());
}

void KZRacingService::OnPlayerLeaveRace(const KZ::racing::events::PlayerLeaveRace &message)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Unregistered", message.player.name.c_str());
}

void KZRacingService::OnPlayerFinish(const KZ::racing::events::PlayerFinish &message)
{
	CUtlString timeStr = utils::FormatTime(message.timeSeconds);

	if (message.teleports > 1)
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (2+ Teleports)", message.player.name.c_str(), timeStr.Get(), message.teleports);
	}
	else if (message.teleports == 1)
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (1 Teleport)", message.player.name.c_str(), timeStr.Get(), message.teleports);
	}
	else
	{
		KZLanguageService::PrintChatAll(true, "Racing - Player Finish (PRO)", message.player.name.c_str(), timeStr.Get(), message.teleports);
	}

	for (auto it = KZRacingService::currentRace.localParticipants.begin(); it != KZRacingService::currentRace.localParticipants.end(); ++it)
	{
		if (it->id == message.player.id)
		{
			KZRacingService::currentRace.localFinishers.push_back(*it);
			break;
		}
	}
}

void KZRacingService::OnPlayerDisconnect(const KZ::racing::events::PlayerDisconnect &message)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Disconnect", message.player.name.c_str());
	// Add to local finishers list to avoid showing go message.
	for (auto it = KZRacingService::currentRace.localParticipants.begin(); it != KZRacingService::currentRace.localParticipants.end(); ++it)
	{
		if (it->id == message.player.id)
		{
			KZRacingService::currentRace.localParticipants.erase(it);
			break;
		}
	}
}

void KZRacingService::OnPlayerSurrender(const KZ::racing::events::PlayerSurrender &message)
{
	KZLanguageService::PrintChatAll(true, "Racing - Player Surrender", message.player.name.c_str());
	// Add to local finishers list to avoid showing go message.
	for (auto it = KZRacingService::currentRace.localParticipants.begin(); it != KZRacingService::currentRace.localParticipants.end(); ++it)
	{
		if (it->id == message.player.id)
		{
			KZRacingService::currentRace.localFinishers.push_back(*it);
			break;
		}
	}
}
