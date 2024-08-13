#pragma once
#ifndef _WIN32
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses"
#endif
#ifndef _WIN32
#pragma clang diagnostic pop
#endif
#include "ehandle.h"
#include "gametrace.h"

class CCSPlayerPawn;
class CCSPlayerController;
class CBaseTrigger;

struct TransmitInfo
{
	CBitVec<16384> *m_pTransmitEdict;
};

enum ObserverMode_t : uint8_t
{
	OBS_MODE_NONE = 0x0,
	OBS_MODE_FIXED = 0x1,
	OBS_MODE_IN_EYE = 0x2,
	OBS_MODE_CHASE = 0x3,
	OBS_MODE_ROAMING = 0x4,
	OBS_MODE_DIRECTED = 0x5,
	NUM_OBSERVER_MODES = 0x6,
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

enum InputBitMask_t : uint64_t
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
};

// used with EmitSound_t
enum gender_t : uint8
{
	GENDER_NONE = 0x0,
	GENDER_MALE = 0x1,
	GENDER_FEMALE = 0x2,
	GENDER_NAMVET = 0x3,
	GENDER_TEENGIRL = 0x4,
	GENDER_BIKER = 0x5,
	GENDER_MANAGER = 0x6,
	GENDER_GAMBLER = 0x7,
	GENDER_PRODUCER = 0x8,
	GENDER_COACH = 0x9,
	GENDER_MECHANIC = 0xA,
	GENDER_CEDA = 0xB,
	GENDER_CRAWLER = 0xC,
	GENDER_UNDISTRACTABLE = 0xD,
	GENDER_FALLEN = 0xE,
	GENDER_RIOT_CONTROL = 0xF,
	GENDER_CLOWN = 0x10,
	GENDER_JIMMY = 0x11,
	GENDER_HOSPITAL_PATIENT = 0x12,
	GENDER_BRIDE = 0x13,
	GENDER_LAST = 0x14,
};

struct EmitSound_t
{
	// clang-format off
	EmitSound_t() :
		m_nChannel( 0 ),
		m_pSoundName( 0 ),
		m_flVolume( VOL_NORM ),
		m_SoundLevel( SNDLVL_NONE ),
		m_nFlags( 0 ),
		m_nPitch( PITCH_NORM ),
		m_pOrigin( 0 ),
		m_flSoundTime( 0.0f ),
		m_pflSoundDuration( 0 ),
		m_bEmitCloseCaption( true ),
		m_bWarnOnMissingCloseCaption( false ),
		m_bWarnOnDirectWaveReference( false ),
		m_nSpeakerEntity( -1 ),
		m_UtlVecSoundOrigin(),
		m_nForceGuid( 0 ),
		m_SpeakerGender( GENDER_NONE )
	{
	}

	// clang-format on
	int m_nChannel;
	const char *m_pSoundName;
	float m_flVolume;
	soundlevel_t m_SoundLevel;
	int m_nFlags;
	int m_nPitch;
	const Vector *m_pOrigin;
	float m_flSoundTime;
	float *m_pflSoundDuration;
	bool m_bEmitCloseCaption;
	bool m_bWarnOnMissingCloseCaption;
	bool m_bWarnOnDirectWaveReference;
	CEntityIndex m_nSpeakerEntity;
	CUtlVector<Vector, CUtlMemory<Vector, int>> m_UtlVecSoundOrigin;
	SoundEventGuid_t m_nForceGuid;
	gender_t m_SpeakerGender;
};

// Tracing stuff.

struct touchlist_t
{
	Vector deltavelocity;
	trace_t trace;
};

class CTraceFilterPlayerMovementCS : public CTraceFilter
{
};

// TODO: Remove this!
struct CCheckTransmitInfoS2
{
	CBitVec<16384> *m_pTransmitEdict;
	uint8 unk[1000];
};

struct SubtickMove
{
	float when;
	uint64 button;
	bool pressed;
};

// Size: 0xF8
class CMoveData
{
public:
	CMoveData() = default;

