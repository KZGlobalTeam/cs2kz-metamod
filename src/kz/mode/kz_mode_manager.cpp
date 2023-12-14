#include "kz_mode.h"
#include "kz_mode_vnl.h"
#include "interfaces/interfaces.h"

bool KZ::mode::InitModeCvars()
{
	bool success = true;
	for (u32 i = 0; i < numCvar; i++)
	{
		ConVarHandle cvarHandle = g_pCVar->FindConVar(KZ::mode::modeCvarNames[i]);
		if (!cvarHandle.IsValid())
		{
			META_CONPRINTF("Failed to find %s!\n", KZ::mode::modeCvarNames[i]);
			success = false;
		}
		modeCvars[i] = g_pCVar->GetConVar(cvarHandle);
	}
	return success;
}

void KZ::mode::InitModeService(KZPlayer *player)
{
	player->modeService = new KZVanillaModeService(player);
}

void KZ::mode::DisableReplicatedModeCvars()
{
	for (u32 i = 0; i < numCvar; ++i)
	{
		assert(modeCvars[i]);
		modeCvars[i]->flags &= ~FCVAR_REPLICATED;
	}
}

void KZ::mode::EnableReplicatedModeCvars()
{
	for (u32 i = 0; i < numCvar; ++i)
	{
		assert(modeCvars[i]);
		modeCvars[i]->flags |= FCVAR_REPLICATED;
	}
}