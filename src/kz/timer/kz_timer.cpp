#include "kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/noclip/kz_noclip.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"
#include "kz/trigger/kz_trigger.h"
#include "kz/spec/kz_spec.h"
#include "announce.h"

#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

// clang-format off
constexpr const char *diffTextKeys[KZTimerService::CompareType::COMPARETYPE_COUNT] = {
	"",
	"Server PB Diff (Overall)",
	"Global PB Diff (Overall)",
	"SR Diff (Overall)",
	"WR Diff (Overall)"
};

constexpr const char *diffTextKeysPro[KZTimerService::CompareType::COMPARETYPE_COUNT] = {
	"",
	"Server PB Diff (Pro)",
	"Global PB Diff (Pro)",
	"SR Diff (Pro)",
	"WR Diff (Pro)"
};

constexpr const char *missedTimeKeys[KZTimerService::CompareType::COMPARETYPE_COUNT] = {
	"",
	"Missed Server PB (Overall)",
	"Missed Global PB (Overall)",
	"Missed SR (Overall)",
	"Missed WR (Overall)"
};

constexpr const char *missedTimeKeysPro[KZTimerService::CompareType::COMPARETYPE_COUNT] = {
	"",
	"Missed Server PB (Pro)",
	"Missed Global PB (Pro)",
	"Missed SR (Pro)",
	"Missed WR (Pro)"
};

constexpr const char *missedTimeKeysBoth[KZTimerService::CompareType::COMPARETYPE_COUNT] = {
	"",
	"Missed Server PB (Overall+Pro)",
	"Missed Global PB (Overall+Pro)",
	"Missed SR (Overall+Pro)",
	"Missed WR (Overall+Pro)"
};

// clang-format on

static_global class KZDatabaseServiceEventListener_Timer : public KZDatabaseServiceEventListener
{
public:
	virtual void OnMapSetup() override;
	virtual void OnClientSetup(Player *player, u64 steamID64, bool isCheater) override;
} databaseEventListener;

static_global class KZOptionServiceEventListener_Timer : public KZOptionServiceEventListener
{
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player)
	{
		player->timerService->OnPlayerPreferencesLoaded();
	}
} optionEventListener;

std::unordered_map<PBDataKey, PBData> KZTimerService::srCache;
std::unordered_map<PBDataKey, PBData> KZTimerService::wrCache;

static_global CUtlVector<KZTimerServiceEventListener *> eventListeners;

bool KZTimerService::RegisterEventListener(KZTimerServiceEventListener *eventListener)
{
	if (eventListeners.Find(eventListener) >= 0)
	{
		return false;
	}
	eventListeners.AddToTail(eventListener);
	return true;
}

bool KZTimerService::UnregisterEventListener(KZTimerServiceEventListener *eventListener)
{
	return eventListeners.FindAndRemove(eventListener);
}

void KZTimerService::StartZoneStartTouch(const KZCourseDescriptor *course)
{
	this->touchedGroundSinceTouchingStartZone = !!(this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND);
	this->TimerStop(false);
}

void KZTimerService::StartZoneEndTouch(const KZCourseDescriptor *course)
{
	if (this->touchedGroundSinceTouchingStartZone)
	{
		this->TimerStart(course);
	}
}

void KZTimerService::SplitZoneStartTouch(const KZCourseDescriptor *course, i32 splitNumber)
{
	if (!this->timerRunning || course->guid != this->currentCourseGUID)
	{
		return;
	}

	assert(splitNumber > INVALID_SPLIT_NUMBER && splitNumber < KZ_MAX_SPLIT_ZONES);

	if (this->splitZoneTimes[splitNumber - 1] < 0)
	{
		this->PlayReachedSplitSound();
		this->splitZoneTimes[splitNumber - 1] = this->GetTime();
		this->ShowSplitText(splitNumber);
		this->lastSplit = splitNumber;
	}
	CALL_FORWARD(eventListeners, OnSplitZoneTouchPost, this->player, splitNumber);
}

void KZTimerService::CheckpointZoneStartTouch(const KZCourseDescriptor *course, i32 cpNumber)
{
	if (!this->timerRunning || course->guid != this->currentCourseGUID)
	{
		return;
	}

	assert(cpNumber > INVALID_CHECKPOINT_NUMBER && cpNumber < KZ_MAX_CHECKPOINT_ZONES);

	if (this->cpZoneTimes[cpNumber - 1] < 0)
	{
		this->PlayReachedCheckpointSound();
		this->cpZoneTimes[cpNumber - 1] = this->GetTime();
		this->ShowCheckpointText(cpNumber);
		this->lastCheckpoint = cpNumber;
		this->reachedCheckpoints++;
	}
}

void KZTimerService::StageZoneStartTouch(const KZCourseDescriptor *course, i32 stageNumber)
{
	if (!this->timerRunning || course->guid != this->currentCourseGUID)
	{
		return;
	}

	assert(stageNumber > INVALID_STAGE_NUMBER && stageNumber < KZ_MAX_STAGE_ZONES);

	if (stageNumber > this->currentStage + 1)
	{
		this->PlayMissedZoneSound();
		this->player->languageService->PrintChat(true, false, "Touched too high stage number (Missed stage)", this->currentStage + 1);
		return;
	}

	if (stageNumber == this->currentStage + 1)
	{
		this->stageZoneTimes[this->currentStage] = this->GetTime();
		this->PlayReachedStageSound();
		this->ShowStageText();
		this->currentStage++;
	}
}

