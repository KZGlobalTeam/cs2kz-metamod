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


// Size: 0x1d0
class CPlayer_MovementServices : public CPlayerPawnComponent
{
public:
	uint32_t m_nImpulse; // 0x40
	uint8_t __pad0044[0x4]; // 0x44
	CInButtonState m_nButtons; // 0x48
	uint64_t m_nQueuedButtonDownMask; // 0x68
	uint64_t m_nQueuedButtonChangeMask; // 0x70
	uint64_t m_nButtonDoublePressed; // 0x78
	uint32_t m_pButtonPressedCmdNumber[64]; // 0x80
	uint32_t m_nLastCommandNumberProcessed; // 0x180
	uint8_t __pad0184[0x4]; // 0x184
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	uint64_t m_nToggleButtonDownMask; // 0x188
	// MNetworkEnable
	// MNetworkBitCount "12"
	// MNetworkMinValue "0.000000"
	// MNetworkMaxValue "2048.000000"
	// MNetworkEncodeFlags
	float m_flMaxspeed; // 0x190
	// MNetworkEnable
	float m_arrForceSubtickMoveWhen[4]; // 0x194
	float m_flForwardMove; // 0x1a4
	float m_flLeftMove; // 0x1a8
	float m_flUpMove; // 0x1ac
	Vector m_vecLastMovementImpulses; // 0x1b0
	QAngle m_vecOldViewAngles; // 0x1bc
	uint8_t __pad01c8[0x8]; // 0x44
};
static_assert(sizeof(CPlayer_MovementServices) == 0x1d0, "Class didn't match expected size");

// Size: 0x220
class CPlayer_MovementServices_Humanoid : public CPlayer_MovementServices
{
public:
	float m_flStepSoundTime; // 0x1d0	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	// MNetworkBitCount "17"
	// MNetworkMinValue "-4096.000000"
	// MNetworkMaxValue "4096.000000"
	// MNetworkEncodeFlags
	float m_flFallVelocity; // 0x1d4	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	bool m_bInCrouch; // 0x1d8	
	uint8_t __pad01d9[0x3]; // 0x1d9
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	uint32_t m_nCrouchState; // 0x1dc	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	GameTime_t m_flCrouchTransitionStartTime; // 0x1e0	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	bool m_bDucked; // 0x1e4	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	bool m_bDucking; // 0x1e5	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	bool m_bInDuckJump; // 0x1e6	
	uint8_t __pad01e7[0x1]; // 0x1e7
	Vector m_groundNormal; // 0x1e8	
	float m_flSurfaceFriction; // 0x1f4	
	CUtlStringToken m_surfaceProps; // 0x1f8	
	uint8_t __pad01fc[0xc]; // 0x1fc
	int32_t m_nStepside; // 0x208	
	int32_t m_iTargetVolume; // 0x20c	
	Vector m_vecSmoothedVelocity; // 0x210	
};
static_assert(sizeof(CPlayer_MovementServices_Humanoid) == 0x220, "Class didn't match expected size");

// Size: 0x4f0
class CCSPlayer_MovementServices : public CPlayer_MovementServices_Humanoid
{
public:
	// MNetworkEnable
	float m_flMaxFallVelocity; // 0x220	
	// MNetworkEnable
	// MNetworkEncoder
	Vector m_vecLadderNormal; // 0x224	
	// MNetworkEnable
	int32_t m_nLadderSurfacePropIndex; // 0x230	
	// MNetworkEnable
	float m_flDuckAmount; // 0x234	
	// MNetworkEnable
	float m_flDuckSpeed; // 0x238	
	// MNetworkEnable
	bool m_bDuckOverride; // 0x23c	
	// MNetworkEnable
	bool m_bDesiresDuck; // 0x23d	
	uint8_t __pad023e[0x2]; // 0x23e
	float m_flDuckOffset; // 0x240	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	// MNetworkPriority "32"
	uint32_t m_nDuckTimeMsecs; // 0x244	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	uint32_t m_nDuckJumpTimeMsecs; // 0x248	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	uint32_t m_nJumpTimeMsecs; // 0x24c	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	float m_flLastDuckTime; // 0x250	
	uint8_t __pad0254[0xc]; // 0x254
	Vector2D m_vecLastPositionAtFullCrouchSpeed; // 0x260	
	bool m_duckUntilOnGround; // 0x268	
	bool m_bHasWalkMovedSinceLastJump; // 0x269	
	bool m_bInStuckTest; // 0x26a	
	uint8_t __pad026b[0xd]; // 0x26b
	float m_flStuckCheckTime[2][64]; // 0x278	
	int32_t m_nTraceCount; // 0x478	
	int32_t m_StuckLast; // 0x47c	
	bool m_bSpeedCropped; // 0x480	
	uint8_t __pad0481[0x3]; // 0x481
	int32_t m_nOldWaterLevel; // 0x484	
	float m_flWaterEntryTime; // 0x488	
	Vector m_vecForward; // 0x48c	
	Vector m_vecLeft; // 0x498	
	Vector m_vecUp; // 0x4a4	
	Vector m_vecPreviouslyPredictedOrigin; // 0x4b0	
	bool m_bMadeFootstepNoise; // 0x4bc	
	uint8_t __pad04bd[0x3]; // 0x4bd
	int32_t m_iFootsteps; // 0x4c0	
	// MNetworkEnable
	bool m_bOldJumpPressed; // 0x4c4	
	uint8_t __pad04c5[0x3]; // 0x4c5
	float m_flJumpPressedTime; // 0x4c8	
	// MNetworkEnable
	float m_flJumpUntil; // 0x4cc	
	// MNetworkEnable
	float m_flJumpVel; // 0x4d0	
	// MNetworkEnable
	GameTime_t m_fStashGrenadeParameterWhen; // 0x4d4	
	// MNetworkEnable
	uint64_t m_nButtonDownMaskPrev; // 0x4d8	
	// MNetworkEnable
	float m_flOffsetTickCompleteTime; // 0x4e0	
	// MNetworkEnable
	float m_flOffsetTickStashedSpeed; // 0x4e4	
	// MNetworkEnable
	float m_flStamina; // 0x4e8	
};
static_assert(sizeof(CCSPlayer_MovementServices) == 0x4f0, "Class didn't match expected size");

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
