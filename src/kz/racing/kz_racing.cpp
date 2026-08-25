#include "kz_racing.h"
#include "kz/timer/kz_timer.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/mode/kz_mode.h"
#include "utils/argparse.h"
#include "utils/simplecmds.h"

SCMD(kz_accept, SCFL_RACING)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	player->racingService->AcceptRace();
	return MRES_SUPERCEDE;
}

SCMD(kz_surrender, SCFL_RACING)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	player->racingService->SurrenderRace();
	return MRES_SUPERCEDE;
}

void KZRacingService::SendReady()
{
	KZ::racing::events::PlayerReady data(this->player->GetSteamId64());
	KZRacingService::SendMessage(data);
}

void KZRacingService::SendDisconnected()
{
	KZ::racing::events::PlayerDisconnected data(this->player->GetSteamId64());
	KZRacingService::SendMessage(data);
}

void KZRacingService::SendSurrenderRace()
{
	KZ::racing::events::PlayerSurrendered data(this->player->GetSteamId64());
	KZRacingService::SendMessage(data);
}

void KZRacingService::SendFinishRace(f64 timeSeconds, u32 teleports)
{
	KZ::racing::events::PlayerFinished data(this->player->GetSteamId64(), timeSeconds, teleports);
	KZRacingService::SendMessage(data);
}

void KZRacingService::SendChatMessage(const std::string &content)
{
	KZ::racing::events::ChatMessage data(content, this->player->GetSteamId64());
	KZRacingService::SendMessage(data);
}

void KZRacingService::BroadcastRaceInfo()
{
	switch (KZRacingService::currentRace.state)
	{
		case RaceInfo::State::Init:
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
						player->languageService->PrintAlert(false, false, "Racing - Race Info - Teleport Limit",
															KZRacingService::currentRace.conf.courseName.c_str(),
															KZRacingService::currentRace.conf.modeName.c_str(),
															*KZRacingService::currentRace.conf.maxTeleports, timeLimitString.c_str());
					}
					else
					{
						player->languageService->PrintAlert(false, false, "Racing - Race Info - No Teleport Limit",
															KZRacingService::currentRace.conf.courseName.c_str(),
															KZRacingService::currentRace.conf.modeName.c_str(), timeLimitString.c_str());
					}
				}
			}
			break;
		}
		case RaceInfo::State::Ongoing:
		{
			// Broadcast to all participants that the race will start in X seconds.
			f32 countdownSeconds =
				(KZRacingService::currentRace.earliestStartTick - g_pKZUtils->GetServerGlobals()->tickcount) * ENGINE_FIXED_TICK_INTERVAL;
			if (countdownSeconds < 0)
			{
				countdownSeconds = 0;
			}

			for (u64 participantSteamID : KZRacingService::currentRace.localParticipants)
			{
				KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(participantSteamID);
				if (player && !player->timerService->GetTimerRunning())
				{
					if (countdownSeconds <= 0)
					{
						bool shouldAnnounce = true;
						for (u64 finisherSteamID : KZRacingService::currentRace.localFinishers)
						{
							if (finisherSteamID == player->GetSteamId64())
							{
								shouldAnnounce = false;
								continue;
							}
						}
						if (shouldAnnounce)
						{
							player->languageService->PrintAlert(false, true, "Racing - Go!");
						}
					}
					else
					{
						player->languageService->PrintAlert(false, true, "Racing - Race Starting In", countdownSeconds);
					}
				}
			}
			break;
		}
	}
}

void KZRacingService::AcceptRace()
{
	if (!this->player->IsAuthenticated())
	{
		this->player->languageService->PrintAlert(false, true, "Player Not Authenticated (Steam)", this->player->GetName());
		return;
	}

	if (KZRacingService::currentRace.state == RaceInfo::State::Init && !this->IsRaceParticipant()
		&& g_pKZUtils->GetCurrentMapWorkshopID() == KZRacingService::currentRace.conf.workshopID)
	{
		this->player->timerService->TimerStop();
		this->SendReady();
		KZRacingService::currentRace.localParticipants.push_back(this->player->GetSteamId64());
	}
}

