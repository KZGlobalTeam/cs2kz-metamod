#pragma once
#include <stdint.h>
#include "utils/datatypes.h"

enum TurnState
{
	TURN_LEFT = -1,
	TURN_NONE = 0,
	TURN_RIGHT = 1
};

struct MoveDataUnkSubtickStruct
{
	u8 unknown[24];
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
		for (int i = 0; i < source.unknown.Count(); i++)
		{
			this->unknown.AddToTail(source.unknown[i]);
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
	float m_flSideMove; // 0x24
	float m_flUpMove; // 0x28
	float m_flSubtickFraction; // 0x38
	Vector m_vecVelocity; // 0x3c
	Vector m_vecAngles; // 0x48
	CUtlVector<MoveDataUnkSubtickStruct> unknown;
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
