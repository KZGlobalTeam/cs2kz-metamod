#pragma once
#include "../kz.h"
#include "../checkpoint/kz_checkpoint.h"
#include "kz/course/kz_course.h"
#include "kz/mappingapi/kz_mappingapi.h"

#define KZ_MAX_MODE_NAME_LENGTH 128

#define KZ_TIMER_MIN_GROUND_TIME 0.05f

#define KZ_TIMER_SOUND_COOLDOWN       0.15f
#define KZ_TIMER_SND_START            "Buttons.snd9"
#define KZ_TIMER_SND_END              "tr.ScoreRegular"
#define KZ_TIMER_SND_FALSE_END        "UIPanorama.buymenu_failure"
#define KZ_TIMER_SND_MISSED_ZONE      "UIPanorama.buymenu_failure"
#define KZ_TIMER_SND_REACH_SPLIT      "tr.Popup"
#define KZ_TIMER_SND_REACH_CHECKPOINT "tr.Popup"
#define KZ_TIMER_SND_REACH_STAGE      "UIPanorama.round_report_odds_up"
#define KZ_TIMER_SND_STOP             "tr.PuckFail"
#define KZ_TIMER_SND_MISSED_TIME      "UI.RankDown"

#define KZ_PAUSE_COOLDOWN 1.0f

class KZTimerServiceEventListener
{
public:
	virtual bool OnTimerStart(KZPlayer *player, u32 courseGUID)
	{
		return true;
	}

	virtual void OnTimerStartPost(KZPlayer *player, u32 courseGUID) {}

	virtual bool OnTimerEnd(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed)
	{
		return true;
	}

	virtual void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed) {}

	virtual void OnTimerStopped(KZPlayer *player, u32 courseGUID) {}

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
	u32 currentCourseGUID {};
	f64 lastEndTime {};
	f64 lastFalseEndTime {};
	f64 lastStartSoundTime {};
	f64 lastMissedTimeSoundTime {};
	bool validTime {};

	u32 lastSplit {};
	CUtlVectorFixed<f64, KZ_MAX_SPLIT_ZONES> splitZoneTimes {};

	u32 lastCheckpoint {};
	i32 reachedCheckpoints {};
	CUtlVectorFixed<f64, KZ_MAX_CHECKPOINT_ZONES> cpZoneTimes {};

	i32 currentStage {};
	CUtlVectorFixed<f64, KZ_MAX_STAGE_ZONES> stageZoneTimes {};

	// PB cache per mode and per course.
	std::unordered_map<PBDataKey, PBData> localPBCache;
	std::unordered_map<PBDataKey, PBData> globalPBCache;

	// SR cache should be loaded upon map start, every time !wr is queried and every time a run beats the server record.
	static std::unordered_map<PBDataKey, PBData> srCache;

	static std::unordered_map<PBDataKey, PBData> wrCache;

public:
	enum CompareType : u8
	{
		COMPARE_NONE = 0,
		COMPARE_SPB, // Local PB
		COMPARE_GPB, // Global PB
		COMPARE_SR,  // Server Record
		COMPARE_WR,  // Global Record
		COMPARETYPE_COUNT
	};

private:
	// The maximum level that we should compare our current time with.
	// For example, if the value is set to COMPARE_GPB, the player will not attempt to compare their splits with SR/WR,
	// but only global PB, and local PB if global data is not available.
	CompareType preferredCompareType = COMPARE_GPB;

	// What we are currently comparing our run against in this current run.
	// This stays the same from the start of the run (unless preferredCompareType changes) to have a consistent comparison across the run.
	CompareType currentCompareType = COMPARE_GPB;

	void UpdateCurrentCompareType(PBDataKey key);
	const PBData *GetCompareTargetForType(CompareType type, PBDataKey key);
	const PBData *GetCompareTarget(PBDataKey key);

	bool shouldAnnounceMissedTime = true;
	bool shouldAnnounceMissedProTime = true;

public:
	static void ClearRecordCache();
	static void UpdateLocalRecordCache();
	static void InsertRecordToCache(f64 time, const KZCourse *courseName, PluginId modeID, bool hasTeleports, bool global, CUtlString metadata = "");

	void ClearPBCache();
	void UpdateLocalPBCache();
	void InsertPBToCache(f64 time, const KZCourse *courseName, PluginId modeID, bool hasTeleports, bool global, CUtlString metadata = "");
	void SetCompareTarget(const char *typeString);

	void CheckMissedTime();

	void ShowSplitText(u32 currentSplit);
	void ShowCheckpointText(u32 currentCheckpoint);
	void ShowStageText();

	CUtlString GetCurrentRunMetadata();

