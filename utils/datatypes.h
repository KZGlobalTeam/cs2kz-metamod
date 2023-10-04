#pragma once

#include <stdint.h>
typedef int32_t GameTick_t;
typedef float GameTime_t;
typedef uint64_t CNetworkedQuantizedFloat;


#include "utlsymbollarge.h"
#include "ihandleentity.h"
#include "vscript/ivscript.h"
#include "ehandle.h"

class CPlayer_MovementServices;
class CBaseFilter;
class CBasePlayerPawn;
class CCSPlayerPawn;

enum class EKillTypes_t : uint8_t
{
	KILL_NONE = 0x0,
	KILL_DEFAULT = 0x1,
	KILL_HEADSHOT = 0x2,
	KILL_BLAST = 0x3,
	KILL_BURN = 0x4,
	KILL_SLASH = 0x5,
	KILL_SHOCK = 0x6,
	KILLTYPE_COUNT = 0x7,
};
enum TakeDamageFlags_t : uint32_t
{
	DFLAG_NONE = 0x0,
	DFLAG_SUPPRESS_HEALTH_CHANGES = 0x1,
	DFLAG_SUPPRESS_PHYSICS_FORCE = 0x2,
	DFLAG_SUPPRESS_EFFECTS = 0x4,
	DFLAG_PREVENT_DEATH = 0x8,
	DFLAG_FORCE_DEATH = 0x10,
	DFLAG_ALWAYS_GIB = 0x20,
	DFLAG_NEVER_GIB = 0x40,
	DFLAG_REMOVE_NO_RAGDOLL = 0x80,
	DFLAG_SUPPRESS_DAMAGE_MODIFICATION = 0x100,
	DFLAG_ALWAYS_FIRE_DAMAGE_EVENTS = 0x200,
	DFLAG_RADIUS_DMG = 0x400,
	DMG_LASTDFLAG = 0x400,
	DFLAG_IGNORE_ARMOR = 0x800,
};

enum SurroundingBoundsType_t : uint8_t
{
	USE_OBB_COLLISION_BOUNDS = 0x0,
	USE_BEST_COLLISION_BOUNDS = 0x1,
	USE_HITBOXES = 0x2,
	USE_SPECIFIED_BOUNDS = 0x3,
	USE_GAME_CODE = 0x4,
	USE_ROTATION_EXPANDED_BOUNDS = 0x5,
	USE_COLLISION_BOUNDS_NEVER_VPHYSICS = 0x6,
	USE_ROTATION_EXPANDED_SEQUENCE_BOUNDS = 0x7,
	SURROUNDING_TYPE_BIT_COUNT = 0x3,
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

enum TOGGLE_STATE : uint32_t
{
	TS_AT_TOP = 0x0,
	TS_AT_BOTTOM = 0x1,
	TS_GOING_UP = 0x2,
	TS_GOING_DOWN = 0x3,
	DOOR_OPEN = 0x0,
	DOOR_CLOSED = 0x1,
	DOOR_OPENING = 0x2,
	DOOR_CLOSING = 0x3,
};


enum Disposition_t : uint32_t
{
	D_ER = 0x0,
	D_HT = 0x1,
	D_FR = 0x2,
	D_LI = 0x3,
	D_NU = 0x4,
	D_ERROR = 0x0,
	D_HATE = 0x1,
	D_FEAR = 0x2,
	D_LIKE = 0x3,
	D_NEUTRAL = 0x4,
};

enum Class_T : uint32_t
{
	CLASS_NONE = 0x0,
	CLASS_PLAYER = 0x1,
	CLASS_PLAYER_ALLY = 0x2,
	CLASS_DZ_DRONE = 0x3,
	CLASS_DZ_SPAWN_CHOPPER = 0x4,
	CLASS_BOMB = 0x5,
	CLASS_FOOT_CONTACT_SHADOW = 0x6,
	CLASS_WEAPON_VIEWMODEL = 0x7,
	NUM_CLASSIFY_CLASSES = 0x8,
};

enum Hull_t : uint32_t
{
	HULL_HUMAN = 0x0,
	HULL_SMALL_CENTERED = 0x1,
	HULL_WIDE_HUMAN = 0x2,
	HULL_TINY = 0x3,
	HULL_MEDIUM = 0x4,
	HULL_TINY_CENTERED = 0x5,
	HULL_LARGE = 0x6,
	HULL_LARGE_CENTERED = 0x7,
	HULL_MEDIUM_TALL = 0x8,
	HULL_SMALL = 0x9,
	NUM_HULLS = 0xa,
	HULL_NONE = 0xb,
};

enum ObserverMode_t : uint32_t
{
	OBS_MODE_NONE = 0x0,
	OBS_MODE_FIXED = 0x1,
	OBS_MODE_IN_EYE = 0x2,
	OBS_MODE_CHASE = 0x3,
	OBS_MODE_ROAMING = 0x4,
	OBS_MODE_DIRECTED = 0x5,
	NUM_OBSERVER_MODES = 0x6,
};

enum ChatIgnoreType_t : uint32_t
{
	CHAT_IGNORE_NONE = 0x0,
	CHAT_IGNORE_ALL = 0x1,
	CHAT_IGNORE_TEAM = 0x2,
};

enum PlayerConnectedState : uint32_t
{
	PlayerNeverConnected = 0xffffffffffffffff,
	PlayerConnected = 0x0,
	PlayerConnecting = 0x1,
	PlayerReconnecting = 0x2,
	PlayerDisconnecting = 0x3,
	PlayerDisconnected = 0x4,
	PlayerReserved = 0x5,
};

enum FixAngleSet_t : uint8_t
{
	None = 0x0,
	Absolute = 0x1,
	Relative = 0x2,
};

// Size: 0x6
enum RelativeDamagedDirection_t : uint32_t
{
	DAMAGED_DIR_NONE = 0x0,
	DAMAGED_DIR_FRONT = 0x1,
	DAMAGED_DIR_BACK = 0x2,
	DAMAGED_DIR_LEFT = 0x3,
	DAMAGED_DIR_RIGHT = 0x4,
	DAMAGED_DIR_TOTAL = 0x5,
};

// Size: 0xa
enum CSPlayerState : uint32_t
{
	STATE_ACTIVE = 0x0,
	STATE_WELCOME = 0x1,
	STATE_PICKINGTEAM = 0x2,
	STATE_PICKINGCLASS = 0x3,
	STATE_DEATH_ANIM = 0x4,
	STATE_DEATH_WAIT_FOR_KEY = 0x5,
	STATE_OBSERVER_MODE = 0x6,
	STATE_GUNGAME_RESPAWN = 0x7,
	STATE_DORMANT = 0x8,
	NUM_PLAYER_STATES = 0x9,
};

// Size: 0x12
enum CSPlayerBlockingUseAction_t : uint32_t
{
	k_CSPlayerBlockingUseAction_None = 0x0,
	k_CSPlayerBlockingUseAction_DefusingDefault = 0x1,
	k_CSPlayerBlockingUseAction_DefusingWithKit = 0x2,
	k_CSPlayerBlockingUseAction_HostageGrabbing = 0x3,
	k_CSPlayerBlockingUseAction_HostageDropping = 0x4,
	k_CSPlayerBlockingUseAction_OpeningSafe = 0x5,
	k_CSPlayerBlockingUseAction_EquippingParachute = 0x6,
	k_CSPlayerBlockingUseAction_EquippingHeavyArmor = 0x7,
	k_CSPlayerBlockingUseAction_EquippingContract = 0x8,
	k_CSPlayerBlockingUseAction_EquippingTabletUpgrade = 0x9,
	k_CSPlayerBlockingUseAction_TakingOffHeavyArmor = 0xa,
	k_CSPlayerBlockingUseAction_PayingToOpenDoor = 0xb,
	k_CSPlayerBlockingUseAction_CancelingSpawnRappelling = 0xc,
	k_CSPlayerBlockingUseAction_EquippingExoJump = 0xd,
	k_CSPlayerBlockingUseAction_PickingUpBumpMine = 0xe,
	k_CSPlayerBlockingUseAction_MapLongUseEntity_Pickup = 0xf,
	k_CSPlayerBlockingUseAction_MapLongUseEntity_Place = 0x10,
	k_CSPlayerBlockingUseAction_MaxCount = 0x11,
};

class IntervalTimer
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkEnable
	GameTime_t m_timestamp; // 0x8	
	// MNetworkEnable
	WorldGroupId_t m_nWorldGroupId; // 0xc	
};

template < class T >
class CUtlVectorEmbeddedNetworkVar
{
public:
	uint8_t unknown[0x50];
};

// Size: 0x18
template< class T >
class CNetworkUtlVectorBase
{
public:
	uint8_t unknown[0x18];
};

// Size: 0x4
struct AnimationUpdateListHandle_t
{
public:
	uint32_t m_Value; // 0x0
};

// Size: 0x30
class CNetworkOriginCellCoordQuantizedVector
{
public:
	uint8_t __pad0000[0x10]; // 0x0
	// MNetworkEnable
	// MNetworkChangeCallback "OnCellChanged"
	// MNetworkPriority "31"
	// MNetworkSerializer "cellx"
	uint16_t m_cellX; // 0x10
	// MNetworkEnable
	// MNetworkChangeCallback "OnCellChanged"
	// MNetworkPriority "31"
	// MNetworkSerializer "celly"
	uint16_t m_cellY; // 0x12
	// MNetworkEnable
	// MNetworkChangeCallback "OnCellChanged"
	// MNetworkPriority "31"
	// MNetworkSerializer "cellz"
	uint16_t m_cellZ; // 0x14
	// MNetworkEnable
	uint16_t m_nOutsideWorld; // 0x16
	// MNetworkBitCount "15"
	// MNetworkMinValue "0.000000"
	// MNetworkMaxValue "1024.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "OnCellChanged"
	// MNetworkPriority "31"
	// MNetworkSerializer "posx"
	CNetworkedQuantizedFloat m_vecX; // 0x18
	// MNetworkBitCount "15"
	// MNetworkMinValue "0.000000"
	// MNetworkMaxValue "1024.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "OnCellChanged"
	// MNetworkPriority "31"
	// MNetworkSerializer "posy"
	CNetworkedQuantizedFloat m_vecY; // 0x20
	// MNetworkBitCount "15"
	// MNetworkMinValue "0.000000"
	// MNetworkMaxValue "1024.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "OnCellChanged"
	// MNetworkPriority "31"
	// MNetworkSerializer "posz"
	CNetworkedQuantizedFloat m_vecZ; // 0x28
};
static_assert(sizeof(CNetworkOriginCellCoordQuantizedVector) == 0x30, "Class didn't match expected size");

