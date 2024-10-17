#pragma once

#include "utils/gameconfig.h"
#include "utils/schema.h"
#include "ehandle.h"

extern CGameConfig *g_pGameConfig;

class CCollisionProperty;

class CNetworkedQuantizedFloat
{
public:
	float32 m_Value;
	uint16 m_nEncoder;
	bool m_bUnflattened;
};

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

class CBaseEntity : public CEntityInstance
{
public:
	DECLARE_SCHEMA_CLASS(CBaseEntity)

	SCHEMA_FIELD(CBodyComponent *, m_CBodyComponent)
	SCHEMA_FIELD(CBitVec<64>, m_isSteadyState)
	SCHEMA_FIELD(float, m_lastNetworkChange)
	SCHEMA_FIELD_POINTER(void, m_NetworkTransmitComponent)
	SCHEMA_FIELD(int, m_iHealth)
	SCHEMA_FIELD(uint8, m_lifeState)
	SCHEMA_FIELD(int, m_iTeamNum)
	SCHEMA_FIELD(CUtlString, m_sUniqueHammerID)
	SCHEMA_FIELD(bool, m_bTakesDamage)
	SCHEMA_FIELD(MoveType_t, m_MoveType)
	SCHEMA_FIELD(MoveType_t, m_nActualMoveType)
	SCHEMA_FIELD(Vector, m_vecBaseVelocity)
	SCHEMA_FIELD(Vector, m_vecAbsVelocity)
	SCHEMA_FIELD(CNetworkVelocityVector, m_vecVelocity)
	SCHEMA_FIELD(CCollisionProperty *, m_pCollision)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hGroundEntity)
	SCHEMA_FIELD(uint32_t, m_fFlags)
	SCHEMA_FIELD(float, m_flGravityScale)
	SCHEMA_FIELD(float, m_flWaterLevel)
	SCHEMA_FIELD(int, m_fEffects)

	int entindex()
	{
		return m_pEntity->m_EHandle.GetEntryIndex();
	}

	bool IsPawn()
	{
		return CALL_VIRTUAL(bool, g_pGameConfig->GetOffset("IsEntityPawn"), this);
	}

	bool IsController()
	{
		return CALL_VIRTUAL(bool, g_pGameConfig->GetOffset("IsEntityController"), this);
	}

	bool IsAlive()
	{
		return this->m_lifeState() == LIFE_ALIVE;
	}

	void SetMoveType(MoveType_t movetype)
	{
		this->m_MoveType(movetype);
		this->m_nActualMoveType(movetype);
	}

	void CollisionRulesChanged()
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("CollisionRulesChanged"), this);
	}

	int GetTeam()
	{
		return m_iTeamNum();
	}

	void StartTouch(CBaseEntity *pOther)
	{
		CALL_VIRTUAL(bool, g_pGameConfig->GetOffset("StartTouch"), this, pOther);
	}

	void Touch(CBaseEntity *pOther)
	{
		CALL_VIRTUAL(bool, g_pGameConfig->GetOffset("Touch"), this, pOther);
	}

	void EndTouch(CBaseEntity *pOther)
	{
		CALL_VIRTUAL(bool, g_pGameConfig->GetOffset("EndTouch"), this, pOther);
	}

	void Teleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
	{
		CALL_VIRTUAL(bool, g_pGameConfig->GetOffset("Teleport"), this, newPosition, newAngles, newVelocity);
	}
};
