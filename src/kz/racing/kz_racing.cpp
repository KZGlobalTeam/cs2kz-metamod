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

void KZRacingService::SendInitRace(u32 workshopID, std::string courseName, std::string modeName, f64 maxDurationSeconds, u32 maxTeleports)
{
	KZ::racing::RaceSpec data;
	data.workshopID = workshopID;
	data.courseName = courseName;
	data.modeName = modeName;
	data.maxDurationSeconds = maxDurationSeconds;
	data.maxTeleports = maxTeleports;
	KZRacingService::SendMessage("init_race", data);
}

void KZRacingService::SendCancelRace()
{
	KZ::racing::events::CancelRace data;
	KZRacingService::SendMessage("cancel_race", data);
}

void KZRacingService::SendReady()
{
	KZ::racing::events::Ready data;
	KZRacingService::SendMessage("ready", data);
}

void KZRacingService::SendUnready()
{
	KZ::racing::events::Unready data;
	KZRacingService::SendMessage("unready", data);
}

void KZRacingService::SendJoinRace()
{
	KZ::racing::events::PlayerJoinRace data;
	data.player.id = this->player->GetSteamId64();
	data.player.name = this->player->GetName();
	KZRacingService::SendMessage("player_join_race", data);
}

void KZRacingService::SendLeaveRace()
{
	KZ::racing::events::PlayerLeaveRace data;
	data.player.id = this->player->GetSteamId64();
	data.player.name = this->player->GetName();
	KZRacingService::SendMessage("player_leave_race", data);
}

void KZRacingService::SendDisconnect()
{
	KZ::racing::events::PlayerDisconnect data;
	data.player.id = this->player->GetSteamId64();
	data.player.name = this->player->GetName();
	KZRacingService::SendMessage("player_disconnect", data);
}

void KZRacingService::SendSurrenderRace()
{
	KZ::racing::events::PlayerSurrender data;
	data.player.id = this->player->GetSteamId64();
	data.player.name = this->player->GetName();
	KZRacingService::SendMessage("player_surrender", data);
}

void KZRacingService::SendFinishRace(f64 timeSeconds, u32 teleports)
{
	KZ::racing::events::PlayerFinish data;
	data.player.id = this->player->GetSteamId64();
	data.player.name = this->player->GetName();
	data.timeSeconds = timeSeconds;
	data.teleports = teleports;
	KZRacingService::SendMessage("player_finish", data);
}

void KZRacingService::SendRaceFinished()
{
	KZ::racing::events::RaceFinished data;
	KZRacingService::SendMessage("race_finished", data);
}

void KZRacingService::SendChatMessage(const std::string &message)
{
	KZ::racing::events::ChatMessage data;
	data.player.id = this->player->GetSteamId64();
	data.player.name = this->player->GetName();
	data.content = message;
	KZRacingService::SendMessage("chat_message", data);
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
					if (KZRacingService::currentRace.spec.maxDurationSeconds > 0.0f)
					{
						auto timeStr = utils::FormatTime(KZRacingService::currentRace.spec.maxDurationSeconds, false);
						timeLimitString = player->languageService->PrepareMessage("Racing - Time Limit", timeStr.Get());
					}
					else
					{
						timeLimitString = player->languageService->PrepareMessage("Racing - No Time Limit");
					}
					// clang-format off
					player->languageService->PrintAlert(false, false, "Racing - Race Info",
														KZRacingService::currentRace.spec.courseName.c_str(),
														KZRacingService::currentRace.spec.modeName.c_str(),
														KZRacingService::currentRace.spec.maxTeleports,
														timeLimitString.c_str());
					// clang-format on
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

			for (const auto &participant : KZRacingService::currentRace.localParticipants)
			{
				KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(participant.id);
				if (player && !player->timerService->GetTimerRunning())
				{
					if (countdownSeconds <= 0)
					{
						for (auto &finisher : KZRacingService::currentRace.localFinishers)
						{
							if (finisher.id == player->GetSteamId64())
							{
								continue;
							}
						}
						player->languageService->PrintAlert(false, true, "Racing - Go!");
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
		&& g_pKZUtils->GetCurrentMapWorkshopID() == KZRacingService::currentRace.spec.workshopID)
	{
		this->player->timerService->TimerStop();
		this->SendJoinRace();
		KZRacingService::currentRace.localParticipants.push_back({this->player->GetSteamId64(), this->player->GetName()});
	}
}

void KZRacingService::SurrenderRace()
{
	if (KZRacingService::currentRace.state == RaceInfo::State::Ongoing && this->IsRaceParticipant())
	{
		this->player->timerService->TimerStop();
		this->SendSurrenderRace();

		auto &localParticipants = KZRacingService::currentRace.localParticipants;
		for (auto it = localParticipants.begin(); it != localParticipants.end();)
		{
			if (it->id == this->player->GetSteamId64())
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
	for (const auto &participant : KZRacingService::currentRace.localParticipants)
	{
		if (participant.id == this->player->GetSteamId64())
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
	participants.erase(std::remove_if(participants.begin(), participants.end(),
									  [steamID](const KZ::racing::PlayerInfo &participant) { return participant.id == steamID; }),
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
	// Can't teleport if max teleports reached.
	if (this->player->checkpointService->GetTeleportCount() >= KZRacingService::currentRace.spec.maxTeleports)
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
		|| KZRacingService::currentRace.spec.courseName != KZ::course::GetCourseByCourseID(courseGUID)->name)
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
	if (!KZ_STREQI(KZRacingService::currentRace.spec.modeName.c_str(), this->player->modeService->GetModeName())
		&& !KZ_STREQI(KZRacingService::currentRace.spec.modeName.c_str(), this->player->modeService->GetModeShortName()))
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
				this->SendLeaveRace();
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

			this->SendDisconnect();
			break;
		}
		default:
			break;
	}
}