// Size: 0x28
class CNetworkViewOffsetVector
{
public:
	uint8_t __pad0000[0x10]; // 0x0
	// MNetworkBitCount "10"
	// MNetworkMinValue "-64.000000"
	// MNetworkMaxValue "64.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "CNetworkViewOffsetVector"
	CNetworkedQuantizedFloat m_vecX; // 0x10
	// MNetworkBitCount "10"
	// MNetworkMinValue "-64.000000"
	// MNetworkMaxValue "64.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "CNetworkViewOffsetVector"
	CNetworkedQuantizedFloat m_vecY; // 0x18
	// MNetworkBitCount "20"
	// MNetworkMinValue "0.000000"
	// MNetworkMaxValue "128.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "CNetworkViewOffsetVector"
	CNetworkedQuantizedFloat m_vecZ; // 0x20
};
static_assert(sizeof(CNetworkViewOffsetVector) == 0x28, "Class didn't match expected size");

// Size: 0x28
class CNetworkVarChainer
{
public:
	uint8_t __pad0000[0x20]; // 0x0
	// MNetworkDisable
	// MNetworkChangeAccessorFieldPathIndex
	ChangeAccessorFieldPathIndex_t m_PathIndex; // 0x20
	uint8_t __pad0022[0x6];
};
static_assert(sizeof(CNetworkVarChainer) == 0x28, "Class didn't match expected size");

// Size: 0x40
class CPlayerPawnComponent
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkDisable
	// MNetworkChangeAccessorFieldPathIndex
	CNetworkVarChainer __m_pChainEntity; // 0x8
	CBasePlayerPawn* pawn;
	uint8_t __pad0030[0x6]; // 0x0
};
static_assert(sizeof(CPlayerPawnComponent) == 0x40, "Class didn't match expected size");

// Size: 0x20
class CTransform
{
public:
	Vector m_vPosition;
	uint8_t __padding[4];
public:
	Vector4D m_orientation;
};
static_assert(sizeof(CTransform) == 0x20, "Class didn't match expected size");

// Alignment: 2
// Size: 0x10
class CGameSceneNodeHandle
{
	uint8_t __pad0000[0x8]; // 0x0
public:
	// MNetworkEnable
	CEntityHandle m_hOwner; // 0x8
	// MNetworkEnable
	CUtlStringToken m_name; // 0xc
};
static_assert(sizeof(CGameSceneNodeHandle) == 0x10, "Class didn't match expected size");

// Size: 0x150
class CGameSceneNode
{
public:
	uint8_t __pad0000[0x10]; // 0x0
	// MNetworkDisable
	CTransform m_nodeToWorld; // 0x10
	// MNetworkDisable
	CEntityInstance* m_pOwner; // 0x30
	// MNetworkDisable
	CGameSceneNode* m_pParent; // 0x38
	// MNetworkDisable
	CGameSceneNode* m_pChild; // 0x40
	// MNetworkDisable
	CGameSceneNode* m_pNextSibling; // 0x48
	uint8_t __pad0050[0x20]; // 0x50
	// MNetworkEnable
	// MNetworkSerializer "gameSceneNode"
	// MNetworkChangeCallback "gameSceneNodeHierarchyParentChanged"
	// MNetworkPriority "32"
	// MNetworkVarEmbeddedFieldOffsetDelta "8"
	CGameSceneNodeHandle m_hParent; // 0x70
	// MNetworkEnable
	// MNetworkPriority "32"
	// MNetworkUserGroup "Origin"
	// MNetworkChangeCallback "gameSceneNodeLocalOriginChanged"
	CNetworkOriginCellCoordQuantizedVector m_vecOrigin; // 0x80
	uint8_t __pad00b0[0x8]; // 0xb0
	// MNetworkEnable
	// MNetworkEncoder
	// MNetworkPriority "32"
	// MNetworkSerializer "gameSceneNodeStepSimulationAnglesSerializer"
	// MNetworkChangeCallback "gameSceneNodeLocalAnglesChanged"
	QAngle m_angRotation; // 0xb8
	// MNetworkEnable
	// MNetworkChangeCallback "gameSceneNodeLocalScaleChanged"
	// MNetworkPriority "32"
	float m_flScale; // 0xc4
	// MNetworkDisable
	Vector m_vecAbsOrigin; // 0xc8
	// MNetworkDisable
	QAngle m_angAbsRotation; // 0xd4
	// MNetworkDisable
	float m_flAbsScale; // 0xe0
	// MNetworkDisable
	int16_t m_nParentAttachmentOrBone; // 0xe4
	// MNetworkDisable
	bool m_bDebugAbsOriginChanges; // 0xe6
	// MNetworkDisable
	bool m_bDormant; // 0xe7
	// MNetworkDisable
	bool m_bForceParentToBeNetworked; // 0xe8
	struct
	{
		// MNetworkDisable
		uint8_t m_bDirtyHierarchy : 1;
		// MNetworkDisable
		uint8_t m_bDirtyBoneMergeInfo : 1;
		// MNetworkDisable
		uint8_t m_bNetworkedPositionChanged : 1;
		// MNetworkDisable
		uint8_t m_bNetworkedAnglesChanged : 1;
		// MNetworkDisable
		uint8_t m_bNetworkedScaleChanged : 1;
		// MNetworkDisable
		uint8_t m_bWillBeCallingPostDataUpdate : 1;
		// MNetworkDisable
		uint8_t m_bNotifyBoneTransformsChanged : 1;
		// MNetworkDisable
		uint8_t m_bBoneMergeFlex : 1;
		// MNetworkDisable
		uint8_t m_nLatchAbsOrigin : 2;
		// MNetworkDisable
		uint8_t m_bDirtyBoneMergeBoneToRoot : 1;
	}; // 24 bits
	// MNetworkDisable
	uint8_t m_nHierarchicalDepth; // 0xeb
	// MNetworkDisable
	uint8_t m_nHierarchyType; // 0xec
	// MNetworkDisable
	uint8_t m_nDoNotSetAnimTimeInInvalidatePhysicsCount; // 0xed
	uint8_t __pad00ee[0x2]; // 0xee
	// MNetworkEnable
	CUtlStringToken m_name; // 0xf0
	uint8_t __pad00f4[0x3c]; // 0xf4
	// MNetworkEnable
	// MNetworkChangeCallback "gameSceneNodeHierarchyAttachmentChanged"
	CUtlStringToken m_hierarchyAttachName; // 0x130
	// MNetworkDisable
	float m_flZOffset; // 0x134
	// MNetworkDisable
	Vector m_vRenderOrigin; // 0x138
	uint8_t __pad0144[8];
};
static_assert(sizeof(CGameSceneNode) == 0x150, "Class didn't match expected size");

// Alignment: 2
// Size: 0x50
class CBodyComponent : public CEntityComponent
{
public:
	// MNetworkDisable
	CGameSceneNode* m_pSceneNode; // 0x8
	uint8_t __pad0010[0x10]; // 0x10
	// MNetworkDisable
	// MNetworkChangeAccessorFieldPathIndex
	CNetworkVarChainer __m_pChainEntity; // 0x20
	uint8_t __pad0028[0x8];
};
static_assert(sizeof(CBodyComponent) == 0x50, "Class didn't match expected size");

// Size: 0x1b0
class CNetworkTransmitComponent
{
public:
	uint8_t __pad0000[0x16c]; // 0x0
	uint8_t m_nTransmitStateOwnedCounter; // 0x16c
	uint8_t __pad016d[0x43]; // 0x0
};
static_assert(sizeof(CNetworkTransmitComponent) == 0x1b0, "Class didn't match expected size");

// Size: 0x20
struct thinkfunc_t
{
	uint8_t __pad0000[0x8]; // 0x0
	HSCRIPT m_hFn; // 0x8
	CUtlStringToken m_nContext; // 0x10
	GameTick_t m_nNextThinkTick; // 0x14
	GameTick_t m_nLastThinkTick; // 0x18
};
static_assert(sizeof(thinkfunc_t) == 0x20, "Class didn't match expected size");

// Size: 0x18
struct ResponseContext_t
{
	CUtlSymbolLarge m_iszName; // 0x0
	CUtlSymbolLarge m_iszValue; // 0x8
	GameTime_t m_fExpirationTime; // 0x10
};
static_assert(sizeof(ResponseContext_t) == 0x18, "Class didn't match expected size");

// Size: 0x28
class CEntityIOOutput
{
public:
	uint8_t __pad0000[0x18]; // 0x0
	// TODO: CVariantBase< CVariantDefaultAllocator >
	uint8_t m_Value[0x10]; // 0x18 CVariantBase<CVariantDefaultAllocator>
};
static_assert(sizeof(CEntityIOOutput) == 0x28, "Class didn't match expected size");

// Size: 0x28
class CNetworkVelocityVector
{
public:
	uint8_t __pad0000[0x10]; // 0x0
	// MNetworkBitCount "18"
	// MNetworkMinValue "-4096.000000"
	// MNetworkMaxValue "4096.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "CNetworkVelocityVector"
	CNetworkedQuantizedFloat m_vecX; // 0x10
	// MNetworkBitCount "18"
	// MNetworkMinValue "-4096.000000"
	// MNetworkMaxValue "4096.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "CNetworkVelocityVector"
	CNetworkedQuantizedFloat m_vecY; // 0x18
	// MNetworkBitCount "18"
	// MNetworkMinValue "-4096.000000"
	// MNetworkMaxValue "4096.000000"
	// MNetworkEncodeFlags
	// MNetworkChangeCallback "CNetworkVelocityVector"
	CNetworkedQuantizedFloat m_vecZ; // 0x20
};
static_assert(sizeof(CNetworkVelocityVector) == 0x28, "Class didn't match expected size");

// Size: 0x30
struct VPhysicsCollisionAttribute_t
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkEnable
	uint64_t m_nInteractsAs; // 0x8
	// MNetworkEnable
	uint64_t m_nInteractsWith; // 0x10
	// MNetworkEnable
	uint64_t m_nInteractsExclude; // 0x18
	// MNetworkEnable
	uint32_t m_nEntityId; // 0x20
	// MNetworkEnable
	uint32_t m_nOwnerId; // 0x24
	// MNetworkEnable
	uint16_t m_nHierarchyId; // 0x28
	// MNetworkEnable
	uint8_t m_nCollisionGroup; // 0x2a
	// MNetworkEnable
	uint8_t m_nCollisionFunctionMask; // 0x2b
};

static_assert(sizeof(VPhysicsCollisionAttribute_t) == 0x30, "Class didn't match expected size");

