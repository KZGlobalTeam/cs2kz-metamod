#ifndef KZ_REPLAY_H
#define KZ_REPLAY_H

#include "common.h"
#include "sdk/entity/cbaseplayerweapon.h"
// relative to csgo/
#define KZ_REPLAY_PATH      "kzreplays"
#define KZ_REPLAY_RUNS_PATH KZ_REPLAY_PATH "/runs"

enum : u32
{
	KZ_REPLAY_MAGIC = KZ_FOURCC('s', '2', 'k', 'z'),
	KZ_REPLAY_VERSION = 1,
};

enum ReplayType : u32
{
	RP_MANUAL = 0,
	RP_CHEATER = 1, // Contain a ban reason
	RP_RUN = 2,
	RP_JUMPSTATS = 3 // Contain all jumpstats events that happen throughout the replay. Does not contain timer events.
};

enum RpEventType
{
	RPEVENT_TIMER_EVENT,
	RPEVENT_MODE_CHANGE,
	RPEVENT_STYLE_ADD,
	RPEVENT_STYLE_REMOVE,
	RPEVENT_TELEPORT
};

struct RpFlags
{
	bool ducking: 1;
	bool ducked: 1;
	bool desiresDuck: 1;
};

struct WeaponSwitchEvent
{
	u32 serverTick {};
	EconInfo econInfo;
};

struct RpModeStyleInfo
{
	char name[64];
	char md5[33];
};

struct RpStyleChangeInfo : public RpModeStyleInfo
{
	bool add;
};

struct RpEvent
{
	RpEventType type;
	f64 serverTick; // Use f64 to avoid precision loss for large tick values.

	union
	{
		struct
		{
			enum
			{
				TIMER_START,
				TIMER_END,
				TIMER_STOP,
				TIMER_PAUSE,
				TIMER_RESUME,
				TIMER_SPLIT,
				TIMER_CPZ,
				TIMER_STAGE,
			} type;

			i32 index; // Course ID for start/end, split number for split, checkpoint number for cpz, stage number for stage.
			f32 time;  // Final time for end, time reached split/checkpoint/stage for split/cpz/stage. Current time for pause/stop/resume.
		} timer;

		RpModeStyleInfo modeChange;

		RpStyleChangeInfo styleChange;

		struct
		{
			bool hasOrigin;
			bool hasAngles;
			bool hasVelocity;
			f32 origin[3];
			f32 angles[3];
			f32 velocity[3];
		} teleport;
	} data;
};

static_assert(std::is_trivial<RpEvent>::value, "RpEvent must be a trivial type");

struct SubtickData
{
	u32 numSubtickMoves;

	struct RpSubtickMove
	{
		f32 when;
		// u32 instead of u64 because buttons above 32 bits look irrelevant.
		u32 button;

		union
		{
			bool pressed;

			struct
			{
				f32 analog_forward_delta;
				f32 analog_left_delta;
				f32 pitch_delta;
				f32 yaw_delta;
			} analogMove;
		};

		bool IsAnalogInput()
		{
			return button == 0;
		}
	} subtickMoves[64];
};

static_assert(std::is_trivial<SubtickData>::value, "SubtickData must be a trivial type");

struct TickData
{
	u32 serverTick {};
	f32 gameTime {};
	f32 realTime {};
	u64 unixTime {};
	/* Note:
		Pre data is obtained at the start of physics simulation, post data is obtained at the end.
		Current input represents a simple version of CUserCmd, and is obtained on SetupMove/CreateMove pre hook.
	*/
	// This can change multiple times in a tick, for the sake of playback we just store and use the very last input (which should have a non zero
	// frametime).
	u32 cmdNumber {};
	u32 clientTick {};
	f32 forward {};
	f32 left {};
	f32 up {};
	bool leftHanded {};

	struct
	{
		Vector origin;
		Vector velocity;
		QAngle angles;
		u32 buttons[3] {};
		f32 jumpPressedTime {};
		// Quack
		f32 duckSpeed {};
		f32 duckAmount {};
		f32 duckOffset {};
		f32 lastDuckTime {};
		RpFlags replayFlags {};
		u32 entityFlags {};
		MoveType_t moveType = MOVETYPE_WALK;
	} pre, post;
};

// While tick data is only recorded during certain conditions, metadata is recorded every time the server receives a move message from the client.
struct CmdData
{
	i32 serverTick {};
	f32 gameTime {};
	f32 realTime {};
	u64 unixTime {};
	f32 framerate {};
	f32 latency {};
	f32 avgLoss {};
	u32 cmdNumber {};
	u32 clientTick {};
	f32 forward {};
	f32 left {};
	f32 up {};
	u64 buttons[3] {};
	QAngle angles {};
	i32 mousedx {};
	i32 mousedy {};
};

struct ReplayHeader
{
	u32 magicNumber;
	u32 version;
	ReplayType type;

	struct
	{
		char name[128];
		u64 steamid64;
	} player;

	struct
	{
		char name[64];
		char md5[33];
	} map;

	EconInfo firstWeapon;
	// Probably not worth the effort to track player models and gloves over time, since this won't affect gameplay in any way that matters.
	EconInfo gloves;
	char modelName[256];
};

#endif // KZ_REPLAY_H
