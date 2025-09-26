#pragma once
#ifndef _WIN32
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses"
#endif
#ifndef _WIN32
#pragma clang diagnostic pop
#endif
#include "ehandle.h"

class CCSPlayerPawn;
class CCSPlayerController;
class CBaseTrigger;

struct TransmitInfo
{
	CBitVec<16384> *m_pTransmitEdict;
};

enum ObserverMode_t : uint32
{
	OBS_MODE_NONE = 0,
	OBS_MODE_FIXED = 1,
	OBS_MODE_IN_EYE = 2,
	OBS_MODE_CHASE = 3,
	OBS_MODE_ROAMING = 4,
	NUM_OBSERVER_MODES = 5,
};

enum MsgDest : int32_t
{
	HUD_PRINTNOTIFY = 1,
	HUD_PRINTCONSOLE = 2,
	HUD_PRINTTALK = 3,
	HUD_PRINTCENTER = 4,
	HUD_PRINTTALK2 = 5, // Not sure what the difference between this and HUD_PRINTTALK is...
	HUD_PRINTALERT = 6
};

enum InputBitMask_t : uint64
{
	IN_NONE = 0x0,
	IN_ALL = 0xffffffffffffffff,
	IN_ATTACK = 0x1,
	IN_JUMP = 0x2,
	IN_DUCK = 0x4,
	IN_FORWARD = 0x8,
	IN_BACK = 0x10,
	IN_USE = 0x20,
	IN_TURNLEFT = 0x80,
	IN_TURNRIGHT = 0x100,
	IN_MOVELEFT = 0x200,
	IN_MOVERIGHT = 0x400,
	IN_ATTACK2 = 0x800,
	IN_RELOAD = 0x2000,
	IN_SPEED = 0x10000,
	IN_JOYAUTOSPRINT = 0x20000,
	IN_FIRST_MOD_SPECIFIC_BIT = 0x100000000,
	IN_USEORRELOAD = 0x100000000,
	IN_SCORE = 0x200000000,
	IN_ZOOM = 0x400000000,
	IN_LOOK_AT_WEAPON = 0x800000000,
};

// Sound stuff.

typedef uint32 SoundEventGuid_t;

struct SndOpEventGuid_t
{
	SndOpEventGuid_t() : m_nGuid(0), m_hStackHash(-1) {}

	SoundEventGuid_t m_nGuid;
	uint32 m_hStackHash;
	// #define SND_OP_FLAG_DEFAULT (1 << 0)
	// #define SND_OP_FLAG_RELIABLE (1 << 1)
	// #define SND_OP_FLAG_INIT (1 << 2)
	// #define SND_OP_FLAG_VOICE (1 << 3)
	uint8 m_nFlags;
	CPlayerBitVec m_Players;
};

struct EmitSound_t
{
	// clang-format off
	EmitSound_t() :
		m_pSoundName( 0 ),
		m_flVolume( VOL_NORM ),
		m_flSoundTime( 0.0f ),
		m_nSpeakerEntity( -1 ),
		m_nForceGuid( 0 ),
		m_nSourceSoundscape( 0 ),
		m_nPitch( PITCH_NORM )
	{
	}

	// clang-format on
	const char *m_pSoundName;
	Vector m_vecOrigin;
	float m_flVolume;    // soundevent's volume_atten
	float m_flSoundTime; // sound delay
	CEntityIndex m_nSpeakerEntity;
	SoundEventGuid_t m_nForceGuid;
	CEntityIndex m_nSourceSoundscape;
	uint16 m_nPitch; // Pretty sure this is unused.
	// (1<<3) overrides the source soundscape with the speaker entity.
	// (1<<4) emits sound at specified position, otherwise attached to entity index.
	// Possibly share the same flags as SndOpEventGuid_t.
	uint8 m_nFlags;
};

// Tracing stuff.

struct touchlist_t
{
	Vector deltavelocity;
	trace_t trace;
};

struct SubtickMove
{
	float when;
	uint64 button;