// Size: 0xb0
class CCollisionProperty
{
public:
	uint8_t __pad0000[0x10]; // 0x0
	// MNetworkEnable
	// MNetworkChangeCallback "CollisionAttributeChanged"
	VPhysicsCollisionAttribute_t m_collisionAttribute; // 0x10
	// MNetworkEnable
	// MNetworkChangeCallback "OnUpdateOBB"
	Vector m_vecMins; // 0x40
	// MNetworkEnable
	// MNetworkChangeCallback "OnUpdateOBB"
	Vector m_vecMaxs; // 0x4c
	uint8_t __pad0058[0x2]; // 0x58
	// MNetworkEnable
	// MNetworkChangeCallback "OnUpdateSolidFlags"
	uint8_t m_usSolidFlags; // 0x5a
	// MNetworkEnable
	// MNetworkChangeCallback "OnUpdateSolidType"
	SolidType_t m_nSolidType; // 0x5b
	// MNetworkEnable
	// MNetworkChangeCallback "MarkSurroundingBoundsDirty"
	uint8_t m_triggerBloat; // 0x5c
	// MNetworkEnable
	// MNetworkChangeCallback "MarkSurroundingBoundsDirty"
	SurroundingBoundsType_t m_nSurroundType; // 0x5d
	// MNetworkEnable
	uint8_t m_CollisionGroup; // 0x5e
	// MNetworkEnable
	// MNetworkChangeCallback "OnUpdateEnablePhysics"
	uint8_t m_nEnablePhysics; // 0x5f
	float m_flBoundingRadius; // 0x60
	// MNetworkEnable
	// MNetworkChangeCallback "MarkSurroundingBoundsDirty"
	Vector m_vecSpecifiedSurroundingMins; // 0x64
	// MNetworkEnable
	// MNetworkChangeCallback "MarkSurroundingBoundsDirty"
	Vector m_vecSpecifiedSurroundingMaxs; // 0x70
	Vector m_vecSurroundingMaxs; // 0x7c
	Vector m_vecSurroundingMins; // 0x88
	// MNetworkEnable
	Vector m_vCapsuleCenter1; // 0x94
	// MNetworkEnable
	Vector m_vCapsuleCenter2; // 0xa0
	// MNetworkEnable
	float m_flCapsuleRadius; // 0xac
};
static_assert(sizeof(CCollisionProperty) == 0xb0, "Class didn't match expected size");

// Size: 0x4b0
class CBaseEntity2 : public CEntityInstance
{
public:
	// MNetworkEnable
	// MNetworkUserGroup "CBodyComponent"
	// MNetworkAlias "CBodyComponent"
	// MNetworkTypeAlias "CBodyComponent"
	// MNetworkPriority "48"
	CBodyComponent* m_CBodyComponent; // 0x30
	CNetworkTransmitComponent m_NetworkTransmitComponent; // 0x38
	uint8_t __pad01e8[0x40]; // 0x1e8
	CUtlVector< thinkfunc_t > m_aThinkFunctions; // 0x228
	int32_t m_iCurrentThinkContext; // 0x240
	GameTick_t m_nLastThinkTick; // 0x244
	CGameSceneNode* m_pSceneNode; // 0x248
	CBitVec< 64 > m_isSteadyState; // 0x250
	float m_lastNetworkChange; // 0x258
	uint8_t __pad025c[0xc]; // 0x25c
	CUtlVector< ResponseContext_t > m_ResponseContexts; // 0x268
	CUtlSymbolLarge m_iszResponseContext; // 0x280
	uint8_t __pad0288[0x20]; // 0x288
	// MNetworkEnable
	// MNetworkSerializer "ClampHealth"
	// MNetworkUserGroup "Player"
	// MNetworkPriority "32"
	int32_t m_iHealth; // 0x2a8
	// MNetworkEnable
	int32_t m_iMaxHealth; // 0x2ac
	// MNetworkEnable
	// MNetworkUserGroup "Player"
	// MNetworkPriority "32"
	uint8_t m_lifeState; // 0x2b0
	uint8_t __pad02b1[0x3]; // 0x2b1
	float m_flDamageAccumulator; // 0x2b4
	// MNetworkEnable
	bool m_bTakesDamage; // 0x2b8
	uint8_t __pad02b9[0x3]; // 0x2b9
	// MNetworkEnable
	TakeDamageFlags_t m_nTakeDamageFlags; // 0x2bc
	uint8_t __pad02c0[0x1]; // 0x2c0
	// MNetworkEnable
	MoveCollide_t m_MoveCollide; // 0x2c1
	// MNetworkEnable
	MoveType_t m_MoveType; // 0x2c2
	uint8_t m_nWaterTouch; // 0x2c3
	uint8_t m_nSlimeTouch; // 0x2c4
	bool m_bRestoreInHierarchy; // 0x2c5
	uint8_t __pad02c6[0x2]; // 0x2c6
	CUtlSymbolLarge m_target; // 0x2c8
	float m_flMoveDoneTime; // 0x2d0
	CHandle< CBaseFilter > m_hDamageFilter; // 0x2d4
	CUtlSymbolLarge m_iszDamageFilterName; // 0x2d8
	// MNetworkEnable
	// MNetworkSendProxyRecipientsFilter
	CUtlStringToken m_nSubclassID; // 0x2e0
	uint8_t __pad02e4[0xc]; // 0x2e4
	// MNetworkEnable
	// MNetworkPriority "0"
	// MNetworkSerializer "animTimeSerializer"
	// MNetworkSendProxyRecipientsFilter
	float m_flAnimTime; // 0x2f0
	// MNetworkEnable
	// MNetworkPriority "1"
	// MNetworkSerializer "simulationTimeSerializer"
	// MNetworkSendProxyRecipientsFilter
	float m_flSimulationTime; // 0x2f4
	// MNetworkEnable
	GameTime_t m_flCreateTime; // 0x2f8
	// MNetworkEnable
	bool m_bClientSideRagdoll; // 0x2fc
	// MNetworkEnable
	uint8_t m_ubInterpolationFrame; // 0x2fd
	uint8_t __pad02fe[0x2]; // 0x2fe
	Vector m_vPrevVPhysicsUpdatePos; // 0x300
	// MNetworkEnable
	uint8_t m_iTeamNum; // 0x30c
	uint8_t __pad030d[0x3]; // 0x30d
	CUtlSymbolLarge m_iGlobalname; // 0x310
	int32_t m_iSentToClients; // 0x318
	float m_flSpeed; // 0x31c
	CUtlString m_sUniqueHammerID; // 0x320
	// MNetworkEnable
	uint32_t m_spawnflags; // 0x328
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	GameTick_t m_nNextThinkTick; // 0x32c
	int32_t m_nSimulationTick; // 0x330
	uint8_t __pad0334[0x4]; // 0x334
	CEntityIOOutput m_OnKilled; // 0x338
	// MNetworkEnable
	// MNetworkPriority "32"
	// MNetworkUserGroup "Player"
	uint32_t m_fFlags; // 0x360
	Vector m_vecAbsVelocity; // 0x364
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	// MNetworkPriority "32"
	CNetworkVelocityVector m_vecVelocity; // 0x370
	uint8_t __pad0398[0x8]; // 0x398
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	Vector m_vecBaseVelocity; // 0x3a0
	int32_t m_nPushEnumCount; // 0x3ac
	CCollisionProperty* m_pCollision; // 0x3b0
	// MNetworkEnable
	CHandle< CBaseEntity > m_hEffectEntity; // 0x3b8
	// MNetworkEnable
	// MNetworkPriority "32"
	CHandle< CBaseEntity > m_hOwnerEntity; // 0x3bc
	// MNetworkEnable
	// MNetworkChangeCallback "OnEffectsChanged"
	uint32_t m_fEffects; // 0x3c0
	// MNetworkEnable
	// MNetworkPriority "32"
	// MNetworkUserGroup "Player"
	CHandle< CBaseEntity > m_hGroundEntity; // 0x3c4
	// MNetworkEnable
	// MNetworkBitCount "8"
	// MNetworkMinValue "0.000000"
	// MNetworkMaxValue "4.000000"
	// MNetworkEncodeFlags
	// MNetworkUserGroup "LocalPlayerExclusive"
	float m_flFriction; // 0x3c8
	// MNetworkEnable
	// MNetworkEncoder
	float m_flElasticity; // 0x3cc
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	float m_flGravityScale; // 0x3d0
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	float m_flTimeScale; // 0x3d4
	// MNetworkEnable
	// MNetworkUserGroup "Water"
	// MNetworkBitCount "8"
	// MNetworkMinValue "0.000000"
	// MNetworkMaxValue "1.000000"
	// MNetworkEncodeFlags
	float m_flWaterLevel; // 0x3d8
	// MNetworkEnable
	bool m_bSimulatedEveryTick; // 0x3dc
	// MNetworkEnable
	bool m_bAnimatedEveryTick; // 0x3dd
	bool m_bDisableLowViolence; // 0x3de
	uint8_t m_nWaterType; // 0x3df
	int32_t m_iEFlags; // 0x3e0
	uint8_t __pad03e4[0x4]; // 0x3e4
	CEntityIOOutput m_OnUser1; // 0x3e8
	CEntityIOOutput m_OnUser2; // 0x410
	CEntityIOOutput m_OnUser3; // 0x438
	CEntityIOOutput m_OnUser4; // 0x460
	int32_t m_iInitialTeamNum; // 0x488
	// MNetworkEnable
	GameTime_t m_flNavIgnoreUntilTime; // 0x48c
	QAngle m_vecAngVelocity; // 0x490
	bool m_bNetworkQuantizeOriginAndAngles; // 0x49c
	bool m_bLagCompensate; // 0x49d
	uint8_t __pad049e[0x2]; // 0x49e
	float m_flOverriddenFriction; // 0x4a0
	CHandle< CBaseEntity > m_pBlocker; // 0x4a4
	float m_flLocalTime; // 0x4a8
	float m_flVPhysicsUpdateLocalTime; // 0x4ac
};

// Size: 0x690
class CBasePlayerController : public CBaseEntity2
{
public:
	uint8_t __pad04b0[0x8]; // 0x4b0
	uint64_t m_nInButtonsWhichAreToggles; // 0x4b8
	// MNetworkEnable
	// MNetworkPriority "1"
	// MNetworkUserGroup "LocalPlayerExclusive"
	uint32_t m_nTickBase; // 0x4c0
	uint8_t __pad04c4[0x2c]; // 0x4c4
	// MNetworkEnable
	// MNetworkChangeCallback "OnPawnChanged"
	CHandle< CBasePlayerPawn > m_hPawn; // 0x4f0
	CSplitScreenSlot m_nSplitScreenSlot; // 0x4f4
	CHandle< CBasePlayerController > m_hSplitOwner; // 0x4f8
	uint8_t __pad04fc[0x4]; // 0x4fc
	CUtlVector< CHandle< CBasePlayerController > > m_hSplitScreenPlayers; // 0x500
	bool m_bIsHLTV; // 0x518
	uint8_t __pad0519[0x3]; // 0x519
	// MNetworkEnable
	// MNetworkChangeCallback "OnConnectionStateChanged"
	PlayerConnectedState m_iConnected; // 0x51c
	// MNetworkEnable
	// MNetworkChangeCallback "OnPlayerControllerNameChanged"
	char m_iszPlayerName[128]; // 0x520
	CUtlString m_szNetworkIDString; // 0x5a0
	float m_fLerpTime; // 0x5a8
	bool m_bLagCompensation; // 0x5ac
	bool m_bPredict; // 0x5ad
	bool m_bAutoKickDisabled; // 0x5ae
	bool m_bIsLowViolence; // 0x5af
	bool m_bGamePaused; // 0x5b0
	uint8_t __pad05b1[0x77]; // 0x5b1
	int32_t m_nHighestCommandNumberReceived; // 0x628
	uint8_t __pad062c[0x4]; // 0x62c
	int64_t m_nUsecTimestampLastUserCmdReceived; // 0x630
	uint8_t __pad0638[0x10]; // 0x638
	ChatIgnoreType_t m_iIgnoreGlobalChat; // 0x648
	float m_flLastPlayerTalkTime; // 0x64c
	float m_flLastEntitySteadyState; // 0x650
	int32_t m_nAvailableEntitySteadyState; // 0x654
	bool m_bHasAnySteadyStateEnts; // 0x658
	uint8_t __pad0659[0xf]; // 0x659
	// MNetworkEnable
	// MNetworkEncoder
	// MNetworkChangeCallback "OnSteamIDChanged"
	uint64_t m_steamID; // 0x668
	// MNetworkEnable
	uint32_t m_iDesiredFOV; // 0x670
	uint8_t __pad0674[0x1c]; // 0x674
};
static_assert(sizeof(CBasePlayerController) == 0x690, "Class didn't match expected size");

