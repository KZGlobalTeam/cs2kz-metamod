#pragma once
#include "../kz.h"
#include "../checkpoint/kz_checkpoint.h"

#define KZ_MAX_COURSE_NAME_LENGTH 128
#define KZ_MAX_MODE_NAME_LENGTH   128

#define KZ_TIMER_MIN_GROUND_TIME 0.05f
#define KZ_TIMER_SOUND_COOLDOWN  0.15f
#define KZ_TIMER_SND_START       "Buttons.snd9"
#define KZ_TIMER_SND_END         "UI.DeathMatch.LevelUp"
#define KZ_TIMER_SND_FALSE_END   "UIPanorama.buymenu_failure"
#define KZ_TIMER_SND_STOP        "tr.PuckFail"

#define KZ_PAUSE_COOLDOWN 1.0f

class KZTimerServiceEventListener
{
public:
	virtual bool OnTimerStart(KZPlayer *player, const char *courseName)
	{
		return true;
	}

	virtual void OnTimerStartPost(KZPlayer *player, const char *courseName) {}

	virtual bool OnTimerEnd(KZPlayer *player, const char *courseName, f32 time, u32 teleportsUsed)
	{
		return true;
	}

	virtual bool OnTimerEndMessage(KZPlayer *player, const char *courseName, f32 time, u32 teleportsUsed)
	{
		return true;
	}

	virtual void OnTimerEndPost(KZPlayer *player, const char *courseName, f32 time, u32 teleportsUsed) {}

	virtual void OnTimerStopped(KZPlayer *player) {}

	virtual void OnTimerInvalidated(KZPlayer *player) {}

	virtual bool OnPause(KZPlayer *player)
	{
		return true;
	}

	virtual void OnPausePost(KZPlayer *player) {}

	virtual bool OnResume(KZPlayer *player)
	{
		return true;
	}

	virtual void OnResumePost(KZPlayer *player) {}
};

class KZTimerService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool timerRunning {};
	f64 currentTime {};
	char currentCourse[KZ_MAX_COURSE_NAME_LENGTH] {};
	f64 lastEndTime {};
	f64 lastFalseEndTime {};
	f64 lastStartSoundTime {};
	char lastStartMode[128] {};
	bool validTime {};

	bool validJump {};
	f64 lastInvalidateTime {};

public:
	static_global void RegisterCommands();
	static_global bool RegisterEventListener(KZTimerServiceEventListener *eventListener);
	static_global bool UnregisterEventListener(KZTimerServiceEventListener *eventListener);

	bool GetTimerRunning()
	{
		return timerRunning;
	}

	bool GetValidTimer()
	{
		return validTime;
	}

	f64 GetTime()
	{
		return currentTime;
	}

	static_global void FormatTime(f64 time, char *output, u32 length, bool precise = true);

	void SetTime(f64 time)
	{
		currentTime = time;
		timerRunning = time > 0.0f;
	}

	void GetCourse(char *buffer, u32 size)
	{
		V_snprintf(buffer, size, "%s", currentCourse);
	}

	void SetCourse(const char *name)
	{
		V_strncpy(currentCourse, name, KZ_MAX_COURSE_NAME_LENGTH);
	}

	enum TimeType_t
	{
		TimeType_Standard,
		TimeType_Pro
	};

	TimeType_t GetCurrentTimeType()
	{
		return this->player->checkpointService->GetTeleportCount() > 0 ? TimeType_Standard : TimeType_Pro;
	}

	void StartZoneStartTouch();
	void StartZoneEndTouch();
	bool TimerStart(const char *courseName, bool playSound = true);
	bool TimerEnd(const char *courseName);
	bool TimerStop(bool playSound = true);
	static void TimerStopAll(bool playSound = true);

	bool GetValidJump()
	{
		return validJump;
	}

	void InvalidateJump();
	void PlayTimerStartSound();

	// To be used for saveloc.
	void InvalidateRun();

private:
	bool HasValidMoveType();

	static_global bool IsValidMoveType(MoveType_t moveType)
	{
		return moveType == MOVETYPE_WALK || moveType == MOVETYPE_LADDER || moveType == MOVETYPE_NONE || moveType == MOVETYPE_OBSERVER;
	}

	bool JustLanded()
	{
		return g_pKZUtils->GetGlobals()->curtime - this->player->landingTime < KZ_TIMER_MIN_GROUND_TIME;
	}

	bool JustStartedTimer()
	{
		return timerRunning && this->GetTime() < EPSILON;
	}

	bool JustEndedTimer();

	void PlayTimerEndSound();
	void PlayTimerFalseEndSound();
	void PlayTimerStopSound();

	void PrintEndTimeString();

	/*
	 * Pause stuff also goes here.
	 */

private:
	bool paused {};
	bool pausedOnLadder {};
	f32 lastPauseTime {};
	bool hasPausedInThisRun {};
	f32 lastResumeTime {};
	bool hasResumedInThisRun {};
	f32 lastDuckValue {};
	f32 lastStaminaValue {};
	bool touchedGroundSinceTouchingStartZone {};

public:
	bool GetPaused()
	{
		return paused;
	}

	void SetPausedOnLadder(bool ladder)
	{
		pausedOnLadder = ladder;
	}

	void Pause();
	bool CanPause(bool showError = false);
	void Resume(bool force = false);
	bool CanResume(bool showError = false);

	void TogglePause()
	{
		paused ? Resume() : Pause();
	}

	// Event listeners

public:
	virtual void Reset() override;
	void OnPhysicsSimulatePost();
	void OnStartTouchGround();
	void OnStopTouchGround();
	void OnChangeMoveType(MoveType_t oldMoveType);
	void OnTeleportToStart();
	void OnClientDisconnect();
	void OnPlayerSpawn();
	void OnPlayerJoinTeam(i32 team);
	void OnPlayerDeath();
	void OnOptionsChanged();
	static_global void OnRoundStart();
	void OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity);
};
