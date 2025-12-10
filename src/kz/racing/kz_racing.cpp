#include "kz_racing.h"
#include "kz/timer/kz_timer.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "utils/argparse.h"
#include "utils/simplecmds.h"

CON_COMMAND_F(kz_race_init, "Initialize a race", FCVAR_NONE)
{
	// kz_race_init course=<course name> mode=<mode> [duration=<race duration, default to 0/infinite>] [tp=<number of max teleports, default to 0>]
	constexpr const char *paramKeys[] = {"course", "mode", "duration", "tp"};
	KeyValues3 params;
	if (!utils::ParseArgsToKV3(args.ArgS(), params, const_cast<const char **>(paramKeys), KZ_ARRAYSIZE(paramKeys)))
	{
		META_CONPRINTF("[KZ::Racing] Invalid arguments, usage: kz_race_init course=<course name> mode=<mode> [duration=<race duration, default to "
					   "0/infinite>] [tp=<number of max teleports, default to 0>]\n");
		return;
	}
	if (KZRacingService::currentRace.state > RaceInfo::RACE_NONE)
	{
		META_CONPRINTF("[KZ::Racing] Another race is already active, cannot initialize a new one.\n");
		return;
	}
	if (!params.FindMember("course") || !params.FindMember("mode"))
	{
		META_CONPRINTF("[KZ::Racing] Missing required arguments 'course' or 'mode'.\n");
		return;
	}
	std::string courseName = params.GetMemberString("course", "");
	std::string modeName = params.GetMemberString("mode", "");
	u32 maxTeleports = static_cast<u32>(params.GetMemberInt("tp", 0));
	f32 duration = static_cast<f32>(params.GetMemberFloat("duration", 0.0f));
	KZRacingService::SendRaceInit(g_pKZUtils->GetCurrentMapWorkshopID(), courseName, modeName, maxTeleports, duration);
}

CON_COMMAND_F(kz_race_cancel, "Cancel a race", FCVAR_NONE)
{
	KZRacingService::SendRaceCancel();
}

CON_COMMAND_F(kz_race_start, "Start the race", FCVAR_NONE)
{
	if (utils::IsNumeric(args.Arg(1)))
	{
		f32 countdownSeconds = static_cast<f32>(V_atoi(args.Arg(1)));
		KZRacingService::SendRaceStart(countdownSeconds);
	}
	else
	{
		META_CONPRINTF("[KZ::Racing] Invalid argument, usage: kz_race_start <countdown_seconds>\n");
	}

	if (KZRacingService::currentRace.state != RaceInfo::RACE_INIT)
	{
		META_CONPRINTF("[KZ::Racing] No race initialized, cannot start the race.\n");
	}

	f32 countdownSeconds = static_cast<f32>(V_atoi(args.Arg(1)));
	KZRacingService::SendRaceStart(countdownSeconds);
	META_CONPRINTF("[KZ::Racing] Requested race start with a countdown of %.2f seconds.\n", countdownSeconds);
}

CON_COMMAND_F(kz_race_end, "Manually end the race", FCVAR_NONE)
{
	KZRacingService::SendRaceEnd(true);
}

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

SCMD(kz_forfeit, SCFL_RACING)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	player->racingService->ForfeitRace();
	return MRES_SUPERCEDE;
}

void KZRacingService::SendRaceInit(u64 workshopID, std::string courseName, std::string modeName, u32 maxTeleports, f32 duration)
{
	KZ::racing::events::RaceInit raceInit;
	raceInit.raceInfo.workshopID = workshopID;
	raceInit.raceInfo.courseName = courseName;
	raceInit.raceInfo.modeName = modeName;
	raceInit.raceInfo.maxTeleports = maxTeleports;
	raceInit.raceInfo.duration = duration;
	KZRacingService::SendMessage("init_race", raceInit);
}

void KZRacingService::SendRaceCancel()
{
	KZ::racing::events::RaceCancel raceCancel;
	KZRacingService::SendMessage("cancel_race", raceCancel);
}

void KZRacingService::SendRaceJoin()
{
	KZ::racing::events::JoinRace data;
	KZRacingService::SendMessage("join_race", data);
}

void KZRacingService::SendRaceLeave()
{
	KZ::racing::events::LeaveRace data;
	KZRacingService::SendMessage("leave_race", data);
}

void KZRacingService::SendRaceStart(f32 countdownSeconds)
{
	KZ::racing::events::RaceStart raceStart;
	raceStart.countdownSeconds = countdownSeconds;
	KZRacingService::SendMessage("start_race", raceStart);
}

void KZRacingService::SendPlayerUnregistration()
{
	KZ::racing::events::PlayerUnregister data;
	data.player.id = this->player->GetSteamId64();
	data.player.name = this->player->GetName();
	KZRacingService::SendMessage("remove_race_participant", data);
}

void KZRacingService::SendAcceptRace()
{
	KZ::racing::events::PlayerAccept data;
	data.player.id = this->player->GetSteamId64();
	data.player.name = this->player->GetName();
	KZRacingService::SendMessage("add_race_participant", data);
}