bool KZTimerService::TimerStart(const KZCourseDescriptor *courseDesc, bool playSound)
{
	// clang-format off
	if (!this->player->GetPlayerPawn()->IsAlive()
		|| this->JustStartedTimer()
		|| this->player->JustTeleported()
		|| this->player->inPerf
		|| this->player->noclipService->JustNoclipped()
		|| !this->HasValidMoveType()
		|| this->JustLanded()
		|| (this->GetTimerRunning() && courseDesc->guid == this->currentCourseGUID)
		|| (!(this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND) && !this->GetValidJump()))
	// clang-format on
	{
		return false;
	}
	if (V_strlen(this->player->modeService->GetModeName()) > KZ_MAX_MODE_NAME_LENGTH)
	{
		Warning("[KZ] Timer start failed: Mode name is too long!");
		return false;
	}

	bool allowStart = true;
	FOR_EACH_VEC(eventListeners, i)
	{
		allowStart &= eventListeners[i]->OnTimerStart(this->player, courseDesc->guid);
	}
	if (!allowStart)
	{
		return false;
	}

	// In CKZ you can touch trigger in half tick intervals, but here we are incrementing by full tick intervals only.
	// Since the player was still in the trigger for half a tick, we need to offset by half a tick if we started in a half tick.
	// So the current time should be subtracted by the difference between server curtime and client curtime at the moment of starting the timer,
	// That way when we increment by full tick intervals in OnPhysicsSimulatePost, the time will be correct.
	this->currentTime = g_pKZUtils->GetGlobals()->curtime - g_pKZUtils->GetServerGlobals()->curtime;
	assert(this->currentTime <= 0 && this->currentTime > -ENGINE_FIXED_TICK_INTERVAL);
	this->timerRunning = true;
	this->currentStage = 0;
	this->reachedCheckpoints = 0;
	this->lastCheckpoint = 0;
	this->lastSplit = 0;

	f64 invalidTime = -1;
	this->splitZoneTimes.SetSize(courseDesc->splitCount);
	this->cpZoneTimes.SetSize(courseDesc->checkpointCount);
	this->stageZoneTimes.SetSize(courseDesc->checkpointCount);

	this->splitZoneTimes.FillWithValue(invalidTime);
	this->cpZoneTimes.FillWithValue(invalidTime);
	this->stageZoneTimes.FillWithValue(invalidTime);

	SetCourse(courseDesc->guid);
	this->validTime = true;
	this->shouldAnnounceMissedTime = true;
	this->shouldAnnounceMissedProTime = true;

	this->UpdateCurrentCompareType(ToPBDataKey(KZ::mode::GetModeInfo(this->player->modeService).id, courseDesc->guid));

	if (playSound)
	{
		for (KZPlayer *spec = player->specService->GetNextSpectator(NULL); spec != NULL; spec = player->specService->GetNextSpectator(spec))
		{
			player->timerService->PlayTimerStartSound();
		}
		this->PlayTimerStartSound();
	}

	if (!this->player->IsAuthenticated())
	{
		this->player->languageService->PrintChat(true, false, "No Steam Authentication Warning");
	}
	if (KZGlobalService::IsAvailable() && !this->player->hasPrime)
	{
		this->player->languageService->PrintChat(true, false, "No Prime Warning");
	}

	FOR_EACH_VEC(eventListeners, i)
	{
		eventListeners[i]->OnTimerStartPost(this->player, courseDesc->guid);
	}
	return true;
}

bool KZTimerService::TimerEnd(const KZCourseDescriptor *courseDesc)
{
	if (!this->player->IsAlive())
	{
		return false;
	}

	if (!this->timerRunning || courseDesc->guid != this->currentCourseGUID)
	{
		for (KZPlayer *spec = player->specService->GetNextSpectator(NULL); spec != NULL; spec = player->specService->GetNextSpectator(spec))
		{
			player->timerService->PlayTimerFalseEndSound();
		}
		this->PlayTimerFalseEndSound();
		this->lastFalseEndTime = g_pKZUtils->GetServerGlobals()->curtime;
		return false;
	}

	if (this->currentStage != courseDesc->stageCount)
	{
		this->PlayMissedZoneSound();
		this->player->languageService->PrintChat(true, false, "Can't Finish Run (Missed Stage)", this->currentStage + 1);
		return false;
	}

	if (this->reachedCheckpoints != courseDesc->checkpointCount)
	{
		this->PlayMissedZoneSound();
		i32 missCount = courseDesc->checkpointCount - this->reachedCheckpoints;
		if (missCount == 1)
		{
			this->player->languageService->PrintChat(true, false, "Can't Finish Run (Missed a Checkpoint Zone)");
		}
		else
		{
			this->player->languageService->PrintChat(true, false, "Can't Finish Run (Missed Checkpoint Zones)", missCount);
		}
		return false;
	}

	f32 time = this->GetTime() + g_pKZUtils->GetServerGlobals()->frametime;
	u32 teleportsUsed = this->player->checkpointService->GetTeleportCount();

	bool allowEnd = true;
	FOR_EACH_VEC(eventListeners, i)
	{
		allowEnd &= eventListeners[i]->OnTimerEnd(this->player, this->currentCourseGUID, time, teleportsUsed);
	}
	if (!allowEnd)
	{
		return false;
	}
	// Update current time for one last time.
	this->currentTime = time;

	this->timerRunning = false;
	this->lastEndTime = g_pKZUtils->GetServerGlobals()->curtime;

	for (KZPlayer *spec = player->specService->GetNextSpectator(NULL); spec != NULL; spec = player->specService->GetNextSpectator(spec))
	{
		player->timerService->PlayTimerEndSound();
	}
	this->PlayTimerEndSound();

	if (!this->player->GetPlayerPawn()->IsBot())
	{
		RecordAnnounce::Create(this->player);
	}

	FOR_EACH_VEC(eventListeners, i)
	{
		eventListeners[i]->OnTimerEndPost(this->player, this->currentCourseGUID, time, teleportsUsed);
	}

	return true;
}

bool KZTimerService::TimerStop(bool playSound)
{
	if (!this->timerRunning)
	{
		return false;
	}
	this->timerRunning = false;
	if (playSound)
	{
		for (KZPlayer *spec = player->specService->GetNextSpectator(NULL); spec != NULL; spec = player->specService->GetNextSpectator(spec))
		{
			player->timerService->PlayTimerStopSound();
		}
		this->PlayTimerStopSound();
	}

	FOR_EACH_VEC(eventListeners, i)
	{
		eventListeners[i]->OnTimerStopped(this->player, this->currentCourseGUID);
	}

	return true;
}

void KZTimerService::TimerStopAll(bool playSound)
{
	for (int i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player || !player->timerService)
		{
			continue;
		}
		player->timerService->TimerStop(playSound);
	}
}

void KZTimerService::InvalidateJump()
{
	this->validJump = false;
	this->lastInvalidateTime = g_pKZUtils->GetServerGlobals()->curtime;
}

void KZTimerService::PlayTimerStartSound()
{
	if (g_pKZUtils->GetServerGlobals()->curtime - this->lastStartSoundTime > KZ_TIMER_SOUND_COOLDOWN)
	{
		utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_START);
		this->lastStartSoundTime = g_pKZUtils->GetServerGlobals()->curtime;
	}
}

