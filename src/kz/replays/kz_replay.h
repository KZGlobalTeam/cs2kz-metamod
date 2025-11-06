#ifndef KZ_REPLAY_H
#define KZ_REPLAY_H

#include "common.h"
#include "sdk/entity/cbaseplayerweapon.h"
// relative to csgo/
#define KZ_REPLAY_PATH      "kzreplays"
#define KZ_REPLAY_RUNS_PATH KZ_REPLAY_PATH "/runs"

class CSubtickMoveStep;
class KZPlayer;

enum : u32
{
	KZ_REPLAY_MAGIC = KZ_FOURCC('s', '2', 'k', 'z'),
	KZ_REPLAY_VERSION = 1,
};

enum ReplayType : u32
{
	RP_MANUAL = 0,  // Contain who saved the replay.
	RP_CHEATER = 1, // Contain a reason and a reporter.
	RP_RUN = 2,
	RP_JUMPSTATS = 3 // Contain all jumpstats events that happen throughout the replay. Does not contain timer events.
};

enum RpEventType
{
	RPEVENT_TIMER_EVENT,
	RPEVENT_MODE_CHANGE,
	RPEVENT_STYLE_CHANGE,
	RPEVENT_TELEPORT,
	RPEVENT_CHECKPOINT,
	RPEVENT_CVAR,
};

enum RpCvar
{
	RPCVAR_SENSITIVITY,
	RPCVAR_M_YAW,
	RPCVAR_M_PITCH
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
	char shortName[64];
	char md5[33];
};

struct RpStyleChangeInfo : public RpModeStyleInfo
{
	bool clearStyles;
};

class Jump;

// This struct is really big, so we don't store them in run replays.
struct RpJumpStats
{
	struct GeneralData
	{
		u32 serverTick;
		float takeoffOrigin[3];
		float adjustedTakeoffOrigin[3];
		float takeoffVelocity[3];
		float landingOrigin[3];
		float adjustedLandingOrigin[3];

		u8 jumpType;
		u8 distanceTier;
		f32 totalDistance;
		f32 maxSpeed;
		f32 maxHeight;
		f32 airtime;
		f32 duckDuration;
		f32 duckEndDuration;
		f32 release;
		f32 block;
		f32 edge;
		f32 landingEdge;
		char invalidateReason[256];
	} overall;

	struct StrafeData
	{
		f32 duration;
		f32 badAngles;
		f32 overlap;
		f32 deadAir;
		f32 syncDuration;
		f32 width;
		f32 airGain;
		f32 maxGain;
		f32 airLoss;
		f32 collisionGain;
		f32 collisionLoss;
		f32 externalGain;
		f32 externalLoss;
		f32 strafeMaxSpeed;
		bool hasArStats;
		f32 arMax;
		f32 arMedian;
		f32 arAverage;
	};

	std::vector<StrafeData> strafes;

	struct AAData
	{
		u32 strafeIndex;
		f32 externalSpeedDiff;
		f32 prevYaw;
		f32 currentYaw;
		f32 wishDir[3];
		f32 wishSpeed;
		f32 accel;
		f32 surfaceFriction;
		f32 duration;
		u32 buttons[3];
		f32 velocityPre[3];
		f32 velocityPost[3];
		bool ducking;
	};

	std::vector<AAData> aaCalls;

	static void FromJump(RpJumpStats &out, Jump *jump);
	static void ToJump(Jump &out, RpJumpStats *js);
	void PrintJump(KZPlayer *bot);
};

struct RpEvent
{
	RpEventType type;
	u32 serverTick;

	union RpEventData
	{
		struct TimerEvent
		{
			enum TimerEventType
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

		struct
		{
			i32 index;
			i32 checkpointCount;
			i32 teleportCount;
		} checkpoint;

		struct
		{
			RpCvar cvar;
			f32 value;
		} cvar;
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

		void FromMove(const CSubtickMoveStep &step);
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
	// This can change multiple times in a tick.
	// For the sake of playback we just store and use the very last input (which should have a non zero frametime).
	u32 cmdNumber {};
	u32 clientTick {};
	f32 forward {};
	f32 left {};
	f32 up {};
	bool leftHanded {};

	struct MovementData
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
	u32 serverTick {};
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

struct GeneralReplayHeader
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

	u64 timestamp;
	char pluginVersion[32];
	u32 serverVersion;
	u32 serverIP;
	f32 sensitivity;
	f32 yaw;
	f32 pitch;

	// Timestamp when the replay was marked as archived. 0 means not archived.
	// Archived replays older than 14 days will be deleted by the replay watcher.
	u64 archivedTimestamp;

	void Init(KZPlayer *player);
};

struct CheaterReplayHeader
{
	char reason[512];

	// Empty if this is an automated submission.
	struct
	{
		char name[128];
		u64 steamid64;
	} reporter;
};

struct RunReplayHeader
{
	char courseName[256];
	RpModeStyleInfo mode;
	i32 styleCount;
	f32 time;
	i32 numTeleports;
};

struct JumpReplayHeader
{
	RpModeStyleInfo mode;
	u8 jumpType;
	f32 distance;
	i32 blockDistance;
	u32 numStrafes;
	f32 sync;
	f32 pre;
	f32 max;
	f32 airTime;
};

struct ManualReplayHeader
{
	struct
	{
		char name[128];
		u64 steamid64;
	} savedBy;
};
#endif // KZ_REPLAY_H
