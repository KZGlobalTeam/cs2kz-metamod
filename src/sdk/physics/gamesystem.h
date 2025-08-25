#pragma once
#include "types.h"

class CPhysicsGameSystem
{
public:
	struct PhysicsSpawnGroups_t
	{
		uint8 unknown[0x10];
		CPhysAggregateInstance *m_pLevelAggregateInstance;
		uint8 unknown2[0x58];
	};

	uint8 unk[0x78];
	CUtlMap<uint, PhysicsSpawnGroups_t> m_PhysicsSpawnGroups;
};