	union
	{
		bool pressed;

		struct
		{
			float analog_forward_delta;
			float analog_left_delta;
			float analog_pitch_delta;
			float analog_yaw_delta;
		} analogMove;
	};

	bool IsAnalogInput()
	{
		return button == 0;
	}
};

class CMoveDataBase
{
public:
	CMoveDataBase() = default;
	CMoveDataBase(const CMoveDataBase &source)
		// clang-format off
		: m_bHasZeroFrametime {source.m_bHasZeroFrametime},
		m_bIsLateCommand {source.m_bIsLateCommand}, 
		m_nPlayerHandle {source.m_nPlayerHandle},
		m_vecAbsViewAngles {source.m_vecAbsViewAngles},
		m_vecViewAngles {source.m_vecViewAngles},
		m_vecLastMovementImpulses {source.m_vecLastMovementImpulses},
		m_flForwardMove {source.m_flForwardMove}, 
		m_flSideMove {source.m_flSideMove}, 
		m_flUpMove {source.m_flUpMove},
		m_vecVelocity {source.m_vecVelocity}, 
		m_vecAngles {source.m_vecAngles},
		m_vecUnknown {source.m_vecUnknown},
		m_bHasSubtickInputs {source.m_bHasSubtickInputs},
		unknown {source.unknown},
		m_collisionNormal {source.m_collisionNormal},
		m_groundNormal {source.m_groundNormal},
		m_vecAbsOrigin {source.m_vecAbsOrigin},
		m_nTickCount {source.m_nTickCount},
		m_nTargetTick {source.m_nTargetTick},
		m_flSubtickStartFraction {source.m_flSubtickStartFraction},
		m_flSubtickEndFraction {source.m_flSubtickEndFraction}
	// clang-format on
	{
		for (int i = 0; i < source.m_AttackSubtickMoves.Count(); i++)
		{
			this->m_AttackSubtickMoves.AddToTail(source.m_AttackSubtickMoves[i]);
		}
		for (int i = 0; i < source.m_SubtickMoves.Count(); i++)
		{
			this->m_SubtickMoves.AddToTail(source.m_SubtickMoves[i]);
		}
		for (int i = 0; i < source.m_TouchList.Count(); i++)
		{
			auto touch = this->m_TouchList.AddToTailGetPtr();
			touch->deltavelocity = m_TouchList[i].deltavelocity;
			touch->trace.m_pSurfaceProperties = m_TouchList[i].trace.m_pSurfaceProperties;
			touch->trace.m_pEnt = m_TouchList[i].trace.m_pEnt;
			touch->trace.m_pHitbox = m_TouchList[i].trace.m_pHitbox;
			touch->trace.m_hBody = m_TouchList[i].trace.m_hBody;
			touch->trace.m_hShape = m_TouchList[i].trace.m_hShape;
			touch->trace.m_nContents = m_TouchList[i].trace.m_nContents;
			touch->trace.m_BodyTransform = m_TouchList[i].trace.m_BodyTransform;
			touch->trace.m_vHitNormal = m_TouchList[i].trace.m_vHitNormal;
			touch->trace.m_vHitPoint = m_TouchList[i].trace.m_vHitPoint;
			touch->trace.m_flHitOffset = m_TouchList[i].trace.m_flHitOffset;
			touch->trace.m_flFraction = m_TouchList[i].trace.m_flFraction;
			touch->trace.m_nTriangle = m_TouchList[i].trace.m_nTriangle;
			touch->trace.m_nHitboxBoneIndex = m_TouchList[i].trace.m_nHitboxBoneIndex;
			touch->trace.m_eRayType = m_TouchList[i].trace.m_eRayType;
			touch->trace.m_bStartInSolid = m_TouchList[i].trace.m_bStartInSolid;
			touch->trace.m_bExactHitPoint = m_TouchList[i].trace.m_bExactHitPoint;
		}
	}

public:
	bool m_bHasZeroFrametime: 1;
	bool m_bIsLateCommand: 1;
	CHandle<CCSPlayerPawn> m_nPlayerHandle;
	QAngle m_vecAbsViewAngles;
	QAngle m_vecViewAngles;
	Vector m_vecLastMovementImpulses;
	float m_flForwardMove;
	float m_flSideMove; // Warning! Flipped compared to CS:GO, moving right gives negative value
	float m_flUpMove;
	Vector m_vecVelocity;
	QAngle m_vecAngles;
	Vector m_vecUnknown; // Unused. Probably pulled from engine upstream.
	CUtlVector<SubtickMove> m_SubtickMoves;
	CUtlVector<SubtickMove> m_AttackSubtickMoves;
	bool m_bHasSubtickInputs;
	float unknown; // Set to 1.0 during SetupMove, never change during gameplay. Is apparently used for weapon services stuff.
	CUtlVector<touchlist_t> m_TouchList;
	Vector m_collisionNormal;
	Vector m_groundNormal;
	Vector m_vecAbsOrigin;
	int32_t m_nTickCount;
	int32_t m_nTargetTick;
	float m_flSubtickStartFraction;
	float m_flSubtickEndFraction;
};

