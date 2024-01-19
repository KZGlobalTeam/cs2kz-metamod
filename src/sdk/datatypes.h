#pragma once

#include <stdint.h>
typedef int32 GameTick_t;
typedef float GameTime_t;
class CNetworkedQuantizedFloat
{
public:
	float32 m_Value;
	uint16 m_nEncoder;
	bool m_bUnflattened;
};
typedef uint32 SoundEventGuid_t;
struct SndOpEventGuid_t
{
	SoundEventGuid_t m_nGuid;
	uint64 m_hStackHash;
};

#include "utlsymbollarge.h"
#include "entityhandle.h"
#ifndef _WIN32
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses"
#endif
#ifndef _WIN32
#pragma clang diagnostic pop
#endif
#include "ehandle.h"
#include "sdk/entity/cbaseentity.h"
#include "sdk/entity/cbasemodelentity.h"
#include "sdk/entity/cbaseplayercontroller.h"
#include "sdk/entity/cbaseplayerpawn.h"
#include "sdk/ccollisionproperty.h"
#include "sdk/entity/ccsplayercontroller.h"
#include "sdk/entity/ccsplayerpawn.h"
#include "sdk/services.h"

class CPlayer_MovementServices;
class CBaseFilter;
class CBasePlayerPawn;
class CCSPlayerPawn;

struct TransmitInfo
{
	CBitVec<16384> *m_pTransmitEdict;
	uint8 unknown[552];
	CPlayerSlot m_nClientEntityIndex;
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
	HUD_PRINTNOTIFY  = 1,
	HUD_PRINTCONSOLE = 2,
	HUD_PRINTTALK    = 3,
	HUD_PRINTCENTER  = 4,
	HUD_PRINTTALK2   = 5, // Not sure what the difference between this and HUD_PRINTTALK is...
	HUD_PRINTALERT   = 6
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
	CUtlVector<Vector,CUtlMemory<Vector,int> > m_UtlVecSoundOrigin;
	SoundEventGuid_t m_nForceGuid;
	gender_t m_SpeakerGender;
};

class CBaseTrigger : public CBaseEntity2
{

};

struct RnCollisionAttr_t
{ // Unsure, doesn't seem right either for the first few members.
	uint64_t m_nInteractsAs;
	uint64_t m_nInteractsWith;
	uint64_t m_nInteractsExclude;
	uint32_t m_nEntityId;
	uint32_t m_nOwnerId;
	uint16_t m_nHierarchyId;
	uint8_t m_nCollisionGroup;
	uint8_t m_nCollisionFunctionMask;
};

struct alignas(16) trace_t_s2
{
	void *m_pSurfaceProperties;
	void *m_pEnt;
	void *m_pHitbox;
	void *m_hBody;
	void *m_hShape;
	uint64_t contents;
	Vector traceunknown[2];
	uint8_t padding[2];
	RnCollisionAttr_t m_ShapeAttributes;
	Vector startpos;
	Vector endpos;
	Vector planeNormal;
	Vector traceunknown1;
	float traceunknown2;
	float fraction;
	uint8_t traceunknown3[4];
	uint16_t traceunknown4;
	uint8_t traceType;
	bool startsolid;
};

static_assert(offsetof(trace_t_s2, startpos) == 120);
static_assert(offsetof(trace_t_s2, endpos) == 132);
static_assert(offsetof(trace_t_s2, startsolid) == 183);
static_assert(offsetof(trace_t_s2, fraction) == 172);

struct touchlist_t {
	Vector deltavelocity;
	trace_t_s2 trace;
};

class CTraceFilterPlayerMovementCS
{
public:
	void *vtable;
	uint64_t m_nInteractsWith;
	uint64_t m_nInteractsExclude;
	uint64_t m_nInteractsAs;
	uint32_t m_nEntityId[2];
	uint32_t m_nOwnerId[2];
	uint16_t m_nHierarchyId[2];
	uint8_t m_nCollisionFunctionMask;
	uint8_t unk2;
	uint8_t m_nCollisionGroup;
	uint8_t unk3;
	bool unk4;
};

struct vis_info_t
{
	const uint32 *m_pVisBits;
	uint32 m_uVisBitsBufSize;
	float m_flVisibleRadius;
	SpawnGroupHandle_t m_SpawnGroupHandle;
};

