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

enum RpEventType
{
	RPEVENT_TIMER_START,
	RPEVENT_TIMER_END,
	RPEVENT_TIMER_STOPPED,
	RPEVENT_TIMER_PAUSE
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

struct RpEvent
{
	RpEventType eventType;

	union
	{
		struct
		{
			i32 courseId;
		} timerStart;

		struct
		{
			i32 courseId;
			f32 time;
			u32 teleportsUsed;
		} timerEnd;

		struct
		{
			i32 courseId;
		} timerStopped;
	};
};

struct SubtickData
{
	u32 numSubtickMoves;

	struct RpSubtickMove
	{
		float when;
		// u32 instead of u64 because buttons above 32 bits look irrelevant.
		u32 button;

		union
		{
			bool pressed;

			struct
			{
				float analog_forward_delta;
				float analog_left_delta;
				float pitch_delta;
				float yaw_delta;
			} analogMove;
		};

		bool IsAnalogInput()
		{
			return button == 0;
		}
	} subtickMoves[64];
};

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

	struct
	{
		char name[128];
		u64 steamid64;
	} player;

	struct
	{
		char name[64];
		char md5[33];
	} mode;

	struct
	{
		char name[64];
		char md5[33];
	} map;

	EconInfo firstWeapon;
	// Probably not worth the effort to track models and gloves over time, since this won't affect gameplay in any way that matters.
	EconInfo gloves;
	char modelName[256];

	// TODO: styles!
#if 0
	i32 styleCount;
	struct StyleInfo;
	{
		char name[64];
		char md5[33];
	}
	StyleInfo *styles;
#endif
};

#endif // KZ_REPLAY_H