// Size: 0xb8
class CRenderComponent : public CEntityComponent
{
public:
	uint8_t __pad0008[0x8]; // 0x8
	// MNetworkDisable
	// MNetworkChangeAccessorFieldPathIndex
	CNetworkVarChainer __m_pChainEntity; // 0x10
	uint8_t __pad0038[0x18]; // 0x38
	bool m_bIsRenderingWithViewModels; // 0x50
	uint8_t __pad0051[0x3]; // 0x51
	uint32_t m_nSplitscreenFlags; // 0x54
	uint8_t __pad0058[0x8]; // 0x58
	bool m_bEnableRendering; // 0x60
	uint8_t __pad0061[0x4f]; // 0x61
	bool m_bInterpolationReadyToDraw; // 0xb0
	uint8_t __pad00b1[0x7]; // 0xb1
};
static_assert(sizeof(CRenderComponent) == 0xb8, "Class didn't match expected size");

// Size: 0x28
class CHitboxComponent : public CEntityComponent
{
public:
	uint8_t __pad0008[0x1c]; // 0x8
	// MNetworkEnable
	uint32_t m_bvDisabledHitGroups[1]; // 0x24
};
static_assert(sizeof(CHitboxComponent) == 0x28, "Class didn't match expected size");

// Size: 0x58
class CGlowProperty
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	Vector m_fGlowColor; // 0x8
	uint8_t __pad0014[0x1c]; // 0x14
	// MNetworkEnable
	// MNetworkChangeCallback "OnGlowTypeChanged"
	int32_t m_iGlowType; // 0x30
	// MNetworkEnable
	int32_t m_iGlowTeam; // 0x34
	// MNetworkEnable
	int32_t m_nGlowRange; // 0x38
	// MNetworkEnable
	int32_t m_nGlowRangeMin; // 0x3c
	// MNetworkEnable
	// MNetworkChangeCallback "OnGlowColorChanged"
	Color m_glowColorOverride; // 0x40
	// MNetworkEnable
	bool m_bFlashing; // 0x44
	uint8_t __pad0045[0x3]; // 0x45
	// MNetworkEnable
	float m_flGlowTime; // 0x48
	// MNetworkEnable
	float m_flGlowStartTime; // 0x4c
	// MNetworkEnable
	bool m_bEligibleForScreenHighlight; // 0x50
	bool m_bGlowing; // 0x51
	uint8_t __pad0052[0x6]; // 0x52
};
static_assert(sizeof(CGlowProperty) == 0x58, "Class didn't match expected size");

// Size: 0x48
struct EntityRenderAttribute_t
{
public:
	uint8_t __pad0000[0x30]; // 0x0
	// MNetworkEnable
	CUtlStringToken m_ID; // 0x30
	// MNetworkEnable
	Vector4D m_Values; // 0x34
	uint8_t __pad0044[0x4]; // 0x44
};
static_assert(sizeof(EntityRenderAttribute_t) == 0x48, "Class didn't match expected size");

// Size: 0x700
class CBaseModelEntity : public CBaseEntity2
{
public:
	// MNetworkEnable
	// MNetworkUserGroup "CRenderComponent"
	// MNetworkAlias "CRenderComponent"
	// MNetworkTypeAlias "CRenderComponent"
	CRenderComponent* m_CRenderComponent; // 0x4b0
	// MNetworkEnable
	// MNetworkUserGroup "CHitboxComponent"
	// MNetworkAlias "CHitboxComponent"
	// MNetworkTypeAlias "CHitboxComponent"
	CHitboxComponent m_CHitboxComponent; // 0x4b8
	GameTime_t m_flDissolveStartTime; // 0x4e0
	uint8_t __pad04e4[0x4]; // 0x4e4
	CEntityIOOutput m_OnIgnite; // 0x4e8
	// MNetworkEnable
	RenderMode_t m_nRenderMode; // 0x510
	// MNetworkEnable
	RenderFx_t m_nRenderFX; // 0x511
	bool m_bAllowFadeInView; // 0x512
	// MNetworkEnable
	// MNetworkChangeCallback "OnColorChanged"
	Color m_clrRender; // 0x513
	uint8_t __pad0517[0x1]; // 0x517
	// MNetworkEnable
	// MNetworkChangeCallback "OnRenderAttributesChanged"
	CUtlVectorEmbeddedNetworkVar< EntityRenderAttribute_t > m_vecRenderAttributes; // 0x518
	// MNetworkEnable
	// MNetworkChangeCallback "OnLightGroupChanged"
	CUtlStringToken m_LightGroup; // 0x568
	// MNetworkEnable
	bool m_bRenderToCubemaps; // 0x56c
	uint8_t __pad056d[0x3]; // 0x56d
	// MNetworkEnable
	CCollisionProperty m_Collision; // 0x570
	// MNetworkEnable
	CGlowProperty m_Glow; // 0x620
	// MNetworkEnable
	float m_flGlowBackfaceMult; // 0x678
	// MNetworkEnable
	float m_fadeMinDist; // 0x67c
	// MNetworkEnable
	float m_fadeMaxDist; // 0x680
	// MNetworkEnable
	float m_flFadeScale; // 0x684
	// MNetworkEnable
	float m_flShadowStrength; // 0x688
	// MNetworkEnable
	uint8_t m_nObjectCulling; // 0x68c
	uint8_t __pad068d[0x3]; // 0x68d
	// MNetworkEnable
	int32_t m_nAddDecal; // 0x690
	// MNetworkEnable
	Vector m_vDecalPosition; // 0x694
	// MNetworkEnable
	Vector m_vDecalForwardAxis; // 0x6a0
	// MNetworkEnable
	float m_flDecalHealBloodRate; // 0x6ac
	// MNetworkEnable
	float m_flDecalHealHeightRate; // 0x6b0
	uint8_t __pad06b4[0x4]; // 0x6b4
	// MNetworkEnable
	CNetworkUtlVectorBase< CHandle< CBaseModelEntity > > m_ConfigEntitiesToPropagateMaterialDecalsTo; // 0x6b8
	// MNetworkEnable
	// MNetworkPriority "32"
	// MNetworkUserGroup "Player"
	CNetworkViewOffsetVector m_vecViewOffset; // 0x6d0
	uint8_t __pad06f8[0x8]; // 0x6f8
};
static_assert(sizeof(CBaseModelEntity) == 0x700, "Class didn't match expected size");

// Size: 0x50
struct PhysicsRagdollPose_t
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkDisable
	// MNetworkChangeAccessorFieldPathIndex
	CNetworkVarChainer __m_pChainEntity; // 0x8
	// MNetworkEnable
	// MNetworkChangeCallback "OnTransformChanged"
	CNetworkUtlVectorBase< CTransform > m_Transforms; // 0x30
	// MNetworkEnable
	CHandle< CBaseEntity > m_hOwner; // 0x48
	uint8_t __pad0052[4];
};
static_assert(sizeof(PhysicsRagdollPose_t) == 0x50, "Class didn't match expected size");

// Size: 0x890
class CBaseAnimGraph : public CBaseModelEntity
{
public:
	// MNetworkEnable
	bool m_bInitiallyPopulateInterpHistory; // 0x700	
	// MNetworkEnable
	bool m_bShouldAnimateDuringGameplayPause; // 0x701	
	uint8_t __pad0702[0x6]; // 0x702
	void* m_pChoreoServices; // 0x708	
	bool m_bAnimGraphUpdateEnabled; // 0x710	
	uint8_t __pad0711[0x3]; // 0x711
	float m_flMaxSlopeDistance; // 0x714	
	Vector m_vLastSlopeCheckPos; // 0x718	
	bool m_bAnimGraphDirty; // 0x724	
	uint8_t __pad0725[0x3]; // 0x725
	Vector m_vecForce; // 0x728	
	int32_t m_nForceBone; // 0x734	
	uint8_t __pad0738[0x10]; // 0x738
	// MNetworkEnable
	PhysicsRagdollPose_t* m_pRagdollPose; // 0x748	
	// MNetworkEnable
	// MNetworkChangeCallback "OnClientRagdollChanged"
	bool m_bClientRagdoll; // 0x750	
	uint8_t __pad0751[0x138]; // 0x751
};
static_assert(sizeof(CBaseAnimGraph) == 0x890, "Class didn't match expected size");

// Size: 0x920
class CBaseFlex2 : public CBaseAnimGraph
{
public:
	// MNetworkEnable
	// MNetworkBitCount "12"
	// MNetworkMinValue "0.000000"
	// MNetworkMaxValue "1.000000"
	// MNetworkEncodeFlags
	CNetworkUtlVectorBase< float32 > m_flexWeight; // 0x890	
	// MNetworkEnable
	// MNetworkEncoder
	Vector m_vLookTargetPosition; // 0x8a8	
	// MNetworkEnable
	bool m_blinktoggle; // 0x8b4	
	uint8_t __pad08b5[0x53]; // 0x8b5
public:
	GameTime_t m_flAllowResponsesEndTime; // 0x908	
	GameTime_t m_flLastFlexAnimationTime; // 0x90c	
	uint32_t m_nNextSceneEventId; // 0x910	
	bool m_bUpdateLayerPriorities; // 0x914	
	uint8_t __pad0919[0x8]; // 0x919
};
static_assert(sizeof(CBaseFlex2) == 0x920, "Class didn't match expected size");

// Size: 0x8
struct Relationship_t
{
public:
	Disposition_t disposition; // 0x0
	int32_t priority; // 0x4
};
static_assert(sizeof(Relationship_t) == 0x8, "Class didn't match expected size");

// Size: 0x10
struct RelationshipOverride_t : public Relationship_t
{
public:
	CHandle< CBaseEntity > entity; // 0x8
	Class_T classType; // 0xc
};
static_assert(sizeof(RelationshipOverride_t) == 0x10, "Class didn't match expected size");