void KZTimerService::InvalidateRun()
{
	if (!this->validTime)
	{
		return;
	}
	this->validTime = false;

	FOR_EACH_VEC(eventListeners, i)
	{
		eventListeners[i]->OnTimerInvalidated(this->player);
	}
}

bool KZTimerService::HasValidMoveType()
{
	return KZTimerService::IsValidMoveType(this->player->GetMoveType());
}

bool KZTimerService::JustEndedTimer()
{
	return g_pKZUtils->GetServerGlobals()->curtime - this->lastEndTime > 1.0f;
}

void KZTimerService::PlayTimerEndSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_END);
}

void KZTimerService::PlayTimerFalseEndSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_FALSE_END);
}

void KZTimerService::PlayMissedZoneSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_MISSED_ZONE);
}

void KZTimerService::PlayReachedSplitSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_REACH_SPLIT);
}

void KZTimerService::PlayReachedCheckpointSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_REACH_CHECKPOINT);
}

void KZTimerService::PlayReachedStageSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_REACH_STAGE);
}

void KZTimerService::PlayTimerStopSound()
{
	if (this->shouldPlayTimerStopSound)
	{
		utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_STOP);
	}
}

void KZTimerService::PlayMissedTimeSound()
{
	if (g_pKZUtils->GetServerGlobals()->curtime - this->lastMissedTimeSoundTime > KZ_TIMER_SOUND_COOLDOWN)
	{
		utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_TIMER_SND_MISSED_TIME);
		this->lastMissedTimeSoundTime = g_pKZUtils->GetServerGlobals()->curtime;
	}
}

void KZTimerService::FormatTime(f64 time, char *output, u32 length, bool precise)
{
	int roundedTime = RoundFloatToInt(time * 1000); // Time rounded to number of ms

	int milliseconds = roundedTime % 1000;
	roundedTime = (roundedTime - milliseconds) / 1000;
	int seconds = roundedTime % 60;
	roundedTime = (roundedTime - seconds) / 60;
	int minutes = roundedTime % 60;
	roundedTime = (roundedTime - minutes) / 60;
	int hours = roundedTime;

	if (hours == 0)
	{
		if (precise)
		{
			snprintf(output, length, "%02i:%02i.%03i", minutes, seconds, milliseconds);
		}
		else
		{
			snprintf(output, length, "%i:%02i", minutes, seconds);
		}
	}
	else
	{
		if (precise)
		{
			snprintf(output, length, "%i:%02i:%02i.%03i", hours, minutes, seconds, milliseconds);
		}
		else
		{
			snprintf(output, length, "%i:%02i:%02i", hours, minutes, seconds);
		}
	}
}

static_function std::string GetTeleportCountText(int tpCount, const char *language)
{
	return tpCount == 1 ? KZLanguageService::PrepareMessageWithLang(language, "1 Teleport Text")
						: KZLanguageService::PrepareMessageWithLang(language, "2+ Teleports Text", tpCount);
}

void KZTimerService::Pause()
{
	if (!this->CanPause(true))
	{
		return;
	}

	bool allowPause = true;
	FOR_EACH_VEC(eventListeners, i)
	{
		allowPause &= eventListeners[i]->OnPause(this->player);
	}
	if (!allowPause)
	{
		this->player->languageService->PrintChat(true, false, "Can't Pause (Generic)");
		this->player->PlayErrorSound();
		return;
	}

	this->paused = true;
	this->pausedOnLadder = this->player->GetMoveType() == MOVETYPE_LADDER;
	this->lastDuckValue = this->player->GetMoveServices()->m_flDuckAmount;
	this->lastStaminaValue = this->player->GetMoveServices()->m_flStamina;
	this->player->SetVelocity(vec3_origin);
	this->player->SetMoveType(MOVETYPE_NONE);
	this->player->GetPlayerPawn()->SetGravityScale(0);

	if (this->GetTimerRunning())
	{
		this->hasPausedInThisRun = true;
		this->lastPauseTime = g_pKZUtils->GetServerGlobals()->curtime;
	}

	FOR_EACH_VEC(eventListeners, i)
	{
		eventListeners[i]->OnPausePost(this->player);
	}
}

bool KZTimerService::CanPause(bool showError)
{
	if (this->paused)
	{
		return false;
	}

	if (this->player->triggerService->InAntiPauseArea())
	{
		if (showError)
		{
			this->player->languageService->PrintChat(true, false, "Can't Pause (Anti Pause Area)");
			this->player->PlayErrorSound();
		}
		return false;
	}

	Vector velocity;
	this->player->GetVelocity(&velocity);

	if (this->GetTimerRunning())
	{
		if (this->hasResumedInThisRun && g_pKZUtils->GetServerGlobals()->curtime - this->lastResumeTime < KZ_PAUSE_COOLDOWN)
		{
			if (showError)
			{
				this->player->languageService->PrintChat(true, false, "Can't Pause (Just Resumed)");
				this->player->PlayErrorSound();
			}
			return false;
		}
		else if (!(this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND) && !(velocity.Length2D() == 0.0f && velocity.z == 0.0f))
		{
			if (showError)
			{
				this->player->languageService->PrintChat(true, false, "Can't Pause (Midair)");
				this->player->PlayErrorSound();
			}
			return false;
		}
	}
	return true;
}

void KZTimerService::Resume(bool force)
{
	if (!this->paused)
	{
		return;
	}
	if (!force && !this->CanResume(true))
	{
		return;
	}

	bool allowResume = true;
	FOR_EACH_VEC(eventListeners, i)
	{
		allowResume &= eventListeners[i]->OnResume(this->player);
	}
	if (!allowResume)
	{
		this->player->languageService->PrintChat(true, false, "Can't Resume (Generic)");
		this->player->PlayErrorSound();
		return;
	}

	if (this->pausedOnLadder)
	{
		this->player->SetMoveType(MOVETYPE_LADDER);
	}
	else
	{
		this->player->SetMoveType(MOVETYPE_WALK);
	}

	// GOKZ: prevent noclip exploit
	this->player->GetPlayerPawn()->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_STANDARD;
	this->player->GetPlayerPawn()->CollisionRulesChanged();

	this->paused = false;
	if (this->GetTimerRunning())
	{
		this->hasResumedInThisRun = true;
		this->lastResumeTime = g_pKZUtils->GetServerGlobals()->curtime;
	}
	this->player->GetMoveServices()->m_flDuckAmount = this->lastDuckValue;
	this->player->GetMoveServices()->m_flStamina = this->lastStaminaValue;

	FOR_EACH_VEC(eventListeners, i)
	{
		eventListeners[i]->OnResumePost(this->player);
	}
}