void KZRacingService::SendForfeitRace()
{
	KZ::racing::events::PlayerForfeit playerForfeit;
	playerForfeit.player.id = this->player->GetSteamId64();
	playerForfeit.player.name = this->player->GetName();
	KZRacingService::SendMessage("player_forfeit", playerForfeit);
}

void KZRacingService::SendRaceFinish(f32 time, u32 teleportsUsed)
{
	KZ::racing::events::PlayerFinish playerFinish;
	playerFinish.player.id = this->player->GetSteamId64();
	playerFinish.player.name = this->player->GetName();
	playerFinish.time = time;
	playerFinish.teleportsUsed = teleportsUsed;
	KZRacingService::SendMessage("player_finished", playerFinish);
}

void KZRacingService::SendRaceEnd(bool manual)
{
	KZ::racing::events::RaceEnd raceEnd;
	raceEnd.manual = manual;
	KZRacingService::SendMessage("end_race", raceEnd);
}

void KZRacingService::SendChatMessage(const std::string &message)
{
	KZ::racing::events::ChatMessage chatMessage;
	chatMessage.player.id = this->player->GetSteamId64();
	chatMessage.player.name = this->player->GetName();
	chatMessage.message = message;
	KZRacingService::SendMessage("chat_message", chatMessage);
}

void KZRacingService::BroadcastRaceInfo()
{
	switch (KZRacingService::currentRace.state)
	{
		case RaceInfo::RACE_INIT:
		{
			// Broadcast the race info to all players.
			for (i32 i = 0; i < MAXPLAYERS + 1; i++)
			{
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
				if (player)
				{
					std::string timeLimitString;
					if (KZRacingService::currentRace.data.raceInfo.duration > 0.0f)
					{
						auto timeStr = utils::FormatTime(KZRacingService::currentRace.data.raceInfo.duration);
						timeLimitString = player->languageService->PrepareMessage("Racing - Time Limit", timeStr.Get());
					}
					else
					{
						timeLimitString = player->languageService->PrepareMessage("Racing - No Time Limit");
					}
					// clang-format off
					player->languageService->PrintAlert(false, false, "Racing - Race Info",
														KZRacingService::currentRace.data.raceInfo.courseName.c_str(),
														KZRacingService::currentRace.data.raceInfo.modeName.c_str(),
														KZRacingService::currentRace.data.raceInfo.maxTeleports,
														timeLimitString.c_str());
					// clang-format on
				}
			}
			break;
		}
		case RaceInfo::RACE_ONGOING:
		{
			// Broadcast to all participants that the race will start in X seconds.
			i32 countdownSeconds =
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
	if (KZRacingService::currentRace.state == RaceInfo::RACE_INIT && !this->IsRaceParticipant()
		&& g_pKZUtils->GetCurrentMapWorkshopID() == KZRacingService::currentRace.data.raceInfo.workshopID)
	{
		this->player->timerService->TimerStop();
		this->SendAcceptRace();
		KZRacingService::currentRace.localParticipants.push_back({this->player->GetSteamId64(), this->player->GetName()});
	}
}

void KZRacingService::ForfeitRace()
{
	if (KZRacingService::currentRace.state == RaceInfo::RACE_ONGOING && this->IsRaceParticipant())
	{
		this->player->timerService->TimerStop();
		this->SendForfeitRace();

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

bool KZRacingService::OnTimerStart(u32 courseGUID)
{
	if (!KZRacingService::currentRace.state)
	{
		return true;
	}
	if (!this->IsRaceParticipant())
	{
		return true;
	}
	// Check map and course match.
	if (!KZ::course::GetCourseByCourseID(courseGUID)
		|| KZRacingService::currentRace.data.raceInfo.courseName != KZ::course::GetCourseByCourseID(courseGUID)->name)
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
	this->timerStartTickServer = g_pKZUtils->GetServerGlobals()->tickcount;
	return true;
}

void KZRacingService::OnTimerEndPost(u32 courseGUID, f32 time, u32 teleportsUsed)
{
	if (!KZRacingService::currentRace.state)
	{
		return;
	}
	if (!this->IsRaceParticipant())
	{
		return;
	}
	this->SendRaceFinish(time + ((this->timerStartTickServer - KZRacingService::currentRace.earliestStartTick) * ENGINE_FIXED_TICK_INTERVAL),
						 teleportsUsed);
}

void KZRacingService::OnClientDisconnect()
{
	switch (KZRacingService::currentRace.state)
	{
		case RaceInfo::RACE_INIT:
		{
			// Unregister from the race if registered.
			if (this->IsRaceParticipant())
			{
				this->SendPlayerUnregistration();
				KZRacingService::RemoveLocalRaceParticipant(this->player->GetSteamId64());
			}
			break;
		}
		case RaceInfo::RACE_ONGOING:
		{
			if (!this->IsRaceParticipant())
			{
				return;
			}

			this->SendForfeitRace();
			break;
		}
		default:
			break;
	}
}
