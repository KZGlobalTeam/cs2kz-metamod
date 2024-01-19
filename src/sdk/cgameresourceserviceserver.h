#pragma once
#include "utils/addresses.h"

class CGameEntitySystem;

class CGameResourceService
{
public:
	CGameEntitySystem *GetGameEntitySystem()
	{
		return *reinterpret_cast<CGameEntitySystem **>((uintptr_t)(this) + offsets::GameEntitySystem);
	}
};