	CMoveData(const CMoveData &source)
		// clang-format off
		: moveDataFlags {source.moveDataFlags}, 
		m_nPlayerHandle {source.m_nPlayerHandle},
		m_vecAbsViewAngles {source.m_vecAbsViewAngles},
		m_vecViewAngles {source.m_vecViewAngles},
		m_vecLastMovementImpulses {source.m_vecLastMovementImpulses},
		m_flForwardMove {source.m_flForwardMove}, 
		m_flSideMove {source.m_flSideMove}, 
		m_flUpMove {source.m_flUpMove},
		m_flSubtickFraction {source.m_flSubtickFraction}, 
		m_vecVelocity {source.m_vecVelocity}, 
		m_vecAngles {source.m_vecAngles},
		m_bHasSubtickInputs {source.m_bHasSubtickInputs},
		m_collisionNormal {source.m_collisionNormal},
		m_groundNormal {source.m_groundNormal}, 
		m_vecAbsOrigin {source.m_vecAbsOrigin},
		m_nGameModeMovedPlayer {source.m_nGameModeMovedPlayer},
		m_outWishVel {source.m_outWishVel},
		m_vecOldAngles {source.m_vecOldAngles}, 
		m_flMaxSpeed {source.m_flMaxSpeed}, 
		m_flClientMaxSpeed {source.m_flClientMaxSpeed},
		m_flSubtickAccelSpeed {source.m_flSubtickAccelSpeed}, 
		m_bJumpedThisTick {source.m_bJumpedThisTick},
		m_bOnGround {source.m_bOnGround},
		m_bShouldApplyGravity {source.m_bShouldApplyGravity}, 
		m_bGameCodeMovedPlayer {source.m_bGameCodeMovedPlayer}
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
	uint8_t moveDataFlags;
	CHandle<CCSPlayerPawn> m_nPlayerHandle;
	QAngle m_vecAbsViewAngles;
	QAngle m_vecViewAngles;
	Vector m_vecLastMovementImpulses;
	float m_flForwardMove;
	float m_flSideMove; // Warning! Flipped compared to CS:GO, moving right gives negative value
	float m_flUpMove;
	float m_flSubtickFraction;
	Vector m_vecVelocity;
	Vector m_vecAngles;
	CUtlVector<SubtickMove> m_SubtickMoves;
	CUtlVector<SubtickMove> m_AttackSubtickMoves;
	bool m_bHasSubtickInputs;
	float unknown; // Set to 1.0 during SetupMove, never change during gameplay.
	CUtlVector<touchlist_t> m_TouchList;
	Vector m_collisionNormal;
	Vector m_groundNormal; // unsure
	Vector m_vecAbsOrigin;
	bool m_nGameModeMovedPlayer;
	Vector m_outWishVel;
	Vector m_vecOldAngles;
	float m_flMaxSpeed;
	float m_flClientMaxSpeed;
	float m_flSubtickAccelSpeed; // Related to ground acceleration subtick stuff with sv_stopspeed and friction
	bool m_bJumpedThisTick;      // something to do with basevelocity and the tick the player jumps
	bool m_bOnGround;
	bool m_bShouldApplyGravity;
	bool m_bGameCodeMovedPlayer; // true if usercmd cmd number == (m_nGameCodeHasMovedPlayerAfterCommand + 1)
};

static_assert(offsetof(CMoveData, m_vecViewAngles) == 0x14);
static_assert(offsetof(CMoveData, m_vecVelocity) == 0x3c);
static_assert(offsetof(CMoveData, m_vecAngles) == 0x48);
static_assert(offsetof(CMoveData, m_vecAbsOrigin) == 0xc0);
static_assert(offsetof(CMoveData, m_outWishVel) == 0xd0);
static_assert(offsetof(CMoveData, m_flMaxSpeed) == 0xe8);
static_assert(offsetof(CMoveData, m_flClientMaxSpeed) == 0xec);
static_assert(offsetof(CMoveData, m_flSubtickAccelSpeed) == 0xf0);
static_assert(offsetof(CMoveData, m_bJumpedThisTick) == 0xf4);
static_assert(offsetof(CMoveData, m_bOnGround) == 0xf5);
static_assert(sizeof(CMoveData) == 248, "Class didn't match expected size");

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