// Size: 0x9d0
class CBaseCombatCharacter2 : public CBaseFlex2
{
public:
	bool m_bForceServerRagdoll; // 0x920	
	uint8_t __pad0921[0x7]; // 0x921
public:
	// MNetworkEnable
	CNetworkUtlVectorBase< CHandle< CBaseFlex2 > > m_hMyWearables; // 0x928	
	// MNetworkEnable
	float m_flFieldOfView; // 0x940	
	float m_impactEnergyScale; // 0x944	
	uint32_t m_LastHitGroup; // 0x948	
	bool m_bApplyStressDamage; // 0x94c	
	uint8_t __pad094d[0x3]; // 0x94d
public:
	int32_t m_bloodColor; // 0x950	
	uint8_t __pad0954[0x5c]; // 0x954
public:
	int32_t m_navMeshID; // 0x9b0	
	int32_t m_iDamageCount; // 0x9b4	
	CUtlVector< RelationshipOverride_t >* m_pVecRelationships; // 0x9b8	
	CUtlSymbolLarge m_strRelationships; // 0x9c0	
	Hull_t m_eHull; // 0x9c8	
	uint32_t m_nNavHullIdx; // 0x9cc	
};

static_assert(sizeof(CBaseCombatCharacter2) == 0x9d0, "Class didn't match expected size");

// Size: 0x780
class CBaseToggle : public CBaseModelEntity
{
public:
	TOGGLE_STATE m_toggle_state; // 0x700
	float m_flMoveDistance; // 0x704
	float m_flWait; // 0x708
	float m_flLip; // 0x70c
	bool m_bAlwaysFireBlockedOutputs; // 0x710
	uint8_t __pad0711[0x3]; // 0x711
	Vector m_vecPosition1; // 0x714
	Vector m_vecPosition2; // 0x720
	QAngle m_vecMoveAng; // 0x72c
	QAngle m_vecAngle1; // 0x738
	QAngle m_vecAngle2; // 0x744
	float m_flHeight; // 0x750
	CHandle< CBaseEntity > m_hActivator; // 0x754
	Vector m_vecFinalDest; // 0x758
	QAngle m_vecFinalAngle; // 0x764
	int32_t m_movementType; // 0x770
	uint8_t __pad0774[0x4]; // 0x774
	CUtlSymbolLarge m_sMaster; // 0x778
};
static_assert(sizeof(CBaseToggle) == 0x780, "Class didn't match expected size");

// Size: 0xb0
class CPlayer_WeaponServices : public CPlayerPawnComponent
{
public:
	bool m_bAllowSwitchToNoWeapon; // 0x40
	uint8_t __pad0041[0x7]; // 0x41
	// MNetworkEnable
	CNetworkUtlVectorBase< CHandle< CBaseFlex2 > > m_hMyWeapons; // 0x48 CNetworkUtlVectorBase< CHandle< CBasePlayerWeapon > >
	// MNetworkEnable
	CHandle< CBaseFlex2 > m_hActiveWeapon; // 0x60 CHandle< CBasePlayerWeapon >
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	CHandle< CBaseFlex2 > m_hLastWeapon; // 0x64 CHandle< CBasePlayerWeapon >
	// MNetworkEnable
	uint16_t m_iAmmo[32]; // 0x68
	bool m_bPreventWeaponPickup; // 0xa8
	uint8_t __pad00a9[0x7];
};
static_assert(sizeof(CPlayer_WeaponServices) == 0xb0, "Class didn't match expected size");

// Size: 0x50
class CPlayer_ObserverServices : public CPlayerPawnComponent
{
public:
	// MNetworkEnable
	// MNetworkChangeCallback "OnObserverModeChanged"
	uint8_t m_iObserverMode; // 0x40
	uint8_t __pad0041[0x3]; // 0x41
	// MNetworkEnable
	// MNetworkChangeCallback "OnObserverTargetChanged"
	CHandle< CBaseEntity > m_hObserverTarget; // 0x44
	ObserverMode_t m_iObserverLastMode; // 0x48
	bool m_bForcedObserverMode; // 0x4c
};
static_assert(sizeof(CPlayer_ObserverServices) == 0x50, "Class didn't match expected size");

// Size: 0x40
class CPlayer_UseServices : public CPlayerPawnComponent
{
public:
	// No members available
};
static_assert(sizeof(CPlayer_UseServices) == 0x40, "Class didn't match expected size");

// Size: 0x68
struct fogparams_t
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkEnable
	// MNetworkEncoder
	Vector dirPrimary; // 0x8
	// MNetworkEnable
	Color colorPrimary; // 0x14
	// MNetworkEnable
	Color colorSecondary; // 0x18
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	Color colorPrimaryLerpTo; // 0x1c
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	Color colorSecondaryLerpTo; // 0x20
	// MNetworkEnable
	float start; // 0x24
	// MNetworkEnable
	float end; // 0x28
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float farz; // 0x2c
	// MNetworkEnable
	float maxdensity; // 0x30
	// MNetworkEnable
	float exponent; // 0x34
	// MNetworkEnable
	float HDRColorScale; // 0x38
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float skyboxFogFactor; // 0x3c
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float skyboxFogFactorLerpTo; // 0x40
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float startLerpTo; // 0x44
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float endLerpTo; // 0x48
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float maxdensityLerpTo; // 0x4c
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	GameTime_t lerptime; // 0x50
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float duration; // 0x54
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float blendtobackground; // 0x58
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float scattering; // 0x5c
	// MNetworkEnable
	// MNetworkUserGroup "FogController"
	float locallightscale; // 0x60
	// MNetworkEnable
	bool enable; // 0x64
	// MNetworkEnable
	bool blend; // 0x65
	// MNetworkEnable
	bool m_bNoReflectionFog; // 0x66
	bool m_bPadding; // 0x67
};
static_assert(sizeof(fogparams_t) == 0x68, "Class didn't match expected size");

// Size: 0x520
class CFogController : public CBaseEntity2
{
public:
	// MNetworkEnable
	fogparams_t m_fog; // 0x4b0
	bool m_bUseAngles; // 0x518
	uint8_t __pad0519[0x3]; // 0x519
	int32_t m_iChangedVariables; // 0x51c
};
static_assert(sizeof(CFogController) == 0x520, "Class didn't match expected size");

// Size: 0x40
struct fogplayerparams_t
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkEnable
	// MNetworkUserGroup "PlayerFogController"
	CHandle< CFogController > m_hCtrl; // 0x8
	float m_flTransitionTime; // 0xc
	Color m_OldColor; // 0x10
	float m_flOldStart; // 0x14
	float m_flOldEnd; // 0x18
	float m_flOldMaxDensity; // 0x1c
	float m_flOldHDRColorScale; // 0x20
	float m_flOldFarZ; // 0x24
	Color m_NewColor; // 0x28
	float m_flNewStart; // 0x2c
	float m_flNewEnd; // 0x30
	float m_flNewMaxDensity; // 0x34
	float m_flNewHDRColorScale; // 0x38
	float m_flNewFarZ; // 0x3c
};
static_assert(sizeof(fogplayerparams_t) == 0x40, "Class didn't match expected size");

// Size: 0x4d8
class CTonemapController2 : public CBaseEntity2
{
public:
	// MNetworkEnable
	float m_flAutoExposureMin; // 0x4b0
	// MNetworkEnable
	float m_flAutoExposureMax; // 0x4b4
	// MNetworkEnable
	float m_flTonemapPercentTarget; // 0x4b8
	// MNetworkEnable
	float m_flTonemapPercentBrightPixels; // 0x4bc
	// MNetworkEnable
	float m_flTonemapMinAvgLum; // 0x4c0
	// MNetworkEnable
	float m_flExposureAdaptationSpeedUp; // 0x4c4
	// MNetworkEnable
	float m_flExposureAdaptationSpeedDown; // 0x4c8
	// MNetworkEnable
	float m_flTonemapEVSmoothingRange; // 0x4cc
	uint8_t __pad04d0[0x8]; // 0x4d0
};
static_assert(sizeof(CTonemapController2) == 0x4d8, "Class didn't match expected size");

// Size: 0x78
struct audioparams_t
{
	uint8_t __pad0000[0x8]; // 0x0
public:
	// MNetworkEnable
	// MNetworkEncoder
	Vector localSound[8]; // 0x8
	// MNetworkEnable
	int32_t soundscapeIndex; // 0x68
	// MNetworkEnable
	uint8_t localBits; // 0x6c
	uint8_t __pad006d[0x3]; // 0x6d
public:
	// MNetworkEnable
	int32_t soundscapeEntityListIndex; // 0x70
	// MNetworkEnable
	uint32_t soundEventHash; // 0x74
};
static_assert(sizeof(audioparams_t) == 0x78, "Class didn't match expected size");

// Size: 0x48
struct ViewAngleServerChange_t
{
public:
	uint8_t __pad0000[0x30]; // 0x0
	// MNetworkEnable
	FixAngleSet_t nType; // 0x30
	uint8_t __pad0031[0x3]; // 0x31
	// MNetworkEnable
	// MNetworkEncoder
	QAngle qAngle; // 0x34
	// MNetworkEnable
	uint32_t nIndex; // 0x40
	uint8_t __pad0044[0x4]; // 0x44
};
static_assert(sizeof(ViewAngleServerChange_t) == 0x48, "Class didn't match expected size");

// Size: 0xb50
class CBasePlayerPawn : public CBaseCombatCharacter2
{
public:
	// MNetworkEnable
	CPlayer_WeaponServices* m_pWeaponServices; // 0x9c8	
	// MNetworkEnable
	void* m_pItemServices; // 0x9d0	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	void* m_pAutoaimServices; // 0x9d8	
	// MNetworkEnable
	CPlayer_ObserverServices* m_pObserverServices; // 0x9e0	
	// MNetworkEnable
	void* m_pWaterServices; // 0x9e8	
	// MNetworkEnable
	CPlayer_UseServices* m_pUseServices; // 0x9f0	
	// MNetworkEnable
	void* m_pFlashlightServices; // 0x9f8	
	// MNetworkEnable
	void* m_pCameraServices; // 0xa00	
	// MNetworkEnable
	CPlayer_MovementServices* m_pMovementServices; // 0xa08	
	uint8_t __pad0a10[0x8]; // 0xa10
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	CUtlVectorEmbeddedNetworkVar< ViewAngleServerChange_t > m_ServerViewAngleChanges; // 0xa18	
	uint32_t m_nHighestGeneratedServerViewAngleChangeIndex; // 0xa68	
	QAngle v_angle; // 0xa6c	
	QAngle v_anglePrevious; // 0xa78	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	uint32_t m_iHideHUD; // 0xa84	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	uint8_t m_skybox3d[0x90]; // 0xa88	
	GameTime_t m_fTimeLastHurt; // 0xb18	
	// MNetworkEnable
	GameTime_t m_flDeathTime; // 0xb1c	
	GameTime_t m_fNextSuicideTime; // 0xb20	
	bool m_fInitHUD; // 0xb24	
	uint8_t __pad0b25[0x3]; // 0xb25
	void* m_pExpresser; // 0xb28	
	// MNetworkEnable
	CHandle< CBasePlayerController > m_hController; // 0xb30	
	uint8_t __pad0b34[0x4]; // 0xb34
	float m_fHltvReplayDelay; // 0xb38	
	float m_fHltvReplayEnd; // 0xb3c	
	CEntityIndex m_iHltvReplayEntity; // 0xb40	
};
static_assert(sizeof(CBasePlayerPawn) == 0xb50, "Class didn't match expected size");

