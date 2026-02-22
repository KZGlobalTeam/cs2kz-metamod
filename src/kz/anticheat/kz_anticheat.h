#pragma once
#include "../kz.h"
#include "kz/recording/kz_recording.h"

class KZBaseService;
class Jump;

class KZAnticheatService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

	struct Infraction
	{
		enum class Type : u8
		{
			Other = 0,
			StrafeHack = 1,   // Strafe hacking
			BhopHack = 2,     // Bhop hack/macro
			Hyperscroll = 3,  // Scrolling too much
			InvalidCvar = 4,  // Invalid client cvar values
			InvalidInput = 5, // Impossible input values
			Nulls = 6,        // Nulls/Snaptap
			SubtickSpam = 7,  // Subtick spam
			Desubtick = 8,    // Desubticking
			COUNT = 9
		};

		static inline ENetworkDisconnectionReason kickReasons[] = {
			NETWORK_DISCONNECT_KICKED_UNTRUSTEDACCOUNT, // Other
			NETWORK_DISCONNECT_KICKED_CONVICTEDACCOUNT, // StrafeHack
			NETWORK_DISCONNECT_KICKED_CONVICTEDACCOUNT, // BhopHack
			NETWORK_DISCONNECT_KICKED_INPUTAUTOMATION,  // Hyperscroll
			NETWORK_DISCONNECT_KICKED_CONVICTEDACCOUNT, // InvalidCvar
			NETWORK_DISCONNECT_KICKED_CONVICTEDACCOUNT, // InvalidInput
			NETWORK_DISCONNECT_KICKED_INPUTAUTOMATION,  // Nulls
			NETWORK_DISCONNECT_KICKED_INPUTAUTOMATION,  // SubtickSpam
			NETWORK_DISCONNECT_KICKED_INPUTAUTOMATION   // Desubtick
		};
		static_assert(KZ_ARRAYSIZE(kickReasons) == static_cast<u8>(Infraction::Type::COUNT));

		// Ban durations in seconds
		// Note that the API might not agree with these durations, and the API will always take precedence.
		// Big ban for players using hacked clients, medium ban for minor infractions
		static inline f32 banDurations[] = {
			-1.0f,                // Other - no ban, just kick
			31556926.0f * 5,      // StrafeHack - 5 years
			31556926.0f * 5,      // BhopHack - 5 years
			60.0f * 24 * 60 * 60, // Hyperscroll - 60 days
			31556926.0f * 5,      // InvalidCvar - 5 years
			31556926.0f * 5,      // InvalidInput - 5 years
			-1.0f,                // Nulls - no ban, just kick (for now)
			-1.0f,                // SubtickSpam - no ban, just kick (for now)
			-1.0f                 // Desubtick - kick only
		};
		static_assert(KZ_ARRAYSIZE(banDurations) == static_cast<u8>(Infraction::Type::COUNT));

		static inline bool shouldGenerateReplay[] = {
			false, // Other
			true,  // StrafeHack
			true,  // BhopHack
			true,  // Hyperscroll
			false, // InvalidCvar
			true,  // InvalidInput
			true,  // Nulls
			true,  // SubtickSpam
			true   // Desubtick
		};
		// clang-format off
		static inline const char *kickInternalReasons[] = {
			"Other",
			"Strafe Hack",
			"Bhop Hack",
			"Hyperscroll",
			"Invalid Cvar",
			"Invalid Input",
			"Nulls",
			"Subtick Spam",
			"Desubticking"
		};
		// clang-format on
		static_assert(KZ_ARRAYSIZE(kickInternalReasons) == static_cast<u8>(Infraction::Type::COUNT));

		bool submitted = false;
		Type type = Type::Other;
		UUID_t id {};
		u64 steamID {};
		std::string details;
		f32 banDuration = -1.0f; // in seconds, -1 = no ban

		// This will be wiped after the replay is saved.
		std::unique_ptr<CheaterRecorder> replay = nullptr;
		// This will persist after the replay is saved.
		UUID_t replayUUID {};

		// Global infraction submission
		void SubmitGlobalInfraction();
		// Callbacks for global infraction submission
		void OnGlobalSubmitSuccess(const UUID_t &infractionId, const UUID_t &replayUUID, f32 banDuration);
		// This will fire if the global infraction submission fails, or if the threshold
		void OnGlobalSubmitFailure();

		void SubmitLocalInfraction();

		void SaveReplay();

		// Finalize the infraction (ban/kick the player if needed)
		void Finalize();
	};

private:
	static inline std::vector<Infraction> pendingBans;
	u32 currentCmdNum {};

