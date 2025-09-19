#pragma once
#include "cbasemodelentity.h"
#ifndef IDA_IGNORE
class CParticleSystem : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CParticleSystem)

	SCHEMA_FIELD_POINTER(char, m_szSnapshotFileName)
	SCHEMA_FIELD(bool, m_bActive)
	SCHEMA_FIELD(bool, m_bFrozen)
	SCHEMA_FIELD(float32, m_flFreezeTransitionDuration)
	SCHEMA_FIELD(int32, m_nStopType)
	SCHEMA_FIELD(bool, m_bAnimateDuringGameplayPause)
	SCHEMA_FIELD(GameTime_t, m_flStartTime)
	SCHEMA_FIELD(float32, m_flPreSimTime)
	SCHEMA_FIELD_POINTER(Vector, m_vServerControlPoints)
	SCHEMA_FIELD_POINTER(uint8, m_iServerControlPointAssignments)
	SCHEMA_FIELD_POINTER(CHandle<CBaseEntity>, m_hControlPointEnts)
	SCHEMA_FIELD(bool, m_bNoSave)
	SCHEMA_FIELD(bool, m_bNoFreeze)
	SCHEMA_FIELD(bool, m_bNoRamp)
	SCHEMA_FIELD(bool, m_bStartActive)
	SCHEMA_FIELD(CUtlSymbolLarge, m_iszEffectName)
	SCHEMA_FIELD_POINTER(CUtlSymbolLarge, m_iszControlPointNames)
	SCHEMA_FIELD(int32, m_nDataCP)
	SCHEMA_FIELD(Vector, m_vecDataCPValue)
	SCHEMA_FIELD(int32, m_nTintCP)
	SCHEMA_FIELD(Color, m_clrTint)
};

#define CUSTOM_PARTICLE_SYSTEM_TEAM 5
#endif
