#include "kz_racing.h"
#include "kz/timer/kz_timer.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/mode/kz_mode.h"
#include "kz/hud/kz_hud.h"
#include "kz/spec/kz_spec.h"
#include "utils/argparse.h"
#include "utils/simplecmds.h"

SCMD(kz_accept, SCFL_RACING)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return true;
	}

	player->racingService->AcceptRace();
	return true;
}

SCMD(kz_surrender, SCFL_RACING)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return true;
	}

	player->racingService->SurrenderRace();
	return true;
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
	KZ::racing::events::ChatMessage data(content, this->player->GetName());
	KZRacingService::SendMessage(data);
}

void KZRacingService::BroadcastRaceInfo()
{
	if (KZRacingService::currentRace.state == RaceInfo::State::None)
	{
		return;
	}

	const KZ::racing::RaceConfig &conf = KZRacingService::currentRace.conf;
	KZRacingService::hudCourse = KZ::course::GetCourse(conf.courseName.c_str(), false);
	KZRacingService::hudModeID = conf.modeName.empty() ? -2 : KZ::mode::GetModeInfo(conf.modeName.c_str()).id;
	for (KZPlayer *player : g_pKZPlayerManager->GetInGamePlayers())
	{
		if (player->IsFakeClient() || player->hudService->CanShowRaceHud())
		{
			continue;
		}
		HudInfo info;
		KZRacingService::GetHudInfo(player, info);
		player->racingService->DrawLegacyHud(info);
	}
}

void KZRacingService::GetHudInfo(KZPlayer *viewer, HudInfo &out)
{
	const RaceInfo &race = KZRacingService::currentRace;
	if (race.state == RaceInfo::State::None)
	{
		return;
	}
	KZPlayer *source = viewer->specService->GetSpectatedPlayer();
	if (!source)
	{
		source = viewer;
	}
	const bool participant = source->racingService->IsRaceParticipant();
	// Once the race has started, it is only shown for the players in it.
	if (race.state == RaceInfo::State::Ongoing && !participant)
	{
		return;
	}
	out.show = true;
	out.started = race.state == RaceInfo::State::Ongoing;
	out.formatPhrase =
		race.conf.format == KZ::racing::RaceConfig::Format::FastestTime ? "Racing HUD - Format Fastest Time" : "Racing HUD - Format First To Finish";
	out.course = race.conf.courseName.c_str();
	out.mode = race.conf.modeName.c_str();
	out.maxTeleports = race.conf.maxTeleports.has_value() ? (i32)*race.conf.maxTeleports : -1;
	out.timeLimit = race.conf.maxDurationSeconds.has_value() ? *race.conf.maxDurationSeconds : 0;

	if (race.state == RaceInfo::State::Init)
	{
		if (participant)
		{
			out.statusPhrase = "Racing HUD - Accepted";
			out.statusClass = "race-accepted";
		}
		else if (source == viewer && KZRacingService::IsMapCorrectForRace())
		{
			out.statusPhrase = "Racing HUD - Join Hint";
		}
		else
		{
			out.statusPhrase = "Racing HUD - Not Joined";
		}
		return;
	}

	const i32 tick = g_pKZUtils->GetServerGlobals()->tickcount;
	const MHUDPrefs &prefs = viewer->hudService->GetPrefs();
	if (prefs.raceTimeLeft && out.timeLimit > 0)
	{
		const f64 elapsed = Max(tick - race.earliestStartTick, 0) * ENGINE_FIXED_TICK_INTERVAL;
		out.showTimeLeft = true;
		// Rounded up, so it only reads 0:00 once the race is over.
		out.timeLeft = ceil(Max(out.timeLimit - elapsed, 0.0));
	}

	if (source->racingService->IsRaceFinisher())
	{
		out.statusPhrase = "Racing HUD - Finished";
		out.statusClass = "race-finished";
	}
	else
	{
		out.statusPhrase = "Racing HUD - Running";
		out.statusClass = "race-running";
		if (!source->timerService->GetTimerRunning())
		{
			out.countdown = true;
			out.countdownSeconds = (race.earliestStartTick - tick) * ENGINE_FIXED_TICK_INTERVAL;
		}
	}

	// A first to finish race is decided by who finishes first, so a personal best says nothing about it.
	if (prefs.racePB && race.conf.format == KZ::racing::RaceConfig::Format::FastestTime && KZRacingService::hudCourse
		&& KZRacingService::hudModeID >= 0)
	{
		// A race without teleports only compares against pro runs.
		out.pro = out.maxTeleports == 0;
		const PBDataKey key = ToPBDataKey(KZRacingService::hudModeID, KZRacingService::hudCourse->guid);
		for (bool global : {true, false})
		{
			const PBData *pb = source->timerService->GetCachedPB(key, global);
			const f64 time = pb ? (out.pro ? pb->pro.pbTime : pb->overall.pbTime) : 0;
			if (time > 0 && (out.pb <= 0 || time < out.pb))
			{
				out.pb = time;
			}
		}
	}
}

void KZRacingService::DrawLegacyHud(const HudInfo &info)
{
	if (!info.show)
	{
		return;
	}
	KZLanguageService *lang = this->player->languageService;
	if (KZRacingService::currentRace.state == RaceInfo::State::Ongoing)
	{
		if (info.countdown)
		{
			if (info.countdownSeconds > 0)
			{
				lang->PrintAlert(false, false, "Racing - Race Starting In", info.countdownSeconds);
			}
			else
			{
				lang->PrintAlert(false, false, "Racing - Go!");
			}
		}
		return;
	}

	std::string timeLimitString;
	if (info.timeLimit > 0)
	{
		timeLimitString = lang->PrepareMessage("Racing - Time Limit", utils::FormatTime(info.timeLimit, false).Get());
	}
	else
	{
		timeLimitString = lang->PrepareMessage("Racing - No Time Limit");
	}
	if (info.maxTeleports >= 0)
	{
		lang->PrintAlert(false, false, "Racing - Race Info - Teleport Limit", info.course, info.mode, info.maxTeleports, timeLimitString.c_str());
	}
	else
	{
		lang->PrintAlert(false, false, "Racing - Race Info - No Teleport Limit", info.course, info.mode, timeLimitString.c_str());
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

bool KZRacingService::IsRaceFinisher()
{
	for (u64 finisherSteamID : KZRacingService::currentRace.localFinishers)
	{
		if (finisherSteamID == this->player->GetSteamId64())
		{
			return true;
		}
	}
	return false;
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
	if (KZRacingService::currentRace.conf.maxTeleports.has_value()
		&& this->player->checkpointService->GetTeleportCount() >= KZRacingService::currentRace.conf.maxTeleports)
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
	if (!this->player->styleServices.empty())
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
	// First to finish is decided on who crosses the finish line first, so the time spent before starting the run counts
	// towards it. Fastest time only cares about the run itself.
	f32 reportedTime = time;

	if (KZRacingService::currentRace.conf.format == KZ::racing::RaceConfig::Format::FirstToFinish)
	{
		reportedTime += (this->timerStartTickServer - KZRacingService::currentRace.earliestStartTick) * ENGINE_FIXED_TICK_INTERVAL;
	}

	this->SendFinishRace(reportedTime, teleportsUsed);
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