bool KZTimerService::CanResume(bool showError)
{
	if (this->GetTimerRunning() && this->hasPausedInThisRun && g_pKZUtils->GetServerGlobals()->curtime - this->lastPauseTime < KZ_PAUSE_COOLDOWN)
	{
		if (showError)
		{
			this->player->languageService->PrintChat(true, false, "Can't Resume (Just Paused)");
			this->player->PlayErrorSound();
		}
		return false;
	}
	return true;
}

SCMD(kz_timerstopsound, SCFL_TIMER | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->timerService->ToggleTimerStopSound();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_tss, kz_toggletimerstopsound);

void KZTimerService::ToggleTimerStopSound()
{
	this->shouldPlayTimerStopSound = !this->shouldPlayTimerStopSound;
	this->player->optionService->SetPreferenceBool("timerStopSound", this->shouldPlayTimerStopSound);
	this->player->languageService->PrintChat(true, false, this->shouldPlayTimerStopSound ? "Timer Stop Sound Enabled" : "Timer Stop Sound Disabled");
}

void KZTimerService::Reset()
{
	this->timerRunning = {};
	this->currentTime = {};
	this->currentCourseGUID = 0;
	this->lastEndTime = {};
	this->lastFalseEndTime = {};
	this->lastStartSoundTime = {};
	this->lastMissedTimeSoundTime = {};
	this->validTime = {};
	this->paused = {};
	this->pausedOnLadder = {};
	this->lastPauseTime = {};
	this->hasPausedInThisRun = {};
	this->lastResumeTime = {};
	this->hasResumedInThisRun = {};
	this->lastDuckValue = {};
	this->lastStaminaValue = {};
	this->validJump = {};
	this->lastInvalidateTime = {};
	this->touchedGroundSinceTouchingStartZone = {};
	this->shouldPlayTimerStopSound = true;
}

void KZTimerService::OnPhysicsSimulatePost()
{
	if (this->player->IsAlive() && this->GetTimerRunning() && !this->GetPaused())
	{
		this->currentTime += ENGINE_FIXED_TICK_INTERVAL;
		this->CheckMissedTime();
	}
}

void KZTimerService::OnStartTouchGround()
{
	this->touchedGroundSinceTouchingStartZone = true;
}

void KZTimerService::OnStopTouchGround()
{
	if (this->HasValidMoveType() && this->lastInvalidateTime != g_pKZUtils->GetServerGlobals()->curtime)
	{
		this->validJump = true;
	}
	else
	{
		this->InvalidateJump();
	}
}

void KZTimerService::OnChangeMoveType(MoveType_t oldMoveType)
{
	if (oldMoveType == MOVETYPE_LADDER && this->player->GetMoveType() == MOVETYPE_WALK
		&& this->lastInvalidateTime != g_pKZUtils->GetServerGlobals()->curtime)
	{
		this->validJump = true;
	}
	else
	{
		this->InvalidateJump();
	}
	// Check if player has escaped MOVETYPE_NONE
	if (!this->paused || this->player->GetMoveType() == MOVETYPE_NONE)
	{
		return;
	}

	this->paused = false;
	if (this->GetTimerRunning())
	{
		this->hasResumedInThisRun = true;
		this->lastResumeTime = g_pKZUtils->GetServerGlobals()->curtime;
	}

	FOR_EACH_VEC(eventListeners, i)
	{
		eventListeners[i]->OnResumePost(this->player);
	}
}

void KZTimerService::OnTeleportToStart()
{
	this->TimerStop();
}

void KZTimerService::OnClientDisconnect()
{
	this->TimerStop();
}

void KZTimerService::OnPlayerSpawn()
{
	if (!this->player->GetPlayerPawn() || !this->paused)
	{
		return;
	}

	// Player has left paused state by spawning in, so resume
	this->paused = false;
	if (this->GetTimerRunning())
	{
		this->hasResumedInThisRun = true;
		this->lastResumeTime = g_pKZUtils->GetServerGlobals()->curtime;
	}
	this->player->GetMoveServices()->m_flDuckAmount = this->lastDuckValue;
	this->player->GetMoveServices()->m_flStamina = this->lastStaminaValue;

	FOR_EACH_VEC(eventListeners, i)
	{
		eventListeners[i]->OnResumePost(this->player);
	}
}

void KZTimerService::OnPlayerJoinTeam(i32 team)
{
	if (team == CS_TEAM_SPECTATOR)
	{
		this->paused = true;
		if (this->GetTimerRunning())
		{
			this->hasPausedInThisRun = true;
			this->lastPauseTime = g_pKZUtils->GetServerGlobals()->curtime;
		}

		FOR_EACH_VEC(eventListeners, i)
		{
			eventListeners[i]->OnPausePost(this->player);
		}
	}
}

void KZTimerService::OnPlayerDeath()
{
	this->TimerStop();
}

void KZTimerService::OnRoundStart()
{
	KZTimerService::TimerStopAll();
}

void KZTimerService::OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
{
	if (newPosition || newVelocity)
	{
		this->InvalidateJump();
	}
}

SCMD(kz_stop, SCFL_TIMER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (player->timerService->GetTimerRunning())
	{
		player->timerService->TimerStop();
	}
	return MRES_SUPERCEDE;
}

SCMD(kz_pause, SCFL_TIMER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->timerService->TogglePause();
	return MRES_SUPERCEDE;
}

SCMD(kz_comparelevel, SCFL_RECORD | SCFL_TIMER | SCFL_PREFERENCE | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->timerService->SetCompareTarget(args->Arg(1));
	return MRES_SUPERCEDE;
}

static_function KZTimerService::CompareType GetCompareTypeFromString(const char *typeString)
{
	if (V_stricmp("off", typeString) == 0 || V_stricmp("none", typeString) == 0)
	{
		return KZTimerService::CompareType::COMPARE_NONE;
	}
	if (V_stricmp("spb", typeString) == 0)
	{
		return KZTimerService::CompareType::COMPARE_SPB;
	}
	if (V_stricmp("gpb", typeString) == 0 || V_stricmp("pb", typeString) == 0)
	{
		return KZTimerService::CompareType::COMPARE_GPB;
	}
	if (V_stricmp("sr", typeString) == 0)
	{
		return KZTimerService::CompareType::COMPARE_SR;
	}
	if (V_stricmp("wr", typeString) == 0)
	{
		return KZTimerService::CompareType::COMPARE_WR;
	}
	return KZTimerService::CompareType::COMPARETYPE_COUNT;
}

