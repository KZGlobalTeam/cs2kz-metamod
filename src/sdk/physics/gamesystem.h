#pragma once
#include "types.h"

class CPhysAggregateInstance
{
	u8 unknown[0x18];
	void *physicsWorld;

public:
	CPhysAggregateData *aggregateData;
};

class CPhysicsGameSystem
{
public:
	struct PhysicsSpawnGroups_t
	{
		u8 unknown[0x10];
		CPhysAggregateInstance *m_pLevelAggregateInstance;
		u8 unknown2[0x58];
	};

	uint8_t unk[0x20];
	CUtlMap<uint, PhysicsSpawnGroups_t> m_PhysicsSpawnGroups;
};
