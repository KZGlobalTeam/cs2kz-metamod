#pragma once

#include <stdint.h>
typedef int32_t GameTick_t;
typedef float GameTime_t;
typedef uint64_t CNetworkedQuantizedFloat;
typedef uint32_t SoundEventGuid_t;


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
#include "utils/entity/cbaseentity.h"
#include "utils/entity/cbasemodelentity.h"
#include "utils/entity/cbaseplayercontroller.h"
#include "utils/entity/cbaseplayerpawn.h"
#include "utils/entity/ccollisionproperty.h"
#include "utils/entity/ccsplayercontroller.h"
#include "utils/entity/ccsplayerpawn.h"
#include "utils/entity/services.h"

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

enum EInButtonState : uint64_t
{
	IN_BUTTON_UP = 0x0,
	IN_BUTTON_DOWN = 0x1,
	IN_BUTTON_DOWN_UP = 0x2,
	IN_BUTTON_UP_DOWN = 0x3,
	IN_BUTTON_UP_DOWN_UP = 0x4,
	IN_BUTTON_DOWN_UP_DOWN = 0x5,
	IN_BUTTON_DOWN_UP_DOWN_UP = 0x6,
	IN_BUTTON_UP_DOWN_UP_DOWN = 0x7,
	IN_BUTTON_STATE_COUNT = 0x8,
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
