#pragma once
#include "../kz.h"
#include "kz/recording/kz_recording.h"

// Note that the API might not agree with these durations, and the API will always take precedence.
// big ban for players using hacked clients
#define BAN_DURATION_INVALID_CVARS      0.0f // permanent ban
#define BAN_DURATION_BHOP_HACK          0.0f // permanent ban
#define BAN_DURATION_EXTERNAL_DESUBTICK 0.0f // permanent ban
// medium ban for minor infractions
#define BAN_DURATION_HYPERSCROLL  60.0f // 60 days
#define BAN_DURATION_SUBTICK_SPAM 60.0f // 60 days
#define BAN_DURATION_NULLS        60.0f // 60 days

class KZBaseService;

class KZAnticheatService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

private:
	std::string cheatReason = "";
	std::unique_ptr<CheaterRecorder> cheaterRecorder = nullptr;

public:
	static void Init();
	static void CleanupSvCheatsWatcher();
	bool isBanned = false;

	// Cvar checker stuff
	bool hasValidCvars = true;

	bool ShouldCheckClientCvars()
	{
		return hasValidCvars;
	}

	static void InitSvCheatsWatcher();

	void MarkHasInvalidCvars()
	{
		hasValidCvars = false;
	}

	void InitCvarMonitor();

	static f64 KickPlayerInvalidSettings(CPlayerUserId userID);

	// Hyperscroll stuff
	void UpdateScrollStatistics(PlayerCommand *cmd);

	// Subtick abuse stuff

	std::deque<f32> suspiciousSubtickMoveTimes;
	std::deque<f32> invalidCommandTimes;
	// Clear up old entries beyond a certain duration and ban if there are too many within that timeframe.
	void CheckSuspiciousSubtickCommands();
	void OnSetupMove(PlayerCommand *cmd);

	void OnPlayerFullyConnect();
	void OnAuthorized();
	void OnGlobalAuthFinished(bool isBanned, const UUID_t *banId, std::string reason);
	void OnClientSetup(bool isBanned);
	void MarkDetection(std::string reason, bool generateReplay, f32 banDuration = 0.0f, bool kick = false);

public:
	f32 currentMaxFps = 0.0f;
};