void KZTimerService::SetCompareTarget(const char *typeString)
{
	if (!typeString || !V_stricmp("", typeString))
	{
		this->player->languageService->PrintChat(true, false, "Compare Command Usage");
		return;
	}

	CompareType type = GetCompareTypeFromString(typeString);
	if (type == COMPARETYPE_COUNT)
	{
		this->player->languageService->PrintChat(true, false, "Compare Command Usage");
		return;
	}

	assert(type < COMPARETYPE_COUNT && type >= COMPARE_NONE);
	switch (type)
	{
		case COMPARE_NONE:
		{
			this->player->languageService->PrintChat(true, false, "Compare Disabled");
			break;
		}
		case COMPARE_SPB:
		{
			this->player->languageService->PrintChat(true, false, "Compare Server PB");
			break;
		}
		case COMPARE_GPB:
		{
			this->player->languageService->PrintChat(true, false, "Compare Global PB");
			break;
		}
		case COMPARE_SR:
		{
			this->player->languageService->PrintChat(true, false, "Compare Server Record");
			break;
		}
		case COMPARE_WR:
		{
			this->player->languageService->PrintChat(true, false, "Compare World Record");
			break;
		}
	}
	this->preferredCompareType = type;
	this->player->optionService->SetPreferenceInt("preferredCompareType", this->preferredCompareType);
	if (this->GetCourse())
	{
		this->UpdateCurrentCompareType(ToPBDataKey(KZ::mode::GetModeInfo(this->player->modeService).id, this->GetCourse()->guid));
	}
}

void KZTimerService::UpdateCurrentCompareType(PBDataKey key)
{
	for (u8 type = this->preferredCompareType; type > COMPARE_NONE; type--)
	{
		if (this->GetCompareTargetForType((CompareType)type, key))
		{
			this->currentCompareType = (CompareType)type;
			return;
		}
	}
	this->currentCompareType = COMPARE_NONE;
}

const PBData *KZTimerService::GetCompareTargetForType(CompareType type, PBDataKey key)
{
	switch (type)
	{
		case COMPARE_WR:
		{
			if (KZTimerService::wrCache.find(key) != KZTimerService::wrCache.end())
			{
				return &KZTimerService::wrCache[key];
			}
			break;
		}
		case COMPARE_SR:
		{
			if (KZTimerService::srCache.find(key) != KZTimerService::srCache.end())
			{
				return &KZTimerService::srCache[key];
			}
			break;
		}
		case COMPARE_GPB:
		{
			if (KZTimerService::globalPBCache.find(key) != KZTimerService::globalPBCache.end())
			{
				return &this->globalPBCache[key];
			}
			break;
		}
		case COMPARE_SPB:
		{
			if (KZTimerService::localPBCache.find(key) != KZTimerService::localPBCache.end())
			{
				return &this->localPBCache[key];
			}
			break;
		}
	}
	return nullptr;
}

const PBData *KZTimerService::GetCompareTarget(PBDataKey key)
{
	switch (this->currentCompareType)
	{
		case COMPARE_WR:
		{
			if (KZTimerService::wrCache.find(key) != KZTimerService::wrCache.end())
			{
				return &KZTimerService::wrCache[key];
			}
			break;
		}
		case COMPARE_SR:
		{
			if (KZTimerService::srCache.find(key) != KZTimerService::srCache.end())
			{
				return &KZTimerService::srCache[key];
			}
			break;
		}
		case COMPARE_GPB:
		{
			if (KZTimerService::globalPBCache.find(key) != KZTimerService::globalPBCache.end())
			{
				return &this->globalPBCache[key];
			}
			break;
		}
		case COMPARE_SPB:
		{
			if (KZTimerService::localPBCache.find(key) != KZTimerService::localPBCache.end())
			{
				return &this->localPBCache[key];
			}
			break;
		}
	}
	return nullptr;
}

void KZTimerService::ClearRecordCache()
{
	KZTimerService::srCache.clear();
	KZTimerService::wrCache.clear();
}

void KZTimerService::UpdateLocalRecordCache()
{
	auto onQuerySuccess = [](std::vector<ISQLQuery *> queries)
	{
		ISQLResult *result = queries[0]->GetResultSet();
		if (result && result->GetRowCount() > 0)
		{
			while (result->FetchRow())
			{
				auto modeInfo = KZ::mode::GetModeInfoFromDatabaseID(result->GetInt(2));
				if (modeInfo.databaseID < 0)
				{
					continue;
				}
				const KZCourseDescriptor *course = KZ::course::GetCourseByLocalCourseID(result->GetInt(1));
				if (!course)
				{
					continue;
				}
				KZTimerService::InsertRecordToCache(result->GetFloat(0), course, modeInfo.id, true, false, result->GetString(3));
			}
		}
		result = queries[1]->GetResultSet();
		if (result && result->GetRowCount() > 0)
		{
			while (result->FetchRow())
			{
				auto modeInfo = KZ::mode::GetModeInfoFromDatabaseID(result->GetInt(2));
				if (modeInfo.databaseID < 0)
				{
					continue;
				}
				const KZCourseDescriptor *course = KZ::course::GetCourseByLocalCourseID(result->GetInt(1));
				if (!course)
				{
					continue;
				}
				KZTimerService::InsertRecordToCache(result->GetFloat(0), course, modeInfo.id, false, false, result->GetString(3));
			}
		}
	};
	KZDatabaseService::QueryAllRecords(g_pKZUtils->GetCurrentMapName(), onQuerySuccess, KZDatabaseService::OnGenericTxnFailure);
}

