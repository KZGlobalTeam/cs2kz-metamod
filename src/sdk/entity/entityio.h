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

	struct
	{
		KeyValues3 unknown;
		bool unknown2;
	} m_Unknown;
};

struct EntityIOConnection_t : EntityIOConnectionDesc_t
{
	bool m_bMarkedForRemoval;
	EntityIOConnection_t *m_pNext;
};

static_assert(offsetof(EntityIOConnection_t, m_bMarkedForRemoval) == 0x40, "EntityIOConnection_t::m_bMarkedForRemoval offset is incorrect");
static_assert(sizeof(EntityIOConnection_t) == 0x50, "EntityIOConnection_t size is incorrect");

struct EntityIOOutputDesc_t
{
	const char *m_pName;
	uint32 m_nFlags;
	uint32 m_nOutputOffset;
};
#ifndef IDA_IGNORE
class CEntityIOOutput
{
public:
	void *vtable;
	EntityIOConnection_t *m_pConnections;
	EntityIOOutputDesc_t *m_pDesc;
};
#endif
