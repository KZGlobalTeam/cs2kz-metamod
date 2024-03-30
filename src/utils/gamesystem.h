#pragma once

#include "common.h"
#include "entitysystem.h"
#include "igamesystemfactory.h"

bool InitGameSystems();

class CGameSystem : public CBaseGameSystem
{
public:
	GS_EVENT(ServerGamePostSimulate);

	void Shutdown() override
	{
		delete sm_Factory;
	}

	void SetGameSystemGlobalPtrs(void *pValue) override
	{
		if (sm_Factory)
		{
			sm_Factory->SetGlobalPtr(pValue);
		}
	}

	bool DoesGameSystemReallocate() override
	{
		return sm_Factory->ShouldAutoAdd();
	}

	static IGameSystemFactory *sm_Factory;
};