void KZTimerService::InsertRecordToCache(f64 time, const KZCourseDescriptor *course, PluginId modeID, bool overall, bool global, CUtlString metadata)
{
	PBData &pb = global ? KZTimerService::wrCache[ToPBDataKey(modeID, course->guid)] : KZTimerService::srCache[ToPBDataKey(modeID, course->guid)];

	overall ? pb.overall.pbTime = time : pb.pro.pbTime = time;
	KeyValues3 kv(KV3_TYPEEX_TABLE, KV3_SUBTYPE_UNSPECIFIED);
	CUtlString error = "";
	if (metadata.IsEmpty())
	{
		return;
	}
	LoadKV3FromJSON(&kv, &error, metadata.Get(), "");
	if (!error.IsEmpty())
	{
		META_CONPRINTF("[KZ::Timer] Failed to insert PB to cache due to metadata error: %s\n", error.Get());
		return;
	}

	KeyValues3 *data = kv.FindMember("splitZoneTimes");
	if (data && data->GetType() == KV3_TYPE_ARRAY)
	{
		for (i32 i = 0; i < course->splitCount; i++)
		{
			f64 time = -1.0f;
			KeyValues3 *element = data->GetArrayElement(i);
			if (element)
			{
				time = element->GetDouble(-1.0);
			}
			overall ? pb.overall.pbSplitZoneTimes[i] = time : pb.pro.pbSplitZoneTimes[i] = time;
		}
	}

	data = kv.FindMember("cpZoneTimes");
	if (data && data->GetType() == KV3_TYPE_ARRAY)
	{
		for (i32 i = 0; i < course->checkpointCount; i++)
		{
			f64 time = -1.0f;
			KeyValues3 *element = data->GetArrayElement(i);
			if (element)
			{
				time = element->GetDouble(-1.0);
			}
			overall ? pb.overall.pbCpZoneTimes[i] = time : pb.pro.pbCpZoneTimes[i] = time;
		}
	}

	data = kv.FindMember("stageZoneTimes");
	if (data && data->GetType() == KV3_TYPE_ARRAY)
	{
		for (i32 i = 0; i < course->stageCount; i++)
		{
			f64 time = -1.0f;
			KeyValues3 *element = data->GetArrayElement(i);
			if (element)
			{
				time = element->GetDouble(-1.0);
			}
			overall ? pb.overall.pbStageZoneTimes[i] = time : pb.pro.pbStageZoneTimes[i] = time;
		}
	}
}

void KZTimerService::ClearPBCache()
{
	this->localPBCache.clear();
}

const PBData *KZTimerService::GetGlobalCachedPB(const KZCourseDescriptor *course, PluginId modeID)
{
	PBDataKey key = ToPBDataKey(modeID, course->guid);

	if (this->globalPBCache.find(key) == this->globalPBCache.end())
	{
		return nullptr;
	}

	return &this->globalPBCache[key];
}

void KZTimerService::InsertPBToCache(f64 time, const KZCourseDescriptor *course, PluginId modeID, bool overall, bool global, CUtlString metadata,
									 f64 points)
{
	PBData &pb = global ? this->globalPBCache[ToPBDataKey(modeID, course->guid)] : this->localPBCache[ToPBDataKey(modeID, course->guid)];

	overall ? pb.overall.points = points : pb.pro.points = points;
	overall ? pb.overall.pbTime = time : pb.pro.pbTime = time;
	KeyValues3 kv(KV3_TYPEEX_TABLE, KV3_SUBTYPE_UNSPECIFIED);
	CUtlString error = "";
	if (metadata.IsEmpty())
	{
		return;
	}
	LoadKV3FromJSON(&kv, &error, metadata.Get(), "");
	if (!error.IsEmpty())
	{
		META_CONPRINTF("[KZ::Timer] Failed to insert server record to cache due to metadata error: %s\n", error.Get());
		return;
	}

	KeyValues3 *data = kv.FindMember("splitZoneTimes");
	if (data && data->GetType() == KV3_TYPE_ARRAY)
	{
		for (i32 i = 0; i < course->splitCount; i++)
		{
			f64 time = -1.0f;
			KeyValues3 *element = data->GetArrayElement(i);
			if (element)
			{
				time = element->GetDouble(-1.0);
			}
			overall ? pb.overall.pbSplitZoneTimes[i] = time : pb.pro.pbSplitZoneTimes[i] = time;
		}
	}

	data = kv.FindMember("cpZoneTimes");
	if (data && data->GetType() == KV3_TYPE_ARRAY)
	{
		for (i32 i = 0; i < course->checkpointCount; i++)
		{
			f64 time = -1.0f;
			KeyValues3 *element = data->GetArrayElement(i);
			if (element)
			{
				time = element->GetDouble(-1.0);
			}
			overall ? pb.overall.pbCpZoneTimes[i] = time : pb.pro.pbCpZoneTimes[i] = time;
		}
	}

	data = kv.FindMember("stageZoneTimes");
	if (data && data->GetType() == KV3_TYPE_ARRAY)
	{
		for (i32 i = 0; i < course->stageCount; i++)
		{
			f64 time = -1.0f;
			KeyValues3 *element = data->GetArrayElement(i);
			if (element)
			{
				time = element->GetDouble(-1.0);
			}
			overall ? pb.overall.pbStageZoneTimes[i] = time : pb.pro.pbStageZoneTimes[i] = time;
		}
	}
}

void KZTimerService::CheckMissedTime()
{
	const KZCourseDescriptor *course = this->GetCourse();
	// No active course, the timer is not running or if we already announce late PBs.
	if (!course || !this->GetTimerRunning() || (!this->shouldAnnounceMissedTime && !this->shouldAnnounceMissedProTime))
	{
		return;
	}
	// No comparison available for styled runs.
	if (this->player->styleServices.Count() > 0)
	{
		return;
	}
	if (this->player->checkpointService->GetCheckpointCount() > 0)
	{
		this->shouldAnnounceMissedProTime = false;
	}
	auto modeInfo = KZ::mode::GetModeInfo(this->player->modeService->GetModeName());

	PBDataKey key = ToPBDataKey(modeInfo.id, course->guid);

	// Check if there is personal best data for this mode and course.
	auto pb = this->GetCompareTarget(key);
	if (!pb)
	{
		return;
	}
	if (this->shouldAnnounceMissedProTime && pb->pro.pbTime > 0 && this->GetTime() > pb->pro.pbTime)
	{
		// Check if they share the same time.
		if (this->shouldAnnounceMissedTime && pb->overall.pbTime == pb->pro.pbTime)
		{
			CUtlString timeText = KZTimerService::FormatTime(pb->overall.pbTime);
			this->player->languageService->PrintChat(true, false, missedTimeKeysBoth[this->currentCompareType], timeText.Get());
			this->shouldAnnounceMissedTime = false;
		}
		else
		{
			CUtlString timeText = KZTimerService::FormatTime(pb->pro.pbTime);
			this->player->languageService->PrintChat(true, false, missedTimeKeysPro[this->currentCompareType], timeText.Get());
		}
		this->shouldAnnounceMissedProTime = false;
		this->PlayMissedTimeSound();
	}

	if (this->shouldAnnounceMissedTime && pb->overall.pbTime > 0 && this->GetTime() > pb->overall.pbTime)
	{
		CUtlString timeText = KZTimerService::FormatTime(pb->overall.pbTime);
		this->player->languageService->PrintChat(true, false, missedTimeKeys[this->currentCompareType], timeText.Get());
		this->shouldAnnounceMissedTime = false;
		this->PlayMissedTimeSound();
	}
}

