#pragma once

#include <string_t.h>
#include <entityhandle.h>
#include <entitysystem.h>

struct EntityIOConnectionDesc_t
{
	string_t m_targetDesc;
	string_t m_targetInput;
	string_t m_valueOverride;
	CEntityHandle m_hTarget;
	EntityIOTargetType_t m_nTargetType;
	int32 m_nTimesToFire;
	float m_flDelay;
};

struct EntityIOConnection_t : EntityIOConnectionDesc_t
{
	bool m_bMarkedForRemoval;
	EntityIOConnection_t *m_pNext;
};

struct EntityIOOutputDesc_t
{
	const char *m_pName;
	uint32 m_nFlags;
	uint32 m_nOutputOffset;
};

class CEntityIOOutput
{
public:
	void *vtable;
	EntityIOConnection_t *m_pConnections;
	EntityIOOutputDesc_t *m_pDesc;
};