struct CCheckTransmitInfoS2
{
	CBitVec<16384> *m_pTransmitEdict;
	uint8 unk[1000];
};

enum TurnState
{
	TURN_LEFT = -1,
	TURN_NONE = 0,
	TURN_RIGHT = 1
};

struct SubtickMove
{
	float when;
	uint64 button;
	bool pressed;
};

// Size: 0xE8
class CMoveData
{
public:
	CMoveData() = default;
	CMoveData( const CMoveData &source ) : 
		moveDataFlags{source.moveDataFlags},
		m_nPlayerHandle{source.m_nPlayerHandle},
		m_vecAbsViewAngles{ source.m_vecAbsViewAngles},
		m_vecViewAngles{source.m_vecViewAngles},
		m_vecLastMovementImpulses{source.m_vecLastMovementImpulses},
		m_flForwardMove{source.m_flForwardMove},
		m_flSideMove{source.m_flSideMove},
		m_flUpMove{source.m_flUpMove},
		m_flSubtickFraction{source.m_flSubtickFraction},
		m_vecVelocity{source.m_vecVelocity},
		m_vecAngles{source.m_vecAngles},
		m_bGameCodeMovedPlayer{source.m_bGameCodeMovedPlayer},
		m_collisionNormal{source.m_collisionNormal},
		m_groundNormal{source.m_groundNormal},
		m_vecAbsOrigin{source.m_vecAbsOrigin},
		m_nGameModeMovedPlayer{source.m_nGameModeMovedPlayer},
		m_vecOldAngles{source.m_vecOldAngles},
		m_flMaxSpeed{source.m_flMaxSpeed},
		m_flClientMaxSpeed{source.m_flClientMaxSpeed},
		m_flSubtickAccelSpeed{source.m_flSubtickAccelSpeed},
		m_bJumpedThisTick{source.m_bJumpedThisTick},
		m_bShouldApplyGravity{source.m_bShouldApplyGravity},
		m_outWishVel{source.m_outWishVel}
	{
		for (int i = 0; i < source.m_SubtickMoves.Count(); i++)
		{
			this->m_SubtickMoves.AddToTail(source.m_SubtickMoves[i]);
		}
		for (int i = 0; i < source.m_TouchList.Count(); i++)
		{
			this->m_TouchList.AddToTail(source.m_TouchList[i]);
		}

	}
public:
	uint8_t moveDataFlags; // 0x0
	CHandle<CCSPlayerPawn> m_nPlayerHandle; // 0x4 don't know if this is actually a CHandle. <CBaseEntity> is a placeholder
	QAngle m_vecAbsViewAngles; // 0x8 unsure
	QAngle m_vecViewAngles; // 0x14
	Vector m_vecLastMovementImpulses;
	float m_flForwardMove; // 0x20
	float m_flSideMove; // 0x24 Warning! Flipped compared to CS:GO, moving right gives negative value
	float m_flUpMove; // 0x28
	float m_flSubtickFraction; // 0x38
	Vector m_vecVelocity; // 0x3c
	Vector m_vecAngles; // 0x48
	CUtlVector<SubtickMove> m_SubtickMoves; // 0x58
	bool m_bGameCodeMovedPlayer; // 0x70
	CUtlVector<touchlist_t> m_TouchList; // 0x78
	Vector m_collisionNormal; // 0x90
	Vector m_groundNormal; // 0x9c unsure
	Vector m_vecAbsOrigin; // 0xa8
	uint8_t padding[4]; // 0xb4 unsure
	bool m_nGameModeMovedPlayer; // 0xb8
	Vector m_vecOldAngles; // 0xbc
	float m_flMaxSpeed; // 0xc8
	float m_flClientMaxSpeed; // 0xcc
	float m_flSubtickAccelSpeed; // 0xd0 Related to ground acceleration subtick stuff with sv_stopspeed and friction
	bool m_bJumpedThisTick; // 0xd4 something to do with basevelocity and the tick the player jumps
	bool m_bShouldApplyGravity; // 0xd5
	Vector m_outWishVel; //0xd8
};
static_assert(sizeof(CMoveData) == 0xE8, "Class didn't match expected size");
