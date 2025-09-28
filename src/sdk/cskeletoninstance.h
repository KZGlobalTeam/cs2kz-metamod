#pragma once
#include "sdk/entity/cbaseentity.h"

class IPhysAggregateInstance;

class CModelState
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CModelState, 0)

	SCHEMA_FIELD(IPhysAggregateInstance *, m_pVPhysicsAggregate)
	SCHEMA_FIELD(uint64, m_MeshGroupMask)
	SCHEMA_FIELD_POINTER(CUtlVector<int32>, m_nBodyGroupChoices)
	SCHEMA_FIELD(CUtlSymbolLarge, m_ModelName)
};

class CSkeletonInstance : public CGameSceneNode
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CSkeletonInstance)
	SCHEMA_FIELD(CModelState, m_modelState);
};