class CMoveData : public CMoveDataBase
{
public:
	CMoveData() = default;

	CMoveData(const CMoveData &source)
		: CMoveDataBase(source), m_outWishVel {source.m_outWishVel}, m_vecOldAngles {source.m_vecOldAngles},
		  m_vecInputRotated {source.m_vecInputRotated}, m_vecContinousAcceleration {source.m_vecContinousAcceleration},
		  m_vecFrameVelocityDelta {source.m_vecFrameVelocityDelta}, m_flMaxSpeed {source.m_flMaxSpeed}
	{
	}

	Vector m_outWishVel;
	QAngle m_vecOldAngles;
	// World space input vector. Used to compare against last the movement services' previous rotation for ground movement stuff.
	Vector m_vecInputRotated;
	// u/s^2.
	Vector m_vecContinousAcceleration;
	// Immediate delta in u/s. Air acceleration bypasses per second acceleration, applies up to half of its impulse to the velocity and the rest goes
	// straight into this.
	Vector m_vecFrameVelocityDelta;
	float m_flMaxSpeed;
	float m_flClientMaxSpeed;
	float m_flFrictionDecel;
	bool m_bInAir;
	bool m_bGameCodeMovedPlayer; // true if usercmd cmd number == (m_nGameCodeHasMovedPlayerAfterCommand + 1)
};
#ifdef _WIN32
static_assert(sizeof(CMoveData) == 312, "Class didn't match expected size");
#else
static_assert(sizeof(CMoveData) == 304, "Class didn't match expected size");
#endif
// Custom data types goes here.

enum TurnState
{
	TURN_LEFT = -1,
	TURN_NONE = 0,
	TURN_RIGHT = 1
};

class CTraceFilterHitAllTriggers : public CTraceFilter
{
public:
	CTraceFilterHitAllTriggers()
	{
		m_nInteractsAs = 0;
		m_nInteractsExclude = 0;
		m_nInteractsWith = 4;
		m_nEntityIdsToIgnore[0] = -1;
		m_nEntityIdsToIgnore[1] = -1;
		m_nOwnerIdsToIgnore[0] = -1;
		m_nOwnerIdsToIgnore[1] = -1;
		m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
		m_nHierarchyIds[0] = 0;
		m_nHierarchyIds[1] = 0;
		m_bIterateEntities = true;
		m_nCollisionGroup = COLLISION_GROUP_DEBRIS;
		m_bHitTrigger = true;
	}

	CUtlVector<CEntityHandle> hitTriggerHandles;

	virtual ~CTraceFilterHitAllTriggers()
	{
		hitTriggerHandles.Purge();
	}

	virtual bool ShouldHitEntity(CEntityInstance *other) override
	{
		hitTriggerHandles.AddToTail(other->GetRefEHandle());
		return false;
	}
};