void KZTimerService::ShowSplitText(u32 currentSplit)
{
	const KZCourseDescriptor *course = this->GetCourse();
	// No active course so we can't compare anything.
	if (!course)
	{
		return;
	}
	// No comparison available for styled runs.
	if (this->player->styleServices.Count() > 0)
	{
		return;
	}

	CUtlString time;
	std::string pbDiff, pbDiffPro = "";

	time = KZTimerService::FormatTime(this->splitZoneTimes[currentSplit - 1]);
	if (this->lastSplit != 0)
	{
		f64 diff = this->splitZoneTimes[currentSplit - 1] - this->splitZoneTimes[this->lastSplit - 1];
		CUtlString splitTime = KZTimerService::FormatDiffTime(diff);
		splitTime.Format(" {grey}({default}%s{grey})", splitTime.Get());
		time.Append(splitTime.Get());
	}

	auto modeInfo = KZ::mode::GetModeInfo(this->player->modeService->GetModeName());
	PBDataKey key = ToPBDataKey(modeInfo.id, course->guid);

	// Check if there is personal best data for this mode and course.
	const PBData *pb = this->GetCompareTarget(key);
	if (pb)
	{
		if (pb->overall.pbSplitZoneTimes[currentSplit - 1] > 0)
		{
			META_CONPRINTF("pb->overall.pbSplitZoneTimes[currentSplit - 1] = %lf\n", pb->overall.pbSplitZoneTimes[currentSplit - 1]);
			f64 diff = this->splitZoneTimes[currentSplit - 1] - pb->overall.pbSplitZoneTimes[currentSplit - 1];
			CUtlString diffText = KZTimerService::FormatDiffTime(diff);
			diffText.Format("{grey}%s%s{grey}", diff < 0 ? "{green}" : "{lightred}", diffText.Get());
			pbDiff = this->player->languageService->PrepareMessage(diffTextKeys[this->currentCompareType], diffText.Get());
		}
		if (this->player->checkpointService->GetTeleportCount() == 0 && pb->pro.pbTime > 0 && pb->pro.pbSplitZoneTimes[currentSplit - 1] > 0)
		{
			f64 diff = this->splitZoneTimes[currentSplit - 1] - pb->pro.pbSplitZoneTimes[currentSplit - 1];
			CUtlString diffText = KZTimerService::FormatDiffTime(diff);
			diffText.Format("{grey}%s%s{grey}", diff < 0 ? "{green}" : "{lightred}", diffText.Get());
			pbDiffPro = this->player->languageService->PrepareMessage(diffTextKeysPro[this->currentCompareType], diffText.Get());
		}
	}

	this->player->languageService->PrintChat(true, false, "Course Split Reached", currentSplit, time.Get(), pbDiff.c_str(), pbDiffPro.c_str());
}

void KZTimerService::ShowCheckpointText(u32 currentCheckpoint)
{
	const KZCourseDescriptor *course = this->GetCourse();
	// No active course so we can't compare anything.
	if (!course)
	{
		return;
	}
	// No comparison available for styled runs.
	if (this->player->styleServices.Count() > 0)
	{
		return;
	}

	CUtlString time;
	std::string pbDiff, pbDiffPro = "";

	time = KZTimerService::FormatTime(this->cpZoneTimes[currentCheckpoint - 1]);
	if (this->lastCheckpoint != 0)
	{
		f64 diff = this->cpZoneTimes[currentCheckpoint - 1] - this->cpZoneTimes[this->lastCheckpoint - 1];
		CUtlString splitTime = KZTimerService::FormatDiffTime(diff);
		splitTime.Format(" {grey}({default}%s{grey})", splitTime.Get());
		time.Append(splitTime.Get());
	}

	auto modeInfo = KZ::mode::GetModeInfo(this->player->modeService->GetModeName());
	PBDataKey key = ToPBDataKey(modeInfo.id, course->guid);

	// Check if there is personal best data for this mode and course.
	const PBData *pb = this->GetCompareTarget(key);
	if (pb)
	{
		if (pb->overall.pbCpZoneTimes[currentCheckpoint - 1] > 0)
		{
			f64 diff = this->cpZoneTimes[currentCheckpoint - 1] - pb->overall.pbCpZoneTimes[currentCheckpoint - 1];
			CUtlString diffText = KZTimerService::FormatDiffTime(diff);
			diffText.Format("{grey}%s%s{grey}", diff < 0 ? "{green}" : "{lightred}", diffText.Get());
			pbDiff = this->player->languageService->PrepareMessage(diffTextKeys[this->currentCompareType], diffText.Get());
		}
		if (this->player->checkpointService->GetTeleportCount() == 0 && pb->pro.pbTime > 0 && pb->pro.pbCpZoneTimes[currentCheckpoint - 1] > 0)
		{
			f64 diff = this->cpZoneTimes[currentCheckpoint - 1] - pb->pro.pbCpZoneTimes[currentCheckpoint - 1];
			CUtlString diffText = KZTimerService::FormatDiffTime(diff);
			diffText.Format("{grey}%s%s{grey}", diff < 0 ? "{green}" : "{lightred}", diffText.Get());
			pbDiffPro = this->player->languageService->PrepareMessage(diffTextKeysPro[this->currentCompareType], diffText.Get());
		}
	}

	this->player->languageService->PrintChat(true, false, "Course Checkpoint Reached", currentCheckpoint, time.Get(), pbDiff.c_str(),
											 pbDiffPro.c_str());
}

