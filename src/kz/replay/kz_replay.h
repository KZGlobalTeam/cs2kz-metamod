#pragma once
#include "../kz.h"
#include <optional>
#include <variant>
#include <mutex>
#include <deque>

#define RP_MAGIC_NUMBER 0x4B5A5250

class Jump;

// There are way more things tracked than necessary, but for future anticheat purposes,
// it's better to have too much than too little.
// Also, we would like to use as much of the data as possible for accurate sound prediction.

// ====================================
// Replay events
// ====================================
struct WeaponSwitchEventData
{
	std::string name {};
	u32 slot {};

	// Playback doesn't use this, but video editors want this.
	struct
	{
		u16 defIndex;
		i32 entityQuality;
		u32 entityLevel;
		u64 itemID;
		u32 itemIDHigh;
		u32 itemIDLow;

		// See AttributeList
		struct Attribute
		{
			u16 defIndex;
			f32 value;
		};

		std::vector<Attribute> attributes;
	} econAttributes;
};

// Track all teleport events, not just the ones with the checkpoint menu.
struct TeleportEventData
{
	enum
	{
		UNKNOWN,
		TRIGGER,
		PLUGIN,
	} source {};

	// Note: This uses std::optional just to not forget later that any of these fields can be null.
	std::optional<Vector> origin;
	std::optional<QAngle> angles;
	std::optional<Vector> velocity;
};

// Use to override takeoff data in replays (used by the HUD for example).
struct TakeoffEventData
{
	f32 takeoffSpeed;

	enum
	{
		NORMAL,
		PERF,
		JUMPBUG
	} type;
};

// Due to things like breakables and moving blocks, we can't rely on tick data to replicate jumpstats.
struct JumpstatEventData
{
	Vector takeoffOrigin;
	Vector adjustedTakeoffOrigin;
	Vector takeoffVelocity;
	Vector landingOrigin;
	Vector adjustedLandingOrigin;
	u8 type {};
	f32 release {};
	f32 totalDistance {};
	u32 block {};
	f32 edge {};
	f32 miss {};
	f32 offset {};
	f32 duckDuration {};
	f32 duckEndDuration {};

	struct Strafe
	{
		struct AACall
		{
			f32 externalSpeedDiff {};
			f32 prevYaw {};
			f32 currentYaw {};
			Vector wishdir;
			f32 maxspeed {};
			f32 wishspeed {};
			f32 accel {};

			f32 surfaceFriction {};
			f32 duration {};
			u64 buttons[3] {};
			Vector velocityPre;
			Vector velocityPost;
			bool ducking {};
		};

		std::vector<AACall> aaCalls;
	};

	std::vector<JumpstatEventData::Strafe> strafes;

	void FromJump(const Jump *jump);
};

// Also tracks yaw and pitch.
struct SensitivityChangeEventData
{
	enum
	{
		SENSITIVITY,
		YAW,
		PITCH
	} type;

	f32 value {};
};

struct ReplayEvent
{
	static inline u64 nextUID = 0;

	void Init()
	{
		this->guid = nextUID++;
	}

	u64 guid;
	u32 serverTick;

	enum Type : u8
	{
		WeaponSwitch,
		Teleport,
		Takeoff,
		Jumpstat,
		SenstivityChange
	};

	using ReplayEventData = std::variant<WeaponSwitchEventData, TeleportEventData, TakeoffEventData, JumpstatEventData, SensitivityChangeEventData>;
	ReplayEventData data;
};

// ====================================
// Replay headers
// ====================================
struct RunReplayHeader
{
	std::string courseName {};
	u32 timerStartTick {};
	u32 timerEndTick {};
};

struct CheaterReplayHeader
{
	u32 reason {};
};

struct JumpstatReplayHeader
{
	// Track which event in the file is the relevant one.
	u32 eventIndex {};
};

struct ManualReplayHeader
{
	std::string name {};
};

enum ReplayType : u8
{
	ReplayType_None = 0,
	ReplayType_Run,
	ReplayType_Jumpstats,
	ReplayType_Cheater,
	ReplayType_Manual
};

struct ReplayHeader
{
	ReplayHeader() : magicNumber(RP_MAGIC_NUMBER), uid(RandomInt(0, 0xFFFFFFFFFFFFFFFF)) {}

	const u32 magicNumber;
	const u64 uid;
	u32 version {};

	using ReplayDataHeader = std::variant<std::monostate, RunReplayHeader, CheaterReplayHeader, JumpstatReplayHeader, ManualReplayHeader>;
	ReplayDataHeader typeSpecificHeader;

