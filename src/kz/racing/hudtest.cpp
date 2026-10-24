// Temporary commands that fake a race locally, so every state of the race panel can be checked without a coordinator.
// Delete this file and its AMBuilder line once the panel has been tested.
#include "kz_racing.h"
#include "kz/mode/kz_mode.h"
#include "kz/timer/kz_timer.h"
#include "utils/simplecmds.h"
#include "random.h"

#include "tier0/memdbgon.h"

static_function void RemoveFinisher(u64 steamID)
{
	auto &finishers = KZRacingService::currentRace.localFinishers;
	finishers.erase(std::remove(finishers.begin(), finishers.end(), steamID), finishers.end());
}

// Finishers come first, fastest first, the way the coordinator orders them. Some results have no points, to check that case too.
static_function void SendRandomResults(KZPlayer *player)
{
	static_persist const char *const NAMES[] = {"Alice", "Bob", "Charlie", "Dave", "Eve"};
	static_persist const KZ::racing::RaceResult::Status NON_FINISHED[] = {
		KZ::racing::RaceResult::Status::Surrendered, KZ::racing::RaceResult::Status::Disconnected, KZ::racing::RaceResult::Status::DidNotFinish};

	KZ::racing::events::RaceCompleted message;
	for (i32 i = 0; i <= KZ_ARRAYSIZE(NAMES); i++)
	{
		KZ::racing::RaceResult result;
		result.playerName = i == KZ_ARRAYSIZE(NAMES) ? player->GetName() : NAMES[i];
		if (RandomInt(0, 3) > 0)
		{
			result.status = KZ::racing::RaceResult::Status::Finished;
			result.timeSeconds = RandomFloat(30.0f, 300.0f);
			result.teleports = RandomInt(0, 5);
		}
		else
		{
			result.status = NON_FINISHED[RandomInt(0, KZ_ARRAYSIZE(NON_FINISHED) - 1)];
		}
		if (RandomInt(0, 4) > 0)
		{
			result.points = RandomInt(-20, 100);
		}
		message.results.push_back(result);
	}
	std::stable_sort(message.results.begin(), message.results.end(),
					 [](const KZ::racing::RaceResult &a, const KZ::racing::RaceResult &b)
					 {
						 const bool aFinished = a.status == KZ::racing::RaceResult::Status::Finished;
						 const bool bFinished = b.status == KZ::racing::RaceResult::Status::Finished;
						 if (aFinished != bFinished)
						 {
							 return aFinished;
						 }
						 return aFinished && *a.timeSeconds < *b.timeSeconds;
					 });
	KZRacingService::OnRaceCompleted(message);
}

static_function void AddParticipant(u64 steamID)
{
	auto &participants = KZRacingService::currentRace.localParticipants;
	if (std::find(participants.begin(), participants.end(), steamID) == participants.end())
	{
		participants.push_back(steamID);
	}
}

// kz_racetest init [max TPs, -1 for none] [time limit in seconds, 0 for none]
// kz_racetest join | leave | start [countdown seconds] | finish | unfinish | pb <seconds> [pro] | results | end
SCMD(kz_racetest, SCFL_RACING)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return true;
	}
	RaceInfo &race = KZRacingService::currentRace;
	const u64 steamID = player->GetSteamId64();
	const char *action = args->ArgC() >= 2 ? args->Arg(1) : "";

	if (KZ_STREQI(action, "init"))
	{
		const KZCourseDescriptor *course = player->timerService->GetCourse();
		if (!course)
		{
			course = KZ::course::GetFirstCourse();
		}
		if (!course)
		{
			player->PrintChat(false, false, "{darkred}No course on this map.");
			return true;
		}
		const i32 maxTeleports = args->ArgC() >= 3 ? V_atoi(args->Arg(2)) : -1;
		const i32 timeLimit = args->ArgC() >= 4 ? V_atoi(args->Arg(3)) : 300;

		race = {};
		race.state = RaceInfo::State::Init;
		race.conf.mapName = g_pKZUtils->GetCurrentMapName().Get();
		race.conf.courseName = course->name;
		race.conf.workshopID = g_pKZUtils->GetCurrentMapWorkshopID();
		race.conf.modeName = player->modeService->GetModeShortName();
		race.conf.format = KZ::racing::RaceConfig::Format::FastestTime;
		if (maxTeleports >= 0)
		{
			race.conf.maxTeleports = (u32)maxTeleports;
		}
		if (timeLimit > 0)
		{
			race.conf.maxDurationSeconds = (u32)timeLimit;
		}
		player->PrintChat(false, false, "{grey}Fake race on {default}%s{grey} (%s).", course->name, race.conf.modeName.c_str());
	}
	else if (KZ_STREQI(action, "results"))
	{
		// Ends the race the way a real one does, if there is one.
		SendRandomResults(player);
	}
	else if (race.state == RaceInfo::State::None)
	{
		player->PrintChat(false, false, "{grey}Run {default}kz_racetest init{grey} first.");
	}
	else if (KZ_STREQI(action, "join"))
	{
		AddParticipant(steamID);
	}
	else if (KZ_STREQI(action, "leave"))
	{
		KZRacingService::RemoveLocalRaceParticipant(steamID);
		RemoveFinisher(steamID);
	}
	else if (KZ_STREQI(action, "start"))
	{
		const i32 countdown = args->ArgC() >= 3 ? V_atoi(args->Arg(2)) : 5;
		AddParticipant(steamID);
		RemoveFinisher(steamID);
		race.state = RaceInfo::State::Ongoing;
		race.earliestStartTick = g_pKZUtils->GetServerGlobals()->tickcount + countdown * ENGINE_FIXED_TICK_RATE;
		player->timerService->TimerStop();
	}
	else if (KZ_STREQI(action, "finish"))
	{
		RemoveFinisher(steamID);
		race.localFinishers.push_back(steamID);
	}
	else if (KZ_STREQI(action, "unfinish"))
	{
		RemoveFinisher(steamID);
	}
	else if (KZ_STREQI(action, "pb"))
	{
		const KZCourseDescriptor *course = KZ::course::GetCourse(race.conf.courseName.c_str(), false);
		const PluginId modeID = KZ::mode::GetModeInfo(race.conf.modeName.c_str()).id;
		if (!course || args->ArgC() < 3)
		{
			player->PrintChat(false, false, "{grey}Usage: {default}kz_racetest pb <seconds> [pro]");
			return true;
		}
		const f64 time = V_atof(args->Arg(2));
		const bool pro = args->ArgC() >= 4 && KZ_STREQI(args->Arg(3), "pro");
		// Only the in-memory cache, so the next map load forgets it.
		player->timerService->InsertPBToCache(time, course, modeID, true, false);
		if (pro)
		{
			player->timerService->InsertPBToCache(time, course, modeID, false, false);
		}
	}
	else if (KZ_STREQI(action, "end"))
	{
		race = {};
	}
	else
	{
		player->PrintChat(
			false, false,
			"{grey}kz_racetest init [tps] [seconds] | join | leave | start [countdown] | finish | unfinish | pb <seconds> [pro] | results | end");
	}
	return true;
}
