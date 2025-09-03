#ifndef KZ_REPLAY_H
#define KZ_REPLAY_H

#include "common.h"
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
	bool desiresDuck: 1;
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
		float when {};
		// u32 instead of u64 because buttons above 32 bits look irrelevant.
		u32 button {};

		union
		{
			bool pressed {};

			struct
			{
				float analog_forward_delta {};
				float analog_left_delta {};
				float analog_pitch_delta {};
				float analog_yaw_delta {};
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
	i32 serverTick {};
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

	void PrintDebug()
	{
		char buf[8192];
		int len =
			snprintf(buf, sizeof(buf),
					 "TickData Debug Info:\n"
					 "  Server Tick: %d\n"
					 "  Game Time: %f\n"
					 "  Real Time: %f\n"
					 "  Unix Time: %llu\n"
					 "  Cmd Number: %d\n"
					 "  Client Tick: %d\n"
					 "  Forward: %f\n"
					 "  Left: %f\n"
					 "  Up: %f\n"
					 "  Pre Data:\n"
					 "    Origin: %f %f %f\n"
					 "    Velocity: %f %f %f\n"
					 "    Angles: %f %f %f\n"
					 "    Buttons: %d %d %d\n"
					 "    Jump Pressed Time: %f\n"
					 "    Duck Speed: %f\n"
					 "    Duck Amount: %f\n"
					 "    Duck Offset: %f\n"
					 "    Last Duck Time: %f\n"
					 "    Replay Flags: %d %d %d\n"
					 "    Entity Flags: %d\n"
					 "    Move Type: %d\n"
					 "  Post Data:\n"
					 "    Origin: %f %f %f\n"
					 "    Velocity: %f %f %f\n"
					 "    Angles: %f %f %f\n"
					 "    Buttons: %d %d %d\n"
					 "    Jump Pressed Time: %f\n"
					 "    Duck Speed: %f\n"
					 "    Duck Amount: %f\n"
					 "    Duck Offset: %f\n"
					 "    Last Duck Time: %f\n"
					 "    Replay Flags: %d %d %d\n"
					 "    Entity Flags: %d\n"
					 "    Move Type: %d\n",
					 this->serverTick, this->gameTime, this->realTime, this->unixTime, this->cmdNumber, this->clientTick, this->forward, this->left,
					 this->up, this->pre.origin.x, this->pre.origin.y, this->pre.origin.z, this->pre.velocity.x, this->pre.velocity.y,
					 this->pre.velocity.z, this->pre.angles.x, this->pre.angles.y, this->pre.angles.z, this->pre.buttons[0], this->pre.buttons[1],
					 this->pre.buttons[2], this->pre.jumpPressedTime, this->pre.duckSpeed, this->pre.duckAmount, this->pre.duckOffset,
					 this->pre.lastDuckTime, this->pre.replayFlags.ducking, this->pre.replayFlags.ducked, this->pre.replayFlags.desiresDuck,
					 this->pre.entityFlags, this->pre.moveType, this->post.origin.x, this->post.origin.y, this->post.origin.z, this->post.velocity.x,
					 this->post.velocity.y, this->post.velocity.z, this->post.angles.x, this->post.angles.y, this->post.angles.z,
					 this->post.buttons[0], this->post.buttons[1], this->post.buttons[2], this->post.jumpPressedTime, this->post.duckSpeed,
					 this->post.duckAmount, this->post.duckOffset, this->post.lastDuckTime, this->post.replayFlags.ducking,
					 this->post.replayFlags.ducked, this->post.replayFlags.desiresDuck, this->post.entityFlags, this->post.moveType);

		// Print in 2048 char chunks
		for (int i = 0; i < len; i += 2048)
		{
			char chunk[2049];
			int chunkLen = (len - i > 2048) ? 2048 : (len - i);
			memcpy(chunk, buf + i, chunkLen);
			chunk[chunkLen] = '\0';
			META_CONPRINTF("%s", chunk);
		}
	}
};

// While tick data is only recorded during certain conditions, metadata is recorded every time the server receives a move message from the client.
// TODO
struct CmdData
{
	i32 serverTick {};
	f32 gameTime {};
	f32 realTime {};
	u64 unixTime {};
	f32 framerate {};
	f32 avgLatency {};
	f32 avgLoss {};
	u32 cmdNumber {};
	u32 clientTick {};
	f32 forward {};
	f32 left {};
	f32 up {};
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
