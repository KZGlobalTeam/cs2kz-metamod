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
#include "vscript/ivscript.h"
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

struct trace_t_s2
{
	uint8 traceunknown0[8];   // 0
	CBaseEntity *m_pEnt;        // 8
	uint8 traceunknown1[24];  // 16
	int contents;               // 40
	uint8 traceunknown2[4];   // 44 (this is probably just alignment padding)
	__m128i traceunknown3;      // 48
	__m128i traceunknown4;      // 64
	uint8 traceunknown5[40];
	Vector startpos;
	Vector endpos;
	Vector planeNormal;
	Vector traceunknown6;
	uint8 traceunknown7[4];
	float fraction;
	uint8 traceunknown8[7];
	bool startsolid;
	uint8 traceunknown9[9];
};

struct touchlist_t {
	Vector deltavelocity;
	trace_t_s2 trace;
};

class CTraceFilterPlayerMovementCS
{
public:
	uint8 tfunk[64];
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

struct Ray_t_s2
{
	Vector start;
};

class CTraceFilterKZ
{
};