void KZTimerService::ShowStageText()
{
	const KZCourseDescriptor *course = this->GetCourse();
	// No active course so we can't compare anything.
	if (!course)
	{
		return;
	}
	// No comparison available for styled runs.
	if (this->player->styleServices.Count() > 0)
	{
		return;
	}

	CUtlString time;
	std::string pbDiff, pbDiffPro = "";

	time = KZTimerService::FormatTime(this->stageZoneTimes[this->currentStage]);
	if (this->currentStage > 0)
	{
		f64 diff = this->stageZoneTimes[this->currentStage] - this->stageZoneTimes[this->currentStage - 1];
		CUtlString splitTime = KZTimerService::FormatDiffTime(diff);
		splitTime.Format(" {grey}({default}%s{grey})", splitTime.Get());
		time.Append(splitTime.Get());
	}

	auto modeInfo = KZ::mode::GetModeInfo(this->player->modeService->GetModeName());
	PBDataKey key = ToPBDataKey(modeInfo.id, course->guid);

	// Check if there is personal best data for this mode and course.
	const PBData *pb = this->GetCompareTarget(key);
	if (pb)
	{
		if (pb->overall.pbStageZoneTimes[this->currentStage] > 0)
		{
			f64 diff = this->stageZoneTimes[this->currentStage] - pb->overall.pbStageZoneTimes[this->currentStage];
			CUtlString diffText = KZTimerService::FormatDiffTime(diff);
			diffText.Format("{grey}%s%s{grey}", diff < 0 ? "{green}" : "{lightred}", diffText.Get());
			pbDiff = this->player->languageService->PrepareMessage(diffTextKeys[this->currentCompareType], diffText.Get());
		}
		if (this->player->checkpointService->GetTeleportCount() == 0 && pb->pro.pbTime > 0 && pb->pro.pbStageZoneTimes[this->currentStage] > 0)
		{
			f64 diff = this->stageZoneTimes[this->currentStage] - pb->pro.pbStageZoneTimes[this->currentStage];
			CUtlString diffText = KZTimerService::FormatDiffTime(diff);
			diffText.Format("{grey}%s%s{grey}", diff < 0 ? "{green}" : "{lightred}", diffText.Get());
			pbDiffPro = this->player->languageService->PrepareMessage(diffTextKeysPro[this->currentCompareType], diffText.Get());
		}
	}

	this->player->languageService->PrintChat(true, false, "Course Stage Reached", this->currentStage + 1, time.Get(), pbDiff.c_str(),
											 pbDiffPro.c_str());
}

CUtlString KZTimerService::GetCurrentRunMetadata()
{
	KeyValues3 kv(KV3_TYPEEX_TABLE, KV3_SUBTYPE_UNSPECIFIED);

	KeyValues3 *splitZoneTimesKV = kv.FindOrCreateMember("splitZoneTimes");

	splitZoneTimesKV->SetToEmptyArray();
	FOR_EACH_VEC(this->splitZoneTimes, i)
	{
		KeyValues3 *time = splitZoneTimesKV->ArrayAddElementToTail();
		time->SetDouble(this->splitZoneTimes[i]);
	}

	KeyValues3 *cpZoneTimesKV = kv.FindOrCreateMember("cpZoneTimes");
	cpZoneTimesKV->SetToEmptyArray();
	FOR_EACH_VEC(this->cpZoneTimes, i)
	{
		KeyValues3 *time = cpZoneTimesKV->ArrayAddElementToTail();
		time->SetDouble(this->cpZoneTimes[i]);
	}

	splitZoneTimesKV->SetToEmptyArray();

	KeyValues3 *stageZoneTimesKV = kv.FindOrCreateMember("stageZoneTimes");
	FOR_EACH_VEC(this->stageZoneTimes, i)
	{
		KeyValues3 *time = stageZoneTimesKV->ArrayAddElementToTail();
		time->SetDouble(this->stageZoneTimes[i]);
	}

	CUtlString result, error;
	if (SaveKV3AsJSON(&kv, &error, &result))
	{
		return result;
	}
	META_CONPRINTF("[KZ::Timer] Failed to obtain current run's metadata! (%s)\n", error.Get());
	return "";
}

void KZTimerService::UpdateLocalPBCache()
{
	CPlayerUserId uid = player->GetClient()->GetUserID();

	auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
	{
		KZPlayer *pl = g_pKZPlayerManager->ToPlayer(uid);
		if (!pl)
		{
			return;
		}
		ISQLResult *result = queries[0]->GetResultSet();
		if (result && result->GetRowCount() > 0)
		{
			while (result->FetchRow())
			{
				auto modeInfo = KZ::mode::GetModeInfoFromDatabaseID(result->GetInt(2));
				if (modeInfo.databaseID < 0)
				{
					continue;
				}
				const KZCourseDescriptor *course = KZ::course::GetCourseByLocalCourseID(result->GetInt(1));
				if (!course)
				{
					continue;
				}
				pl->timerService->InsertPBToCache(result->GetFloat(0), course, modeInfo.id, true, false, result->GetString(3));
			}
		}
		result = queries[1]->GetResultSet();
		if (result && result->GetRowCount() > 0)
		{
			while (result->FetchRow())
			{
				auto modeInfo = KZ::mode::GetModeInfoFromDatabaseID(result->GetInt(2));
				if (modeInfo.databaseID < 0)
				{
					continue;
				}
				const KZCourseDescriptor *course = KZ::course::GetCourseByLocalCourseID(result->GetInt(1));
				if (!course)
				{
					continue;
				}
				pl->timerService->InsertPBToCache(result->GetFloat(0), course, modeInfo.id, false, false, result->GetString(3));
			}
		}
	};
	KZDatabaseService::QueryAllPBs(player->GetSteamId64(), g_pKZUtils->GetCurrentMapName(), onQuerySuccess, KZDatabaseService::OnGenericTxnFailure);
}

void KZTimerService::Init()
{
	KZDatabaseService::RegisterEventListener(&databaseEventListener);
	KZOptionService::RegisterEventListener(&optionEventListener);
}

void KZTimerService::OnPlayerPreferencesLoaded()
{
	if (this->player->optionService->GetPreferenceInt("preferredCompareType", COMPARE_GPB) > COMPARETYPE_COUNT)
	{
		this->preferredCompareType = COMPARE_GPB;
		return;
	}
	this->preferredCompareType = (CompareType)this->player->optionService->GetPreferenceInt("preferredCompareType", COMPARE_GPB);
	this->shouldPlayTimerStopSound = this->player->optionService->GetPreferenceBool("timerStopSound", true);
}

void KZDatabaseServiceEventListener_Timer::OnMapSetup()
{
	KZ::course::SetupLocalCourses();
	KZTimerService::UpdateLocalRecordCache();
}

void KZDatabaseServiceEventListener_Timer::OnClientSetup(Player *player, u64 steamID64, bool isCheater)
{
	KZPlayer *kzPlayer = g_pKZPlayerManager->ToKZPlayer(player);
	kzPlayer->timerService->UpdateLocalPBCache();
}
