#pragma once
#include "sdk/entity/cbaseentity.h"

class IPhysAggregateInstance;

class CModelState
{
public:
	DECLARE_SCHEMA_CLASS(CModelState)

	SCHEMA_FIELD(IPhysAggregateInstance *, m_pVPhysicsAggregate)
};

class CSkeletonInstance : public CGameSceneNode
{
public:
	DECLARE_SCHEMA_CLASS(CSkeletonInstance)
	SCHEMA_FIELD(CModelState, m_modelState);
};
