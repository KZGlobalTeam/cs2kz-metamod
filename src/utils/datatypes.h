#pragma once

#include <stdint.h>
typedef int32_t GameTick_t;
typedef float GameTime_t;
typedef uint64_t CNetworkedQuantizedFloat;


#include "utlsymbollarge.h"
#include "ihandleentity.h"
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

class CBaseTrigger : public CBaseEntity2
{

};

struct trace_t_s2
{
	uint8_t traceunknown0[8];
	CBaseEntity* m_pEnt;
	uint8_t traceunknown1[24];
	int contents;
	uint8_t traceunknown2[76];
	Vector startpos;
	Vector endpos;
	Vector planeNormal;
	Vector traceunknown3;
	uint8_t traceunknown4[4];
	float fraction;
	uint8_t traceunknown5[7];
	bool startsolid;
};

struct touchlist_t {
	Vector deltavelocity;
	trace_t_s2 trace;
};

class CTraceFilterPlayerMovementCS
{
public:
	uint8_t tfunk[64];
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
	uint8_t unk[1000];
};