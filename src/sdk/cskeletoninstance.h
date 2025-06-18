#pragma once
#include "sdk/entity/cbaseentity.h"

class IPhysAggregateInstance;

class CModelState
{
public:
	DECLARE_SCHEMA_CLASS(CModelState)

private:
	SCHEMA_FIELD_POINTER_OFFSET(IPhysAggregateInstance *, m_bClientClothCreationSuppressed, -8)

public:
	IPhysAggregateInstance *GetPhysAggregateInstance()
	{
		return *m_bClientClothCreationSuppressed();
	}
};

class CSkeletonInstance : public CGameSceneNode
{
public:
	DECLARE_SCHEMA_CLASS(CSkeletonInstance)
	SCHEMA_FIELD(CModelState, m_modelState);
};