	i64 timestamp {};

	struct
	{
		char name[128] {};
		u64 steamid64 {};
	} player;

	struct
	{
		std::string name {};
		char md5[33] {};
	} mode;

	struct
	{
		char name[64] {};
		char md5[33] {};
	} map;

	struct StyleInfo
	{
		std::string name {};
		char md5[33] {};
	};

	std::vector<StyleInfo> styles;
};

enum
{
	REPLAYTICK_PRE_FL_ONGROUND = 0x1,
	REPLAYTICK_PRE_IN_CROUCH = 0x2,
	REPLAYTICK_PRE_DUCKED = 0x4,
	REPLAYTICK_PRE_DUCKING = 0x8,
	REPLAYTICK_PRE_OLD_JUMP_PRESSED = 0x10,
	REPLAYTICK_PRE_FL_BASEVELOCITY = 0x20,
	REPLAYTICK_PRE_FL_CONVEYOR_NEW = 0x40,

	REPLAYTICK_POST_FL_ONGROUND = 0x80,
	REPLAYTICK_POST_DUCKED = 0x100,
	REPLAYTICK_POST_DUCKING = 0x200,
	REPLAYTICK_POST_FL_BASEVELOCITY = 0x400,
	REPLAYTICK_POST_FL_CONVEYOR_NEW = 0x800,
};

// ====================================
// Replay tick data
// ====================================

// Sometimes there are multiple ticks of movement in a single server tick.
// Apart from the latest tick, up to 4 other ticks will be run at timescale 0, and if there are more, the buttons will be condensed so the server
// doesn't run more than 4 commands (or whatever the value of sv_late_commands_allowed is).
// We only track the main ticks for playback purposes. Check the tracked user commands for player sent inputs instead.
struct TickData
{
	u64 flags;
	// The should be overridden after CreateMove.
	// Except for post* variables, they should be overridden after player movement.
	// CMoveData stuff
	u64 initialButtons[3];
	f32 forwardmove;
	f32 sidemove;
	f32 upmove;
	Vector preOrigin, postOrigin;
	QAngle preAngles, postAngles;
	Vector preVelocity, postVelocity;
	u32 crouchState;
	GameTime_t crouchTransitionStartTime;
	f32 jumpPressedTime;
	f32 lastDuckTime;
	MoveType_t moveType;
	// These subtick moves do not match the cmdSubtickMoves as it can be modified by modes,
	// and subtickMoves always have one extra when = 1.0 empty move.
	// The theoretical max amount of moves are 1 (minimum) + 12 (player sent) + 4 (forced moves) = 17.
	// We can consider that there will always be a move at 1.0, so we don't need to track that.
	u8 numSubtickMoves;

	inline bool HasSubtickMoves()
	{
		return numSubtickMoves != 0;
	}
};

// Lighter variant of CS2's SubtickMove.
struct SubtickMoveLite
{
	enum ReplayButton : u32
	{
		None = 0x0,
		All = 0xffffffff,
		Attack = 0x1,
		Jump = 0x2,
		Duck = 0x4,
		Forward = 0x8,
		Back = 0x10,
		Use = 0x20,
		TurnLeft = 0x80,
		TurnRight = 0x100,
		MoveLeft = 0x200,
		MoveRight = 0x400,
		Attack2 = 0x800,
		Reload = 0x2000,
		Speed = 0x10000,
		JoyAutoSprint = 0x20000,
		Zoom = 0x40000000,        // 0x400000000
		LookAtWeapon = 0x80000000 // 0x800000000
	};

	f32 when;
	ReplayButton button;

	union
	{
		bool pressed;

		struct
		{
			f32 analog_forward_delta;
			f32 analog_left_delta;
		} analogMove;
	};

	static ReplayButton ConvertButtons(u64 buttons)
	{
		u32 replayButton = static_cast<u32>(buttons);
		// The following use more than 32 bits, so we need to shift them down.
		if (buttons & IN_ZOOM)
		{
			replayButton |= ReplayButton::Zoom;
		}
		if (buttons & IN_LOOK_AT_WEAPON)
		{
			replayButton |= ReplayButton::LookAtWeapon;
		}
		return static_cast<ReplayButton>(replayButton);
	}

	static SubtickMoveLite FromSubtickMove(SubtickMove &move)
	{
		SubtickMoveLite moveLite;
		moveLite.when = move.when;
		moveLite.button = SubtickMoveLite::ConvertButtons(move.button);
		if (move.IsAnalogInput())
		{
			moveLite.analogMove.analog_forward_delta = move.analogMove.analog_forward_delta;
			moveLite.analogMove.analog_left_delta = move.analogMove.analog_left_delta;
		}
		else
		{
			moveLite.pressed = move.pressed;
		}
		return moveLite;
	}
};