void KZRacingService::SurrenderRace()
{
	if (KZRacingService::currentRace.state == RaceInfo::State::Ongoing && this->IsRaceParticipant())
	{
		this->player->timerService->TimerStop();
		this->SendSurrenderRace();

		auto &localParticipants = KZRacingService::currentRace.localParticipants;
		for (auto it = localParticipants.begin(); it != localParticipants.end(); it++)
		{
			if (*it == this->player->GetSteamId64())
			{
				localParticipants.erase(it);
				break;
			}
		}
	}
}

bool KZRacingService::IsRaceParticipant()
{
	bool isParticipating = false;
	for (u64 participantSteamID : KZRacingService::currentRace.localParticipants)
	{
		if (participantSteamID == this->player->GetSteamId64())
		{
			isParticipating = true;
			break;
		}
	}
	return isParticipating;
}

void KZRacingService::RemoveLocalRaceParticipant(u64 steamID)
{
	auto &participants = KZRacingService::currentRace.localParticipants;
	participants.erase(
		std::remove_if(participants.begin(), participants.end(), [steamID](u64 participantSteamID) { return participantSteamID == steamID; }),
		participants.end());
}

bool KZRacingService::CanTeleport()
{
	if (KZRacingService::currentRace.state == RaceInfo::State::None)
	{
		return true;
	}
	if (!this->IsRaceParticipant())
	{
		return true;
	}
	if (KZRacingService::currentRace.localFinishers.size() > 0)
	{
		for (u64 finisherSteamID : KZRacingService::currentRace.localFinishers)
		{
			if (finisherSteamID == this->player->GetSteamId64())
			{
				return true;
			}
		}
	}
	// Can't teleport if max teleports reached.
	if (this->player->checkpointService->GetTeleportCount() >= KZRacingService::currentRace.conf.maxTeleports)
	{
		return false;
	}
	return true;
}

bool KZRacingService::OnTimerStart(u32 courseGUID)
{
	if (KZRacingService::currentRace.state == RaceInfo::State::None)
	{
		return true;
	}
	if (!this->IsRaceParticipant())
	{
		return true;
	}
	// Check map and course match.
	if (!KZ::course::GetCourseByCourseID(courseGUID)
		|| !KZ_STREQI(KZ::course::GetCourseByCourseID(courseGUID)->name, KZRacingService::currentRace.conf.courseName.c_str()))
	{
		return false;
	}
	// Can't start before the earliest start tick.
	if (g_pKZUtils->GetServerGlobals()->tickcount < KZRacingService::currentRace.earliestStartTick)
	{
		return false;
	}
	// Styles are not supported.
	if (this->player->styleServices.Count() > 0)
	{
		return false;
	}
	// Mode mismatches are not supported.
	if (!KZ_STREQI(KZRacingService::currentRace.conf.modeName.c_str(), this->player->modeService->GetModeName())
		&& !KZ_STREQI(KZRacingService::currentRace.conf.modeName.c_str(), this->player->modeService->GetModeShortName()))
	{
		return false;
	}
	this->timerStartTickServer = g_pKZUtils->GetServerGlobals()->tickcount;
	return true;
}

void KZRacingService::OnTimerEndPost(u32 courseGUID, f32 time, u32 teleportsUsed)
{
	if (KZRacingService::currentRace.state == RaceInfo::State::None)
	{
		return;
	}
	if (!this->IsRaceParticipant())
	{
		return;
	}
	this->SendFinishRace(time + ((this->timerStartTickServer - KZRacingService::currentRace.earliestStartTick) * ENGINE_FIXED_TICK_INTERVAL),
						 teleportsUsed);
	// We do this in advance to avoid having to go text showing up for just finished players.
	for (auto it = KZRacingService::currentRace.localParticipants.begin(); it != KZRacingService::currentRace.localParticipants.end(); ++it)
	{
		if (*it == this->player->GetSteamId64())
		{
			KZRacingService::currentRace.localFinishers.push_back(*it);
			break;
		}
	}
}

void KZRacingService::OnClientDisconnect()
{
	switch (KZRacingService::currentRace.state)
	{
		case RaceInfo::State::Init:
		{
			// Unregister from the race if registered.
			if (this->IsRaceParticipant())
			{
				this->SendDisconnected();
				KZRacingService::RemoveLocalRaceParticipant(this->player->GetSteamId64());
			}
			break;
		}
		case RaceInfo::State::Ongoing:
		{
			if (!this->IsRaceParticipant())
			{
				return;
			}

			this->SendDisconnected();
			break;
		}
		default:
			break;
	}
}
