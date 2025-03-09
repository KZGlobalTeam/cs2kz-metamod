#pragma once
#include "../kz.h"
#include <optional>

// There are way more things tracked than necessary, but for future anticheat purposes,
// it's better to have too much than too little.
// Also, we would like to use as much of the data as possible for accurate sound prediction.

struct ReplayEvent
{
	u32 tick;

	enum Type : u8
	{
		WeaponSwitch,
		Teleport,
		Takeoff,
		Jumpstat,
		SenstivityChangeEvent
	};
};

struct WeaponSwitchEvent : public ReplayEvent
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

		// See m_AttributeList
		struct Attribute
		{
			u16 defIndex;
			f32 value;
		};

		std::vector<Attribute> attributes;
	} econAttributes;
};

// Track all teleport events, not just the ones with the checkpoint menu.
struct TeleportEvent : public ReplayEvent
{
	enum
	{
		UNKNOWN,
		TRIGGER,
		PLUGIN,
	} source {};

	std::optional<Vector> origin;
	std::optional<QAngle> angles;
	std::optional<Vector> velocity;
};

// Use to override takeoff data in replays.
struct TakeoffEvent : public ReplayEvent
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
struct JumpstatEvent : public ReplayEvent
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
	f32 duckTime {};
	f32 duckTimeEnd {};

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

	std::vector<JumpstatEvent::Strafe> strafes;
};

// Also tracks yaw and pitch.
struct SensitivityChangeEvent : public ReplayEvent
{
	enum
	{
		SENSITIVITY,
		YAW,
		PITCH
	} type;

	f32 value {};
};

enum ReplayType : u8
{
	ReplayType_Run = 0,
	ReplayType_Jumpstats,
	ReplayType_Cheater,
	ReplayType_Manual
};

struct ReplayHeader
{
	u32 magicNumber;
	u32 version;
	ReplayType replayType;
	char version[32];
	u32 timestamp;

	struct
	{
		std::string name {};
		u64 steamid64 {};
	} player;

	struct
	{
		std::string name {};
		std::string md5 {};
	} mode;

	struct
	{
		std::string name {};
		std::string md5 {};
	} map;

	struct StyleInfo
	{
		std::string name {};
		std::string md5 {};
	};

	std::vector<StyleInfo> styles;
};

struct RunReplayHeader
{
	std::string courseName;
};

struct CheaterReplayHeader
{
	u32 reason {};
};

struct JumpstatReplayHeader
{
	// Track which jump event in the file is the relevant one.
	u32 jumpEventID {};
};

struct ManualReplayHeader
{
	std::string name;
};

struct TickData
{
	// Sometimes there are multiple ticks of movement in a single server tick.
	// Apart from the latest tick, up to 4 other ticks will be run at timescale 0, and if there are more, the buttons will be condensed so the server
	// doesn't run more than 4 commands (or whatever the value of sv_late_commands_allowed is).

	// Player command stuff
	// This is only meaningful for the anticheat. Playback doesn't use this.
	std::vector<SubtickMove> cmdSubtickMoves;
	i32 mousedx {};
	i32 mousedy {};
	i32 weapon {};
	QAngle angles;
	u64 buttons[3] {};
	// True if the command is not a substitute or a null command.
	bool isNewCommand {};
	// Technically, these arriving commands will not necessarily be processed on this tick, but it is still useful to verify if something is off.
	u32 numCommandArrivedThisTick {};

	// Data to override in CreateMove.
	struct
	{
		// CMoveData stuff
		f32 forwardmove;
		f32 sidemove;
		f32 upmove;
		Vector origin;
		QAngle angles;
		Vector velocity;
		// These subtick moves do not match the cmdSubtickMoves as it can be modified by modes,
		// and subtickMoves always have one extra when = 1.0 empty move.
		std::vector<SubtickMove> subtickMoves;
		// Movement services stuff
		// Why are there 6+ variables that track crouching again?
		bool inCrouch;
		bool ducked;
		bool ducking;
		bool oldJumpPressed;
		u32 crouchState;
		// These variables must be made to be relative to the replay!!
		f32 crouchTransitionStartTime;
		f32 jumpPressedTime;
		f32 lastDuckTime;
		// Other stuff
		MoveType_t m_MoveType;
	} preTickData;

	// Data to override after player movement.
	struct
	{
		// CMoveData stuff
		Vector origin;
		QAngle angles;
		Vector velocity;
		bool ducked;
		bool ducking;
		bool inAir;
	} postTickData;
};

struct ReplayData
{
	std::vector<ReplayEvent *> event;
	std::vector<TickData> tickData;
};

class KZReplayService : public KZBaseService
{
	using KZBaseService::KZBaseService;
};
