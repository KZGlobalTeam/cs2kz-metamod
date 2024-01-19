#pragma once

#include "utils/addresses.h"
#include "utils/schema.h"
#include "mathlib/vector.h"
#include "baseentity.h"
#include "../ccollisionproperty.h"

class CNetworkOriginCellCoordQuantizedVector
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CNetworkOriginCellCoordQuantizedVector)

	SCHEMA_FIELD(uint16, m_cellX)
	SCHEMA_FIELD(uint16, m_cellY)
	SCHEMA_FIELD(uint16, m_cellZ)
	SCHEMA_FIELD(uint16, m_nOutsideWorld)

	SCHEMA_FIELD(CNetworkedQuantizedFloat, m_vecX)
	SCHEMA_FIELD(CNetworkedQuantizedFloat, m_vecY)
	SCHEMA_FIELD(CNetworkedQuantizedFloat, m_vecZ)
};

class CNetworkVelocityVector
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CNetworkVelocityVector)

	SCHEMA_FIELD(CNetworkedQuantizedFloat, m_vecX)
	SCHEMA_FIELD(CNetworkedQuantizedFloat, m_vecY)
	SCHEMA_FIELD(CNetworkedQuantizedFloat, m_vecZ)
};

class CGameSceneNode
{
public:
	DECLARE_SCHEMA_CLASS(CGameSceneNode)

	SCHEMA_FIELD(CEntityInstance *, m_pOwner)
	SCHEMA_FIELD(CGameSceneNode *, m_pParent)
	SCHEMA_FIELD(CGameSceneNode *, m_pChild)
	SCHEMA_FIELD(CNetworkOriginCellCoordQuantizedVector, m_vecOrigin)
	SCHEMA_FIELD(QAngle, m_angRotation)
	SCHEMA_FIELD(float, m_flScale)
	SCHEMA_FIELD(float, m_flAbsScale)
	SCHEMA_FIELD(Vector, m_vecAbsOrigin)
	SCHEMA_FIELD(Vector, m_vRenderOrigin)
};

class CBodyComponent
{
public:
	DECLARE_SCHEMA_CLASS(CBodyComponent)

	SCHEMA_FIELD(CGameSceneNode *, m_pSceneNode)
};

class CBaseEntity2 : public CBaseEntity
{
public:
	typedef CBaseEntity2 ThisClass;
	static constexpr const char *ThisClassName = "CBaseEntity";
	static constexpr bool IsStruct = false;

	SCHEMA_FIELD(CBodyComponent *, m_CBodyComponent)
	SCHEMA_FIELD(CBitVec<64>, m_isSteadyState)
	SCHEMA_FIELD(float, m_lastNetworkChange)
	SCHEMA_FIELD_POINTER(void, m_NetworkTransmitComponent)
	SCHEMA_FIELD(int, m_iHealth)
	SCHEMA_FIELD(uint8, m_lifeState)
	SCHEMA_FIELD(int, m_iTeamNum)
	SCHEMA_FIELD(bool, m_bTakesDamage)
	SCHEMA_FIELD(MoveType_t, m_MoveType)
	SCHEMA_FIELD(Vector, m_vecBaseVelocity)
	SCHEMA_FIELD(Vector, m_vecAbsVelocity)
	SCHEMA_FIELD(CNetworkVelocityVector, m_vecVelocity)
	SCHEMA_FIELD(CCollisionProperty *, m_pCollision)
	SCHEMA_FIELD(CHandle< CBaseEntity2 >, m_hGroundEntity)
	SCHEMA_FIELD(uint32_t, m_fFlags)
	SCHEMA_FIELD(float, m_flGravityScale)
	SCHEMA_FIELD(float, m_flWaterLevel)

	int entindex() { return m_pEntity->m_EHandle.GetEntryIndex(); }
	bool IsPawn() { return CALL_VIRTUAL(bool, offsets::IsEntityPawn, this); }
	bool IsController() { return CALL_VIRTUAL(bool, offsets::IsEntityController, this); }

	void SetMoveType(MoveType_t movetype) { CALL_VIRTUAL(void, offsets::SetMoveType, this, movetype); }
	void CollisionRulesChanged() { CALL_VIRTUAL(void, offsets::CollisionRulesChanged, this); }
};
