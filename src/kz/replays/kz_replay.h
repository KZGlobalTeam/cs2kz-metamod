
#ifndef KZ_REPLAY_H
#define KZ_REPLAY_H

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
	RPEVENT_TIMER_PAUSE,
	RPEVENT_NEW_WEAPON,
	RPEVENT_REMOVE_WEAPON,
};

struct RpFlags
{
	bool ducking: 1;
	bool ducked: 1;
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
		
		struct
		{
			i32 courseId;
		} timerStopped;
	};
};

struct Tickdata
{
	i32 gameTick;
	f32 gameTime;
	f32 realTime;
	u64 unixTime;

	Vector origin;
	Vector velocity;
	QAngle angles;
	// Quack
	f32 duckSpeed;
	f32 duckAmount;
	u64 buttons;
	RpFlags replayFlags;
	u32 entityFlags;
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