// Size: 0x50
class CTouchExpansionComponent : public CEntityComponent
{
public:
	uint8_t __pad0008[0x48]; // 0x8
};
static_assert(sizeof(CTouchExpansionComponent) == 0x50, "Class didn't match expected size");

// Size: 0x38
struct WeaponPurchaseCount_t
{
public:
	uint8_t __pad0000[0x30]; // 0x0
	// MNetworkEnable
	uint16_t m_nItemDefIndex; // 0x30
	// MNetworkEnable
	uint16_t m_nCount; // 0x32
	uint8_t __pad0034[0x4]; // 0x34
};
static_assert(sizeof(WeaponPurchaseCount_t) == 0x38, "Class didn't match expected size");

// Size: 0x58
struct WeaponPurchaseTracker_t
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkEnable
	CUtlVectorEmbeddedNetworkVar< WeaponPurchaseCount_t > m_weaponPurchases; // 0x8
};
static_assert(sizeof(WeaponPurchaseTracker_t) == 0x58, "Class didn't match expected size");

// Size: 0x328
class CCSPlayer_ActionTrackingServices : public CPlayerPawnComponent
{
public:
	uint8_t __pad0040[0x1f0]; // 0x40
	void* m_lastWeaponBeforeC4AutoSwitch; // 0x230 CBasePlayerWeapon *
	uint8_t __pad0238[0x30]; // 0x238
	// MNetworkEnable
	bool m_bIsRescuing; // 0x268
	uint8_t __pad0269[0x7]; // 0x269
	// MNetworkEnable
	WeaponPurchaseTracker_t m_weaponPurchasesThisMatch; // 0x270
	// MNetworkEnable
	WeaponPurchaseTracker_t m_weaponPurchasesThisRound; // 0x2c8
	uint8_t __pad0320[0x8]; // 0x320
};
static_assert(sizeof(CCSPlayer_ActionTrackingServices) == 0x328, "Class didn't match expected size");

// Size: 0x4
class HSequence
{
public:
	int32_t m_Value; // 0x0	
};

// Size: 0x8d8
class CBaseViewModel : public CBaseAnimGraph
{
	uint8_t __pad0890[0x8]; // 0x890
public:
	Vector m_vecLastFacing; // 0x898	
	// MNetworkEnable
	uint32_t m_nViewModelIndex; // 0x8a4	
	// MNetworkEnable
	uint32_t m_nAnimationParity; // 0x8a8	
	// MNetworkEnable
	float m_flAnimationStartTime; // 0x8ac	
	// MNetworkEnable
	CHandle< CBaseEntity > m_hWeapon; // 0x8b0	
	uint8_t __pad08b4[0x4]; // 0x8b4
public:
	CUtlSymbolLarge m_sVMName; // 0x8b8	
	CUtlSymbolLarge m_sAnimationPrefix; // 0x8c0	
	HSequence m_hOldLayerSequence; // 0x8c8	
	int32_t m_oldLayer; // 0x8cc	
	float m_oldLayerStartTime; // 0x8d0	
	// MNetworkEnable
	CHandle< CBaseEntity > m_hControlPanel; // 0x8d4	
};
static_assert(sizeof(CBaseViewModel) == 0x8d8, "Class didn't match expected size");

// Size: 0x50
class CCSPlayer_ViewModelServices : public CPlayerPawnComponent
{
public:
	// MNetworkEnable
	CHandle< CBaseViewModel > m_hViewModel[3]; // 0x40
	uint8_t __pad004c[0x4]; // 0x44
};
static_assert(sizeof(CCSPlayer_ViewModelServices) == 0x50, "Class didn't match expected size");

// Size: 0x18
struct EntitySpottedState_t
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkEnable
	// MNetworkChangeCallback "OnIsSpottedChanged"
	bool m_bSpotted; // 0x8
	uint8_t __pad0009[0x3]; // 0x9
	// MNetworkEnable
	// MNetworkChangeCallback "OnIsSpottedChanged"
	uint32_t m_bSpottedByMask[2]; // 0xc
	uint8_t __pad0014[0x4]; // 0x14
};
static_assert(sizeof(EntitySpottedState_t) == 0x18, "Class didn't match expected size");

// Size: 0x18
class CountdownTimer
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	// MNetworkEnable
	float m_duration; // 0x8
	// MNetworkEnable
	GameTime_t m_timestamp; // 0xc
	// MNetworkEnable
	float m_timescale; // 0x10
	// MNetworkEnable
	WorldGroupId_t m_nWorldGroupId; // 0x14
};
static_assert(sizeof(CountdownTimer) == 0x18, "Class didn't match expected size");

class CCSPlayerController : public CBasePlayerController
{
	uint8_t __pad0690[0x10]; // 0x690
public:
	// MNetworkEnable
	void* m_pInGameMoneyServices; // 0x6a0	
	// MNetworkEnable
	void* m_pInventoryServices; // 0x6a8	
	// MNetworkEnable
	void* m_pActionTrackingServices; // 0x6b0	
	// MNetworkEnable
	void* m_pDamageServices; // 0x6b8	
	// MNetworkEnable
	uint32_t m_iPing; // 0x6c0	
	// MNetworkEnable
	bool m_bHasCommunicationAbuseMute; // 0x6c4	
	uint8_t __pad06c5[0x3]; // 0x6c5
public:
	// MNetworkEnable
	CUtlSymbolLarge m_szCrosshairCodes; // 0x6c8	
	// MNetworkEnable
	uint8_t m_iPendingTeamNum; // 0x6d0	
	uint8_t __pad06d1[0x3]; // 0x6d1
public:
	// MNetworkEnable
	GameTime_t m_flForceTeamTime; // 0x6d4	
	// MNetworkEnable
	int32_t m_iCompTeammateColor; // 0x6d8	
	// MNetworkEnable
	bool m_bEverPlayedOnTeam; // 0x6dc	
	bool m_bAttemptedToGetColor; // 0x6dd	
	uint8_t __pad06de[0x2]; // 0x6de
public:
	int32_t m_iTeammatePreferredColor; // 0x6e0	
	bool m_bTeamChanged; // 0x6e4	
	bool m_bInSwitchTeam; // 0x6e5	
	bool m_bHasSeenJoinGame; // 0x6e6	
	bool m_bJustBecameSpectator; // 0x6e7	
	bool m_bSwitchTeamsOnNextRoundReset; // 0x6e8	
	bool m_bRemoveAllItemsOnNextRoundReset; // 0x6e9	
	uint8_t __pad06ea[0x6]; // 0x6ea
public:
	// MNetworkEnable
	CUtlSymbolLarge m_szClan; // 0x6f0	
	char m_szClanName[32]; // 0x6f8	
	// MNetworkEnable
	int32_t m_iCoachingTeam; // 0x718	
	uint8_t __pad071c[0x4]; // 0x71c
public:
	// MNetworkEnable
	uint64_t m_nPlayerDominated; // 0x720	
	// MNetworkEnable
	uint64_t m_nPlayerDominatingMe; // 0x728	
	// MNetworkEnable
	int32_t m_iCompetitiveRanking; // 0x730	
	// MNetworkEnable
	int32_t m_iCompetitiveWins; // 0x734	
	// MNetworkEnable
	int8_t m_iCompetitiveRankType; // 0x738	
	uint8_t __pad0739[0x3]; // 0x739
public:
	// MNetworkEnable
	int32_t m_iCompetitiveRankingPredicted_Win; // 0x73c	
	// MNetworkEnable
	int32_t m_iCompetitiveRankingPredicted_Loss; // 0x740	
	// MNetworkEnable
	int32_t m_iCompetitiveRankingPredicted_Tie; // 0x744	
	// MNetworkEnable
	int32_t m_nEndMatchNextMapVote; // 0x748	
	// MNetworkEnable
	uint16_t m_unActiveQuestId; // 0x74c	
	uint8_t __pad074e[0x2]; // 0x74e
public:
	// MNetworkEnable
	uint32_t m_nQuestProgressReason; // 0x750	
	// MNetworkEnable
	uint32_t m_unPlayerTvControlFlags; // 0x754	
	uint8_t __pad0758[0x68]; // 0x758
public:
	int32_t m_iDraftIndex; // 0x7c0	
	uint32_t m_msQueuedModeDisconnectionTimestamp; // 0x7c4	
	uint32_t m_uiAbandonRecordedReason; // 0x7c8	
	bool m_bEverFullyConnected; // 0x7cc	
	bool m_bAbandonAllowsSurrender; // 0x7cd	
	bool m_bAbandonOffersInstantSurrender; // 0x7ce	
	bool m_bDisconnection1MinWarningPrinted; // 0x7cf	
	bool m_bScoreReported; // 0x7d0	
	uint8_t __pad07d1[0x3]; // 0x7d1
public:
	// MNetworkEnable
	int32_t m_nDisconnectionTick; // 0x7d4	
	uint8_t __pad07d8[0x8]; // 0x7d8
public:
	// MNetworkEnable
	bool m_bControllingBot; // 0x7e0	
	// MNetworkEnable
	bool m_bHasControlledBotThisRound; // 0x7e1	
	bool m_bHasBeenControlledByPlayerThisRound; // 0x7e2	
	uint8_t __pad07e3[0x1]; // 0x7e3
public:
	int32_t m_nBotsControlledThisRound; // 0x7e4	
	// MNetworkEnable
	bool m_bCanControlObservedBot; // 0x7e8	
	uint8_t __pad07e9[0x3]; // 0x7e9
public:
	// MNetworkEnable
	CHandle< CCSPlayerPawn > m_hPlayerPawn; // 0x7ec	
	// MNetworkEnable
	int m_hObserverPawn; // 0x7f0	
	int32_t m_DesiredObserverMode; // 0x7f4	
	CEntityHandle m_hDesiredObserverTarget; // 0x7f8	
	// MNetworkEnable
	bool m_bPawnIsAlive; // 0x7fc	
	uint8_t __pad07fd[0x3]; // 0x7fd
public:
	// MNetworkEnable
	uint32_t m_iPawnHealth; // 0x800	
	// MNetworkEnable
	int32_t m_iPawnArmor; // 0x804	
	// MNetworkEnable
	bool m_bPawnHasDefuser; // 0x808	
	// MNetworkEnable
	bool m_bPawnHasHelmet; // 0x809	
	// MNetworkEnable
	uint16_t m_nPawnCharacterDefIndex; // 0x80a	
	// MNetworkEnable
	int32_t m_iPawnLifetimeStart; // 0x80c	
	// MNetworkEnable
	int32_t m_iPawnLifetimeEnd; // 0x810	
	// MNetworkEnable
	int32_t m_iPawnGunGameLevel; // 0x814	
	// MNetworkEnable
	int32_t m_iPawnBotDifficulty; // 0x818	
	// MNetworkEnable
	CHandle< CCSPlayerController > m_hOriginalControllerOfCurrentPawn; // 0x81c	
	// MNetworkEnable
	int32_t m_iScore; // 0x820	
	int32_t m_iRoundScore; // 0x824	
	// MNetworkEnable
	CNetworkUtlVectorBase< EKillTypes_t > m_vecKills; // 0x828	
	// MNetworkEnable
	int32_t m_iMVPs; // 0x840	
	int32_t m_nUpdateCounter; // 0x844	
	uint8_t __pad0848[0xf0a0]; // 0x848
public:
	IntervalTimer m_lastHeldVoteTimer; // 0xf8e8	
	uint8_t __padf8f8[0x8]; // 0xf8f8
public:
	bool m_bShowHints; // 0xf900	
	uint8_t __padf901[0x3]; // 0xf901
public:
	int32_t m_iNextTimeCheck; // 0xf904	
};

