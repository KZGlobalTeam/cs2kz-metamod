#pragma once
#include "utils/addresses.h"
#include "utils/gameconfig.h"

class CGameEntitySystem;
extern CGameConfig *g_pGameConfig;

class CGameResourceService
{
public:
	CGameEntitySystem *GetGameEntitySystem()
	{
		static_persist const uintptr_t offset = g_pGameConfig->GetOffset("GameEntitySystem");
		return *reinterpret_cast<CGameEntitySystem **>((uintptr_t)(this) + offset);
	}
};
