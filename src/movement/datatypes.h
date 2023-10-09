#pragma once
#include <stdint.h>
#include "utils/datatypes.h"
// Size: 0x20
class CInButtonState
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	uint64_t m_pButtonStates[3]; // 0x8
};
static_assert(sizeof(CInButtonState) == 0x20, "Class didn't match expected size");


// Size: 0xE8
class CMoveData
{
public:
	uint8_t moveDataFlags; // 0x0
	CHandle<CCSPlayerPawn> m_nPlayerHandle; // 0x4 don't know if this is actually a CHandle. <CBaseEntity> is a placeholder
	QAngle m_vecAbsViewAngles; // 0x8 unsure
	QAngle m_vecViewAngles; // 0x14
	Vector m_vecLastMovementImpulses;
	float m_flForwardMove; // 0x20
	float m_flSideMove; // 0x24
	float m_flUpMove; // 0x28
	float m_flSubtickFraction; // 0x38
	Vector m_vecVelocity; // 0x3c
	Vector m_vecAngles; // 0x48
	uint8_t padding1[4]; //0x54 unsure
	int m_nMovementCmdsThisTick; // 0x58 unsure, but it goes up if you spam lots of key in a tick
	uint8_t padding2[4]; // 0x5c
	uint8_t unknown3[8]; // 0x60 unsure, address of some sort, doesn't seem to change during gameplay
	uint8_t unknown4[8]; // 0x68 always 0, could be padding
	bool m_bGameCodeMovedPlayer; // 0x70
	uint8_t padding3[4]; // 0x74
	CUtlVector<touchlist_t> m_TouchList; // 0x78
	Vector m_collisionNormal; // 0x90
	Vector m_groundNormal; // 0x9c unsure
	Vector m_vecAbsOrigin; // 0xa8
	uint8_t padding4[4]; // 0xb4 unsure
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
