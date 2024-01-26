#pragma once

#include "utils/addresses.h"
#include "utils/schema.h"
#include "mathlib/vector.h"
#include "baseentity.h"
#include "../ccollisionproperty.h"
#include "ehandle.h"

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

	void SetMoveType(MoveType_t movetype) { this->m_MoveType(movetype); }
	void CollisionRulesChanged() { CALL_VIRTUAL(void, offsets::CollisionRulesChanged, this); }

	// Vtable stuff
	virtual ~CBaseEntity2() = 0;
private:
	virtual void Unk_35() = 0;
	virtual void Unk_36() = 0;
	virtual void Unk_37() = 0;
	virtual void Unk_38() = 0;
	virtual void Unk_39() = 0;
	virtual void Unk_40() = 0;
	virtual void Unk_41() = 0;
	virtual void Unk_42() = 0;
	virtual void Unk_43() = 0;
	virtual void Unk_44() = 0;
	virtual void Unk_45() = 0;
	virtual void Unk_46() = 0;
	virtual void Unk_47() = 0;
	virtual void Unk_48() = 0;
	virtual void Unk_49() = 0;
	virtual void Unk_50() = 0;
	virtual void Unk_51() = 0;
	virtual void Unk_52() = 0;
	virtual void Unk_53() = 0;
	virtual void Unk_54() = 0;
	virtual void Unk_55() = 0;
	virtual void Unk_56() = 0;
	virtual void Unk_57() = 0;
	virtual void Unk_58() = 0;
	virtual void Unk_59() = 0;
	virtual void Unk_60() = 0;
	virtual void Unk_61() = 0;
	virtual void Unk_62() = 0;
	virtual void Unk_63() = 0;
	virtual void Unk_64() = 0;
	virtual void Unk_65() = 0;
	virtual void Unk_66() = 0;
	virtual void Unk_67() = 0;
	virtual void Unk_68() = 0;
	virtual void Unk_69() = 0;
	virtual void Unk_70() = 0;
	virtual void Unk_71() = 0;
	virtual void Unk_72() = 0;
	virtual void Unk_73() = 0;
	virtual void Unk_74() = 0;
	virtual void Unk_75() = 0;
	virtual void Unk_76() = 0;
	virtual void Unk_77() = 0;
	virtual void Unk_78() = 0;
	virtual void Unk_79() = 0;
	virtual void Unk_80() = 0;
	virtual void Unk_81() = 0;
	virtual void Unk_82() = 0;
	virtual void Unk_83() = 0;
	virtual void Unk_84() = 0;
	virtual void Unk_85() = 0;
	virtual void Unk_86() = 0;
	virtual void Unk_87() = 0;
	virtual void Unk_88() = 0;
	virtual void Unk_89() = 0;
	virtual void Unk_90() = 0;
	virtual void Unk_91() = 0;
	virtual void Unk_92() = 0;
	virtual void Unk_93() = 0;
	virtual void Unk_94() = 0;
	virtual void Unk_95() = 0;
	virtual void Unk_96() = 0;
	virtual void Unk_97() = 0;
	virtual void Unk_98() = 0;
	virtual void Unk_99() = 0;
	virtual void Unk_100() = 0;
	virtual void Unk_101() = 0;
	virtual void Unk_102() = 0;
	virtual void Unk_103() = 0;
	virtual void Unk_104() = 0;
	virtual void Unk_105() = 0;
	virtual void Unk_106() = 0;
	virtual void Unk_107() = 0;
	virtual void Unk_108() = 0;
	virtual void Unk_109() = 0;
	virtual void Unk_110() = 0;
	virtual void Unk_111() = 0;
	virtual void Unk_112() = 0;
	virtual void Unk_113() = 0;
	virtual void Unk_114() = 0;
	virtual void Unk_115() = 0;
	virtual void Unk_116() = 0;
	virtual void Unk_117() = 0;
	virtual void Unk_118() = 0;
	virtual void Unk_119() = 0;
	virtual void Unk_120() = 0;
	virtual void Unk_121() = 0;
	virtual void Unk_122() = 0;
	virtual void Unk_123() = 0;
	virtual void Unk_124() = 0;
	virtual void Unk_125() = 0;
	virtual void Unk_126() = 0;
	virtual void Unk_127() = 0;
	virtual void Unk_128() = 0;
	virtual void Unk_129() = 0;
	virtual void Unk_130() = 0;
	virtual void Unk_131() = 0;
	virtual void Unk_132() = 0;
public:
	// Windows: 133
	virtual void StartTouch(CBaseEntity2 *pOther) = 0;
	virtual void Touch(CBaseEntity2 *pOther) = 0;
	virtual void EndTouch(CBaseEntity2 *pOther) = 0;
};
