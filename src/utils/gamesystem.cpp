/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cs2kz.h"
#include "utils/utils.h"
#include "gameconfig.h"
#include "addresses.h"
#include "gamesystem.h"
#include "tier0/vprof.h"
#include "tier0/memdbgon.h"

extern CGameConfig *g_pGameConfig;

CBaseGameSystemFactory **CBaseGameSystemFactory::sm_pFirst = nullptr;
CGameSystem g_GameSystem;
IGameSystemFactory *CGameSystem::sm_Factory = nullptr;

// This mess is needed to get the pointer to sm_pFirst so we can insert game systems
bool InitGameSystems()
{
	uint8 *ptr = (uint8 *)g_pGameConfig->ResolveSignature("IGameSystem_InitAllSystems_pFirst") + 3;

	if (!ptr)
	{
		Warning("Failed to InitGameSystems, see warnings above.\n");
		return false;
	}

	// Grab the offset as 4 bytes
	uint32 offset = *(uint32 *)ptr;

	// Go to the next instruction, which is the starting point of the relative jump
	ptr += 4;

	// Now grab our pointer
	CBaseGameSystemFactory::sm_pFirst = (CBaseGameSystemFactory **)(ptr + offset);

	// And insert the game system(s)
	CGameSystem::sm_Factory = new CGameSystemStaticFactory<CGameSystem>("CS2KZ_GameSystem", &g_GameSystem);

	return true;
}

GS_EVENT_MEMBER(CGameSystem, BuildGameSessionManifest)
{
	Warning("CGameSystem::BuildGameSessionManifest\n");

	IEntityResourceManifest *pResourceManifest = msg->m_pResourceManifest;

	// This takes any resource type, model or not
	// Any resource adding MUST be done here, the resource manifest is not long-lived
	// pResourceManifest->AddResource("characters/models/my_character_model.vmdl");
	if (g_KZPlugin.IsAddonMounted())
	{
		Warning("[CS2KZ] Precache soundevents_kz\n");
		pResourceManifest->AddResource("soundevents/soundevents_kz.vsndevts");
	}
}