// Size: 0x1568
class CCSPlayerPawnBase : public CBasePlayerPawn
{
	uint8_t __pad0b50[0x10]; // 0xb50
public:
	// MNetworkEnable
	// MNetworkUserGroup "CTouchExpansionComponent"
	// MNetworkAlias "CTouchExpansionComponent"
	// MNetworkTypeAlias "CTouchExpansionComponent"
	CTouchExpansionComponent m_CTouchExpansionComponent; // 0xb60	
	// MNetworkEnable
	void* m_pPingServices; // 0xbb0	
	// MNetworkEnable
	void* m_pViewModelServices; // 0xbb8	
	uint32_t m_iDisplayHistoryBits; // 0xbc0	
	float m_flLastAttackedTeammate; // 0xbc4	
	// MNetworkEnable
	CHandle< CCSPlayerController > m_hOriginalController; // 0xbc8	
	GameTime_t m_blindUntilTime; // 0xbcc	
	GameTime_t m_blindStartTime; // 0xbd0	
	GameTime_t m_allowAutoFollowTime; // 0xbd4	
	// MNetworkEnable
	EntitySpottedState_t m_entitySpottedState; // 0xbd8	
	int32_t m_nSpotRules; // 0xbf0	
	// MNetworkEnable
	CSPlayerState m_iPlayerState; // 0xbf4	
	uint8_t __pad0bf8[0x8]; // 0xbf8
public:
	CountdownTimer m_chickenIdleSoundTimer; // 0xc00	
	CountdownTimer m_chickenJumpSoundTimer; // 0xc18	
	uint8_t __pad0c30[0xa0]; // 0xc30
public:
	Vector m_vecLastBookmarkedPosition; // 0xcd0	
	float m_flLastDistanceTraveledNotice; // 0xcdc	
	float m_flAccumulatedDistanceTraveled; // 0xce0	
	float m_flLastFriendlyFireDamageReductionRatio; // 0xce4	
	bool m_bRespawning; // 0xce8	
	uint8_t __pad0ce9[0x3]; // 0xce9
public:
	int32_t m_nLastPickupPriority; // 0xcec	
	float m_flLastPickupPriorityTime; // 0xcf0	
	// MNetworkEnable
	bool m_bIsScoped; // 0xcf4	
	// MNetworkEnable
	bool m_bIsWalking; // 0xcf5	
	// MNetworkEnable
	bool m_bResumeZoom; // 0xcf6	
	// MNetworkEnable
	bool m_bIsDefusing; // 0xcf7	
	// MNetworkEnable
	bool m_bIsGrabbingHostage; // 0xcf8	
	uint8_t __pad0cf9[0x3]; // 0xcf9
public:
	// MNetworkEnable
	CSPlayerBlockingUseAction_t m_iBlockingUseActionInProgress; // 0xcfc	
	// MNetworkEnable
	GameTime_t m_fImmuneToGunGameDamageTime; // 0xd00	
	// MNetworkEnable
	bool m_bGunGameImmunity; // 0xd04	
	uint8_t __pad0d05[0x3]; // 0xd05
public:
	// MNetworkEnable
	uint32_t m_unTotalRoundDamageDealt; // 0xd08	
	// MNetworkEnable
	float m_fMolotovDamageTime; // 0xd0c	
	// MNetworkEnable
	bool m_bHasMovedSinceSpawn; // 0xd10	
	// MNetworkEnable
	bool m_bCanMoveDuringFreezePeriod; // 0xd11	
	uint8_t __pad0d12[0x2]; // 0xd12
public:
	// MNetworkEnable
	float m_flGuardianTooFarDistFrac; // 0xd14	
	float m_flNextGuardianTooFarHurtTime; // 0xd18	
	// MNetworkEnable
	GameTime_t m_flDetectedByEnemySensorTime; // 0xd1c	
	float m_flDealtDamageToEnemyMostRecentTimestamp; // 0xd20	
	GameTime_t m_flLastEquippedHelmetTime; // 0xd24	
	GameTime_t m_flLastEquippedArmorTime; // 0xd28	
	// MNetworkEnable
	int32_t m_nHeavyAssaultSuitCooldownRemaining; // 0xd2c	
	bool m_bResetArmorNextSpawn; // 0xd30	
	uint8_t __pad0d31[0x3]; // 0xd31
public:
	GameTime_t m_flLastBumpMineBumpTime; // 0xd34	
	// MNetworkEnable
	GameTime_t m_flEmitSoundTime; // 0xd38	
	int32_t m_iNumSpawns; // 0xd3c	
	int32_t m_iShouldHaveCash; // 0xd40	
	bool m_bJustKilledTeammate; // 0xd44	
	bool m_bPunishedForTK; // 0xd45	
	bool m_bInvalidSteamLogonDelayed; // 0xd46	
	uint8_t __pad0d47[0x1]; // 0xd47
public:
	int32_t m_iTeamKills; // 0xd48	
	GameTime_t m_flLastAction; // 0xd4c	
	float m_flNameChangeHistory[5]; // 0xd50	
	float m_fLastGivenDefuserTime; // 0xd64	
	float m_fLastGivenBombTime; // 0xd68	
	// MNetworkEnable
	bool m_bHasNightVision; // 0xd6c	
	// MNetworkEnable
	bool m_bNightVisionOn; // 0xd6d	
	uint8_t __pad0d6e[0x2]; // 0xd6e
public:
	float m_fNextRadarUpdateTime; // 0xd70	
	float m_flLastMoneyUpdateTime; // 0xd74	
	char m_MenuStringBuffer[1024]; // 0xd78	
	float m_fIntroCamTime; // 0x1178	
	int32_t m_nMyCollisionGroup; // 0x117c	
	// MNetworkEnable
	bool m_bInNoDefuseArea; // 0x1180	
	// MNetworkEnable
	bool m_bKilledByTaser; // 0x1181	
	uint8_t __pad1182[0x2]; // 0x1182
public:
	// MNetworkEnable
	int32_t m_iMoveState; // 0x1184	
	GameTime_t m_grenadeParameterStashTime; // 0x1188	
	bool m_bGrenadeParametersStashed; // 0x118c	
	uint8_t __pad118d[0x3]; // 0x118d
public:
	QAngle m_angStashedShootAngles; // 0x1190	
	Vector m_vecStashedGrenadeThrowPosition; // 0x119c	
	Vector m_vecStashedVelocity; // 0x11a8	
	QAngle m_angShootAngleHistory[2]; // 0x11b4	
	Vector m_vecThrowPositionHistory[2]; // 0x11cc	
	Vector m_vecVelocityHistory[2]; // 0x11e4	
	bool m_bDiedAirborne; // 0x11fc	
	uint8_t __pad11fd[0x3]; // 0x11fd
public:
	CEntityIndex m_iBombSiteIndex; // 0x1200	
	// MNetworkEnable
	int32_t m_nWhichBombZone; // 0x1204	
	bool m_bInBombZoneTrigger; // 0x1208	
	bool m_bWasInBombZoneTrigger; // 0x1209	
	uint8_t __pad120a[0x2]; // 0x120a
public:
	// MNetworkEnable
	int32_t m_iDirection; // 0x120c	
	// MNetworkEnable
	int32_t m_iShotsFired; // 0x1210	
	// MNetworkEnable
	int32_t m_ArmorValue; // 0x1214	
	float m_flFlinchStack; // 0x1218	
	// MNetworkEnable
	float m_flVelocityModifier; // 0x121c	
	// MNetworkEnable
	float m_flHitHeading; // 0x1220	
	// MNetworkEnable
	int32_t m_nHitBodyPart; // 0x1224	
	int32_t m_iHostagesKilled; // 0x1228	
	Vector m_vecTotalBulletForce; // 0x122c	
	// MNetworkEnable
	float m_flFlashDuration; // 0x1238	
	// MNetworkEnable
	float m_flFlashMaxAlpha; // 0x123c	
	// MNetworkEnable
	float m_flProgressBarStartTime; // 0x1240	
	// MNetworkEnable
	int32_t m_iProgressBarDuration; // 0x1244	
	// MNetworkEnable
	bool m_bWaitForNoAttack; // 0x1248	
	uint8_t __pad1249[0x3]; // 0x1249
public:
	// MNetworkEnable
	float m_flLowerBodyYawTarget; // 0x124c	
	// MNetworkEnable
	bool m_bStrafing; // 0x1250	
	uint8_t __pad1251[0x3]; // 0x1251
public:
	Vector m_lastStandingPos; // 0x1254	
	float m_ignoreLadderJumpTime; // 0x1260	
	uint8_t __pad1264[0x4]; // 0x1264
public:
	CountdownTimer m_ladderSurpressionTimer; // 0x1268	
	Vector m_lastLadderNormal; // 0x1280	
	Vector m_lastLadderPos; // 0x128c	
	// MNetworkEnable
	// MNetworkEncoder
	// MNetworkPriority "32"
	QAngle m_thirdPersonHeading; // 0x1298	
	// MNetworkEnable
	// MNetworkPriority "32"
	float m_flSlopeDropOffset; // 0x12a4	
	// MNetworkEnable
	// MNetworkPriority "32"
	float m_flSlopeDropHeight; // 0x12a8	
	// MNetworkEnable
	// MNetworkPriority "32"
	Vector m_vHeadConstraintOffset; // 0x12ac	
	uint8_t __pad12b8[0x8]; // 0x12b8
public:
	int32_t m_iLastWeaponFireUsercmd; // 0x12c0	
	// MNetworkEnable
	// MNetworkEncoder
	// MNetworkPriority "32"
	QAngle m_angEyeAngles; // 0x12c4	
	bool m_bVCollisionInitted; // 0x12d0	
	uint8_t __pad12d1[0x3]; // 0x12d1
public:
	Vector m_storedSpawnPosition; // 0x12d4	
	QAngle m_storedSpawnAngle; // 0x12e0	
	bool m_bIsSpawning; // 0x12ec	
	// MNetworkEnable
	bool m_bHideTargetID; // 0x12ed	
	uint8_t __pad12ee[0x2]; // 0x12ee
public:
	int32_t m_nNumDangerZoneDamageHits; // 0x12f0	
	// MNetworkEnable
	bool m_bHud_MiniScoreHidden; // 0x12f4	
	// MNetworkEnable
	bool m_bHud_RadarHidden; // 0x12f5	
	uint8_t __pad12f6[0x2]; // 0x12f6
public:
	// MNetworkEnable
	CEntityIndex m_nLastKillerIndex; // 0x12f8	
	// MNetworkEnable
	int32_t m_nLastConcurrentKilled; // 0x12fc	
	// MNetworkEnable
	int32_t m_nDeathCamMusic; // 0x1300	
	// MNetworkEnable
	int32_t m_iAddonBits; // 0x1304	
	// MNetworkEnable
	int32_t m_iPrimaryAddon; // 0x1308	
	// MNetworkEnable
	int32_t m_iSecondaryAddon; // 0x130c	
	int32_t m_nTeamDamageGivenForMatch; // 0x1310	
	bool m_bTDGaveProtectionWarning; // 0x1314	
	bool m_bTDGaveProtectionWarningThisRound; // 0x1315	
	uint8_t __pad1316[0x2]; // 0x1316
public:
	float m_flLastTHWarningTime; // 0x1318	
	CUtlStringToken m_currentDeafnessFilter; // 0x131c	
	int32_t m_NumEnemiesKilledThisSpawn; // 0x1320	
	int32_t m_NumEnemiesKilledThisRound; // 0x1324	
	int32_t m_NumEnemiesAtRoundStart; // 0x1328	
	int32_t m_iRoundsWon; // 0x132c	
	int32_t m_lastRoundResult; // 0x1330	
	bool m_wasNotKilledNaturally; // 0x1334	
	uint8_t __pad1335[0x3]; // 0x1335
public:
	// MNetworkEnable
	uint32_t m_vecPlayerPatchEconIndices[5]; // 0x1338	
	int32_t m_iDeathFlags; // 0x134c	
	CHandle< CBaseEntity > m_hPet; // 0x1350	
	uint8_t __pad1354[0x1cc]; // 0x1354
public:
	// MNetworkEnable
	uint16_t m_unCurrentEquipmentValue; // 0x1520	
	// MNetworkEnable
	uint16_t m_unRoundStartEquipmentValue; // 0x1522	
	// MNetworkEnable
	uint16_t m_unFreezetimeEndEquipmentValue; // 0x1524	
	uint8_t __pad1526[0x2]; // 0x1526
public:
	int32_t m_nSuicides; // 0x1528	
	// MNetworkEnable
	int32_t m_nSurvivalTeamNumber; // 0x152c	
	bool m_bHasDeathInfo; // 0x1530	
	uint8_t __pad1531[0x3]; // 0x1531
public:
	float m_flDeathInfoTime; // 0x1534	
	Vector m_vecDeathInfoOrigin; // 0x1538	
	// MNetworkEnable
	bool m_bKilledByHeadshot; // 0x1544	
	uint8_t __pad1545[0x3]; // 0x1545
public:
	int32_t m_LastHitBox; // 0x1548	
	int32_t m_LastHealth; // 0x154c	
	float m_flLastCollisionCeiling; // 0x1550	
	float m_flLastCollisionCeilingChangeTime; // 0x1554	
	void* m_pBot; // 0x1558	
	bool m_bBotAllowActive; // 0x1560	
	bool m_bCommittingSuicideOnTeamChange; // 0x1561	
};
static_assert(sizeof(CCSPlayerPawnBase) == 0x1568, "Class didn't match expected size");