public:
	void Reset() override
	{
		isBanned = false;
		printedCheaterMessage = false;
		canPrintCheaterMessage = false;
		hasValidCvars = true;
		recentForwardBackwardEvents.clear();
		recentLeftRightEvents.clear();
		lastButtons = 0;
		suspiciousSubtickMoveTimes.clear();
		invalidCommandTimes.clear();
		zeroWhenCommandTimes.clear();
		numCommandsWithSubtickInputs.clear();
		recentJumpStatuses.clear();
		currentCmdNum = {};
		recentJumps.clear();
		recentLandingEvents.clear();
		lastValidMoveTypeTime = -1.0f;
	}

	static void Init();
	static void CleanupSvCheatsWatcher();
	bool isBanned = false;
	void ClearDetectionBuffers();
	bool ShouldRunDetections() const;
	static f64 PrintWarning(CPlayerUserId userID);

	bool canPrintCheaterMessage = false;
	bool printedCheaterMessage = false;
	void PrintCheaterMessage();

	// ==========[ Cvars ]===========
	bool hasValidCvars = true;

	bool ShouldCheckClientCvars()
	{
		return hasValidCvars && this->ShouldRunDetections();
	}

	static void InitSvCheatsWatcher();

	void MarkHasInvalidCvars()
	{
		hasValidCvars = false;
	}

	void InitCvarMonitor();

	static f64 KickPlayerInvalidSettings(CPlayerUserId userID);

	// ==========[ Nulls ]===========

	struct InputEvent
	{
		i32 cmdNum;
		f32 fraction;
		f32 framerate;
		u64 button;
		bool pressed;
		bool analog = false;
		f32 airSpeed = -1.0f;
	};

	std::deque<InputEvent> recentForwardBackwardEvents;
	std::deque<InputEvent> recentLeftRightEvents;
	u64 lastButtons;

	void CreateInputEvents(PlayerCommand *cmd);
	void CheckNulls();
	void AnalyzeNullsForAxis(const std::deque<InputEvent> &events, u64 button1, u64 button2);
	void CleanupOldInputEvents();

	// ===========[ Hyperscroll ]===========

	std::deque<f32> recentJumps;

	struct LandingEvent
	{
		i32 cmdNum;
		f32 landingTime;
		bool pendingPerf = true;
		bool hasPerfectBhop;
		// This is true if the jump penalty is smaller than the tick interval, in which case we should not be counting this towards perf chains
		// since the player can legitimately hit 100% perfs with desubticks.
		bool shouldCountTowardsPerfChains = false;
		u32 numJumpBefore;
		u32 numJumpAfter;

		std::string ToString() const
		{
			return tfm::format("(%d%s%d)", numJumpBefore, hasPerfectBhop ? "*" : " ", numJumpAfter);
		}
	};

	f32 lastValidMoveTypeTime = -1.0f;
	std::deque<LandingEvent> recentLandingEvents;
	void ParseCommandForJump(PlayerCommand *cmd);

	void CreateLandEvent();
	// Update existing events.
	void CheckLandingEvents();

	void OnChangeMoveType(MoveType_t oldMoveType);

	f32 currentAirTime = 0.0f;
	bool airMovedThisFrame = false;
	void OnProcessMovement();
	void OnAirMove();
	void OnProcessMovementPost();

	// ===========[ Subtick abuses ]===========

	std::deque<f32> suspiciousSubtickMoveTimes;
	std::deque<f32> invalidCommandTimes;
	std::deque<f32> zeroWhenCommandTimes;
	std::deque<f32> numCommandsWithSubtickInputs;
	// Clear up old entries beyond a certain duration and ban if there are too many within that timeframe.
	void CheckSubtickAbuse(PlayerCommand *cmd);
	void CheckSuspiciousSubtickCommands();

	// Generic Events
	void OnJump();

	void OnSetupMove(PlayerCommand *cmd);
	void OnPhysicsSimulatePost();

	struct BanInfo
	{
		UUID_t banId;
		std::string reason;
		std::string expirationDate;
	};

	void OnPlayerFullyConnect();
	// TODO Anticheat: Connect this somewhere
	void OnGlobalAuthFinished(BanInfo *banInfo);
	void OnClientSetup(bool isBanned);

	// ==========[ Strafes ]===========
	enum class JumpStatus : u8
	{
		Normal = 0,
		HighStrafeCount = 1,
		VeryHighStrafeCount = 2,
		HighEfficiency = 3,
		HighSync = 4,
		COUNT = 5
	};
	std::deque<JumpStatus> recentJumpStatuses;
	void OnJumpFinish(Jump *jump);

public:
	f32 currentMaxFps = 0.0f;

	// Infraction stuff
	static Infraction *GetPendingInfraction(const UUID_t &infractionId);

	Infraction *GetPendingInfraction();
	static void CleanupInfractions();

	// Mark a detection, generate a replay if specified, and optionally ban/kick the player.
	void MarkInfraction(Infraction::Type type, const std::string &reason);
};