struct ReplayData
{
	f32 serverStartTime;
	ReplayHeader header;
	std::vector<TickData> tickData;
	std::vector<SubtickMoveLite> subtickMoves;
	std::vector<ReplayEvent> events;
};

// ====================================
// Replay recorders
// ====================================
struct CircularReplayRecorder;

struct CRRSubtickMoveIterator
{
	bool Next();

	const CircularReplayRecorder *ctx;
	u32 currentSubtickMoveIndex;
	u32 subtickMovesLeft;

	// Result
	SubtickMoveLite *move;
};

struct CRRIterator
{
	bool Next();

	const CircularReplayRecorder *ctx;
	u32 currentTickIndex;
	u32 ticksLeft;
	u32 currentSubtickMoveIndex;
	bool forward;

	// Result
	TickData *tickData;
	CRRSubtickMoveIterator subtickMovesIter;
};

/*
	This contains a fixed amount of tick data (2 minutes) and a variable amount of subtick moves in form of a circular buffer.
	When there are more subtick moves than initially allocated, the buffer will be reallocated to fit the new amount of moves.
*/
struct CircularReplayRecorder
{
	void Init();
	void Deinit();

	void Push(const TickData &tick, const SubtickMoveLite *subtickMoves);

	CRRIterator Iterate() const;
	CRRIterator IterateReverse() const;

	static inline const u32 maxTicks = 64 * 60 * 2; // 2 minutes at 64 ticks per second.
	TickData *tickData;
	u32 nextTickIndex;

	u32 maxSubtickMoves;
	SubtickMoveLite *subtickMoves;
	u32 nextSubtickMoveIndex;
	u32 emptySubtickMoveSlots;

	// TODO: This will probably cause a whole lot of reallocations.
	std::deque<ReplayEvent> events;

	void CheckEvents();

	ReplayEvent *PushEvent(const ReplayEvent &event)
	{
		return &events.emplace_back(event);
	}
};

struct ReplayRecorder
{
	ReplayRecorder()
	{
		this->rpData = new ReplayData();
		this->guid = nextUID++;
	}

	void Cleanup();
	void Push(const TickData &tick, const SubtickMoveLite *subtickMoves);
	u64 PushBreather(const struct CircularReplayRecorder &crr, u64 wishNumTicks);

private:
	static inline u64 nextUID = 0;

public:
	u64 guid;
	ReplayData *rpData = nullptr;
	bool keepData = false;
	u32 idealEndTick {};

	enum
	{
		DISABLED,
		PENDING,
		FINISHED
	} globalState, localState;

	std::string path {};
	bool SaveToFile();
};

// ====================================
// Replay service
// ====================================
class KZReplayService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

	// This gets updated throughout the tick, and should be propagated to all active recordings.
	TickData currentTickData;
	SubtickMoveLite currentSubtickMoves[17];
	CircularReplayRecorder circularRecorder;
	std::vector<ReplayRecorder *> recorders;

	// Finished replays first get saved in memory, then only in the file system if the global or local database is enabled.
	static inline std::mutex finishedRecordersMutex;
	static inline std::vector<ReplayRecorder *> finishedRecorders;

	static inline std::mutex cachedReplayMutex;
	static inline std::vector<ReplayData *> cachedReplays;

	static void Init();
	// TODO: implement this
	static void CacheReplay(ReplayRecorder *recorder);
	ReplayRecorder *CreateRecorder();

	ReplayRecorder *CreateRunRecorder();
	void OnTimerStart();
	void OnTimerStop();

private:
	ReplayEvent *markedJumpstatEvent {};
	ReplayRecorder *CreateJumpstatRecorder(ReplayEvent *jsEvent);

public:
	void OnJumpStart(const Jump *jump);
	void OnJumpEnd(const Jump *jump);

	ReplayRecorder *CreateCheaterRecorder(u32 reason);

	ReplayRecorder *CreateManualRecorder(const std::string &name);

	static void OnGameFramePost();

private:
	bool readyForPlayback {};

public:
	bool canPlayback {};
	// Check for all running recorders and see if they need to be stopped or saved.
	void CheckRecorders();

	void OnCreateMovePost(CUserCmd *cmd, CMoveData *mv);
	void OnPhysicsSimulatePost();

	void OnClientDisconnect();
};
