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
}

CON_COMMAND_F(kz_race_start, "Start the race", FCVAR_NONE)
{
	if (utils::IsNumeric(args.Arg(1)))
	{
		u32 countdownSeconds = static_cast<u32>(V_atoi(args.Arg(1)));
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

void KZRacingService::SendRaceInit(std::string mapName, u64 workshopID, std::string courseName, std::string modeName, u32 maxTeleports, f32 duration)
{
	KZ::racing::events::RaceInit raceInit;
	raceInit.raceInfo.mapName = mapName;
	raceInit.raceInfo.workshopID = workshopID;
	raceInit.raceInfo.courseName = courseName;
	raceInit.raceInfo.modeName = modeName;
	raceInit.raceInfo.maxTeleports = maxTeleports;
	raceInit.raceInfo.duration = duration;
	KZRacingService::SendMessage("race-init", raceInit);
}

void KZRacingService::SendMapUpdate(std::string mapName, u64 workshopID)
{
	KZ::racing::events::MapUpdated mapUpdated;
	mapUpdated.mapInfo.mapName = mapName;
	mapUpdated.mapInfo.workshopID = workshopID;
	KZRacingService::SendMessage("map-updated", mapUpdated);
}

void KZRacingService::SendRaceStart(u32 countdownSeconds)
{
	KZ::racing::events::RaceStart raceStart;
	raceStart.countdownSeconds = countdownSeconds;
	KZRacingService::SendMessage("race-start", raceStart);
}

void KZRacingService::SendPlayerRegistration()
{
	KZ::racing::PlayerInfo data;
	data.id = this->player->GetSteamId64();
	data.name = this->player->GetName();
	KZRacingService::SendMessage("player-register", data);
}

void KZRacingService::SendPlayerUnregistration()
{
	KZ::racing::PlayerInfo data;
	data.id = this->player->GetSteamId64();
	data.name = this->player->GetName();
	KZRacingService::SendMessage("player-unregister", data);
}

void KZRacingService::SendAcceptRace()
{
	KZ::racing::PlayerInfo data;
	data.id = this->player->GetSteamId64();
	data.name = this->player->GetName();
	KZRacingService::SendMessage("player-accept-race", data);
}

void KZRacingService::SendForfeitRace(bool manual)
{
	KZ::racing::events::PlayerForfeit playerForfeit;
	playerForfeit.player.id = this->player->GetSteamId64();
	playerForfeit.player.name = this->player->GetName();
	playerForfeit.manual = manual;
	KZRacingService::SendMessage("player-forfeit", playerForfeit);
}

void KZRacingService::SendRaceFinish(f32 time, u32 teleportsUsed)
{
	KZ::racing::events::PlayerFinish playerFinish;
	playerFinish.player.id = this->player->GetSteamId64();
	playerFinish.player.name = this->player->GetName();
	playerFinish.time = time;
	playerFinish.teleportsUsed = teleportsUsed;
	KZRacingService::SendMessage("player-finish", playerFinish);
}

void KZRacingService::SendRaceEnd(bool manual)
{
	KZ::racing::events::RaceEnd raceEnd;
	raceEnd.manual = manual;
	KZRacingService::SendMessage("race-end", raceEnd);
}

void KZRacingService::SendChatMessage(const std::string &message)
{
	KZ::racing::events::ChatMessage chatMessage;
	chatMessage.player.id = this->player->GetSteamId64();
	chatMessage.player.name = this->player->GetName();
	chatMessage.message = message;
	KZRacingService::SendMessage("chat-message", chatMessage);
}

void KZRacingService::OnRaceInit(const KZ::racing::events::RaceInit &raceInit)
{
	KZRacingService::currentRace.state = RaceInfo::RACE_INIT;
	KZRacingService::currentRace.data = raceInit;
	KZRacingService::currentRace.earliestStartTick = {};
}

void KZRacingService::OnRaceStart(const KZ::racing::events::RaceStart &raceStart)
{
	KZRacingService::currentRace.state = RaceInfo::RACE_ONGOING;
	KZRacingService::currentRace.earliestStartTick = g_pKZUtils->GetServerGlobals()->tickcount + raceStart.countdownSeconds * ENGINE_FIXED_TICK_RATE;
	KZLanguageService::PrintChatAll(false, "Racing - Race Countdown", raceStart.countdownSeconds);
}

void KZRacingService::OnPlayerForfeit(const KZ::racing::events::PlayerForfeit &playerForfeit)
{
	KZLanguageService::PrintChatAll(false, "Racing - Player Forfeit", playerForfeit.player.name.c_str());
}

void KZRacingService::OnPlayerFinish(const KZ::racing::events::PlayerFinish &playerFinish)
{
	KZLanguageService::PrintChatAll(false, "Racing - Player Finish", playerFinish.player.name.c_str(), playerFinish.time, playerFinish.teleportsUsed);
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
	KZLanguageService::PrintChatAll(false, "Racing - End Results Header");
	// Print first place to last place, then non-finishers.
	u32 position = 1;
	for (const auto &finisher : raceResult.finishers)
	{
		if (finisher.completed)
		{
			if (position == 1)
			{
				KZLanguageService::PrintChatAll(false, "Racing - End Results First Place", finisher.player.name.c_str(), finisher.time,
												finisher.teleportsUsed);
			}
			else if (position == finishers.size())
			{
				KZLanguageService::PrintChatAll(false, "Racing - End Results Last Place", position, finisher.player.name.c_str(), finisher.time,
												finisher.teleportsUsed);
			}
			else
			{
				KZLanguageService::PrintChatAll(false, "Racing - End Results Finisher", position, finisher.player.name.c_str(), finisher.time,
												finisher.teleportsUsed);
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
}

void KZRacingService::OnChatMessage(const KZ::racing::events::ChatMessage &chatMessage)
{
	utils::CPrintChatAll("{yellow}%s{default}: %s", chatMessage.player.name.c_str(), chatMessage.message.c_str());
}

void KZRacingService::BroadcastRaceInfo()
{
	switch (KZRacingService::currentRace.state)
	{
		case RaceInfo::RACE_INIT:
		{
			// clang-format off
			KZLanguageService::PrintAlertAll(false, "Racing - Race Info",
				KZRacingService::currentRace.data.raceInfo.mapName.c_str(),
				KZRacingService::currentRace.data.raceInfo.courseName.c_str(),
				KZRacingService::currentRace.data.raceInfo.modeName.c_str(),
				KZRacingService::currentRace.data.raceInfo.maxTeleports,
				KZRacingService::currentRace.data.raceInfo.duration
			);
			// clang-format on
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
		&& g_pKZUtils->GetCurrentMapName() == KZRacingService::currentRace.data.raceInfo.mapName.c_str())
	{
		this->player->timerService->TimerStop();
		this->SendAcceptRace();
		KZRacingService::currentRace.localParticipants.push_back({this->player->GetSteamId64(), this->player->GetName()});
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
	if (KZRacingService::currentRace.data.raceInfo.mapName != g_pKZUtils->GetCurrentMapName().Get() || !KZ::course::GetCourseByCourseID(courseGUID)
		|| KZRacingService::currentRace.data.raceInfo.courseName != KZ::course::GetCourseByCourseID(courseGUID)->name)
	{
		return false;
	}
	// Can't start before the earliest start tick.
	if (g_pKZUtils->GetServerGlobals()->tickcount < KZRacingService::currentRace.earliestStartTick)
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
	this->SendRaceFinish(time, teleportsUsed);
}

void KZRacingService::OnPlayerDisconnect()
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

			this->SendForfeitRace(false);
			break;
		}
		default:
			break;
	}
}
