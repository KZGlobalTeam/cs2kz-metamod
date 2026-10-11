#pragma once
#include "cbasemodelentity.h"

class CBaseAnimGraph : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CBaseAnimGraph);

	// Networked to every client. When false, the client skips this entity's AnimGraph2 update and the pose holds.
	SCHEMA_FIELD(bool, m_bAnimGraphUpdateEnabled)

	void SetAnimGraphUpdateEnabled(bool enabled)
	{
		if (this->m_bAnimGraphUpdateEnabled() != enabled)
		{
			this->m_bAnimGraphUpdateEnabled(enabled);
		}
	}
};