private:
	bool validJump {};
	f64 lastInvalidateTime {};

public:
	static void Init();
	static void RegisterCommands();
	static void RegisterPBCommand();
	static void RegisterRecordCommands();
	static void RegisterCourseTopCommands();
	static bool RegisterEventListener(KZTimerServiceEventListener *eventListener);
	static bool UnregisterEventListener(KZTimerServiceEventListener *eventListener);

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

	static void FormatTime(f64 time, char *output, u32 length, bool precise = true);

	static CUtlString FormatTime(f64 time, bool precise = true)
	{
		char temp[32];
		FormatTime(time, temp, sizeof(temp), precise);
		return CUtlString(temp);
	}

	static void FormatDiffTime(f64 time, char *output, u32 length, bool precise = true)
	{
		char temp[32];
		if (time > 0)
		{
			FormatTime(time, temp, sizeof(temp));
			V_snprintf(output, length, "+%s", temp);
		}
		else
		{
			FormatTime(-time, temp, sizeof(temp));
			V_snprintf(output, length, "-%s", temp);
		}
	}

	static CUtlString FormatDiffTime(f64 time, bool precise = true)
	{
		char temp[32];
		FormatDiffTime(time, temp, sizeof(temp), precise);
		return CUtlString(temp);
	}

	void SetTime(f64 time)
	{
		currentTime = time;
		timerRunning = time > 0.0f;
	}

	const KZCourse *GetCourse()
	{
		return KZ::course::GetCourse(currentCourseGUID);
	}

	const KZCourseDescriptor *GetCourseDescriptor()
	{
		return GetCourse() ? GetCourse()->descriptor : nullptr;
	}

	void SetCourse(u32 courseGUID)
	{
		currentCourseGUID = courseGUID;
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

	void StartZoneStartTouch(const KZCourseDescriptor *course);
	void StartZoneEndTouch(const KZCourseDescriptor *course);
	void SplitZoneStartTouch(const KZCourseDescriptor *course, i32 splitNumber);
	void CheckpointZoneStartTouch(const KZCourseDescriptor *course, i32 cpNumber);
	void StageZoneStartTouch(const KZCourseDescriptor *course, i32 stageNumber);
	bool TimerStart(const KZCourseDescriptor *course, bool playSound = true);
	bool TimerEnd(const KZCourseDescriptor *course);
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

	static bool IsValidMoveType(MoveType_t moveType)
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
	void PlayMissedZoneSound();
	void PlayReachedSplitSound();
	void PlayReachedCheckpointSound();
	void PlayReachedStageSound();
	void PlayTimerStopSound();
	void PlayMissedTimeSound();

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
	static void OnRoundStart();
	void OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity);

	void OnPlayerPreferencesLoaded();
};

namespace KZ
{
	namespace timer
	{
		// Announcements
		struct LocalRankData
		{
			bool firstTime {};
			f32 pbDiff {};
			u32 rank {};
			u32 maxRank {};

			bool firstTimePro {};
			f32 pbDiffPro {};
			u32 rankPro {};
			u32 maxRankPro {};
		};

		struct GlobalRankData
		{
			u32 mapPointsGained {};
			u32 totalMapPoints {};
			u32 playerRating {};

			bool firstTime {};
			f32 pbDiff {};
			u32 rank {};
			u32 maxRank {};

			bool firstTimePro {};
			f32 pbDiffPro {};
			u32 rankPro {};
			u32 maxRankPro {};
		};

		void AddRunToAnnounceQueue(KZPlayer *player, CUtlString courseName, f64 time, u64 teleportsUsed, const char *metadata);
		void ClearAnnounceQueue();
		void CheckAnnounceQueue();
		void UpdateLocalRankData(u32 id, LocalRankData data);
		void UpdateGlobalRankData(u32 id, GlobalRankData data);

		// PB Requests
		void CheckPBRequests();

		// Record requests
		void CheckRecordRequests();

		// CourseTop requests
		void CheckCourseTopRequests();
	} // namespace timer
} // namespace KZ