class CCSPlayerPawn : public CCSPlayerPawnBase
{
public:
	// MNetworkEnable
	void* m_pBulletServices; // 0x1568	
	// MNetworkEnable
	void* m_pHostageServices; // 0x1570	
	// MNetworkEnable
	void* m_pBuyServices; // 0x1578	
	// MNetworkEnable
	void* m_pActionTrackingServices; // 0x1580	
	void* m_pRadioServices; // 0x1588	
	void* m_pDamageReactServices; // 0x1590	
	uint16_t m_nCharacterDefIndex; // 0x1598	
	uint8_t __pad159a[0x6]; // 0x159a
	uint64_t m_hPreviousModel; // 0x15a0	
	// MNetworkEnable
	bool m_bHasFemaleVoice; // 0x15a8	
	uint8_t __pad15a9[0x7]; // 0x15a9
	CUtlString m_strVOPrefix; // 0x15b0	
	// MNetworkEnable
	char m_szLastPlaceName[18]; // 0x15b8	
	uint8_t __pad15ca[0xae]; // 0x15ca
	// MNetworkEnable
	bool m_bInBuyZone; // 0x1678	
	bool m_bWasInBuyZone; // 0x1679	
	// MNetworkEnable
	bool m_bInHostageRescueZone; // 0x167a	
	// MNetworkEnable
	bool m_bInBombZone; // 0x167b	
	bool m_bWasInHostageRescueZone; // 0x167c	
	uint8_t __pad167d[0x3]; // 0x167d
	// MNetworkEnable
	int32_t m_iRetakesOffering; // 0x1680	
	// MNetworkEnable
	int32_t m_iRetakesOfferingCard; // 0x1684	
	// MNetworkEnable
	bool m_bRetakesHasDefuseKit; // 0x1688	
	// MNetworkEnable
	bool m_bRetakesMVPLastRound; // 0x1689	
	uint8_t __pad168a[0x2]; // 0x168a
	// MNetworkEnable
	int32_t m_iRetakesMVPBoostItem; // 0x168c	
	// MNetworkEnable
	int m_RetakesMVPBoostExtraUtility; // 0x1690	
	// MNetworkEnable
	GameTime_t m_flHealthShotBoostExpirationTime; // 0x1694	
	float m_flLandseconds; // 0x1698	
	// MNetworkEnable
	// MNetworkBitCount "32"
	QAngle m_aimPunchAngle; // 0x169c	
	// MNetworkEnable
	// MNetworkBitCount "32"
	QAngle m_aimPunchAngleVel; // 0x16a8	
	// MNetworkEnable
	int32_t m_aimPunchTickBase; // 0x16b4	
	// MNetworkEnable
	float m_aimPunchTickFraction; // 0x16b8	
	uint8_t __pad16bc[0x4]; // 0x16bc
	CUtlVector< QAngle > m_aimPunchCache; // 0x16c0	
	// MNetworkEnable
	bool m_bIsBuyMenuOpen; // 0x16d8	
	uint8_t __pad16d9[0x557]; // 0x16d9
	CTransform m_xLastHeadBoneTransform; // 0x1c30	
	bool m_bLastHeadBoneTransformIsValid; // 0x1c50	
	uint8_t __pad1c51[0x3]; // 0x1c51
	GameTime_t m_lastLandTime; // 0x1c54	
	int32_t m_iPlayerLocked; // 0x1c58	
	uint8_t __pad1c5c[0x4]; // 0x1c5c
	// MNetworkEnable
	GameTime_t m_flTimeOfLastInjury; // 0x1c60	
	// MNetworkEnable
	// MNetworkUserGroup "LocalPlayerExclusive"
	GameTime_t m_flNextSprayDecalTime; // 0x1c64	
	bool m_bNextSprayDecalTimeExpedited; // 0x1c68	
	uint8_t __pad1c69[0x3]; // 0x1c69
	// MNetworkEnable
	int32_t m_nRagdollDamageBone; // 0x1c6c	
	// MNetworkEnable
	Vector m_vRagdollDamageForce; // 0x1c70	
	// MNetworkEnable
	Vector m_vRagdollDamagePosition; // 0x1c7c	
	// MNetworkEnable
	char m_szRagdollDamageWeaponName[64]; // 0x1c88	
	// MNetworkEnable
	bool m_bRagdollDamageHeadshot; // 0x1cc8	

	uint8_t __pad1cc9[0x7]; // 0x1cc9
	// MNetworkEnable
	uint8_t m_EconGloves[0x278]; // 0x1cd0	
	// MNetworkEnable
	QAngle m_qDeathEyeAngles; // 0x1f48	
	bool m_bSkipOneHeadConstraintUpdate; // 0x1f54	
	uint8_t __pad1f55[0xb]; // 0x1cc9
};
static_assert(sizeof(CCSPlayerPawn) == 0x1f60, "Class didn't match expected size");

// Size: 0x20
struct locksound_t
{
public:
	uint8_t __pad0000[0x8]; // 0x0
	CUtlSymbolLarge sLockedSound; // 0x8
	CUtlSymbolLarge sUnlockedSound; // 0x10
	GameTime_t flwaitSound; // 0x18
};
static_assert(sizeof(locksound_t) == 0x20, "Class didn't match expected size");

// Size: 0x8c8
class CBaseButton : public CBaseToggle
{
public:
	QAngle m_angMoveEntitySpace; // 0x780
	bool m_fStayPushed; // 0x78c
	bool m_fRotating; // 0x78d
	uint8_t __pad078e[0x2]; // 0x78e
	locksound_t m_ls; // 0x790
	CUtlSymbolLarge m_sUseSound; // 0x7b0
	CUtlSymbolLarge m_sLockedSound; // 0x7b8
	CUtlSymbolLarge m_sUnlockedSound; // 0x7c0
	bool m_bLocked; // 0x7c8
	bool m_bDisabled; // 0x7c9
	uint8_t __pad07ca[0x2]; // 0x7ca
	GameTime_t m_flUseLockedTime; // 0x7cc
	bool m_bSolidBsp; // 0x7d0
	uint8_t __pad07d1[0x7]; // 0x7d1
	CEntityIOOutput m_OnDamaged; // 0x7d8
	CEntityIOOutput m_OnPressed; // 0x800
	CEntityIOOutput m_OnUseLocked; // 0x828
	CEntityIOOutput m_OnIn; // 0x850
	CEntityIOOutput m_OnOut; // 0x878
	int32_t m_nState; // 0x8a0
	CEntityHandle m_hConstraint; // 0x8a4
	CEntityHandle m_hConstraintParent; // 0x8a8
	bool m_bForceNpcExclude; // 0x8ac
	uint8_t __pad08ad[0x3]; // 0x8ad
	CUtlSymbolLarge m_sGlowEntity; // 0x8b0
	// MNetworkEnable
	CHandle< CBaseModelEntity > m_glowEntity; // 0x8b8
	// MNetworkEnable
	bool m_usable; // 0x8bc
	uint8_t __pad08bd[0x3]; // 0x8bd
	// MNetworkEnable
	CUtlSymbolLarge m_szDisplayText; // 0x8c0
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
	uint8_t traceunknown5[6];
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
