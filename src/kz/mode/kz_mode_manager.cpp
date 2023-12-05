#include "kz_mode.h"
#include "kz_mode_vnl.h"
#include "interfaces/interfaces.h"

void KZ::mode::InitModeCvars()
{
	for (u32 i = 0; i < numCvar; i++)
	{
		ConVarHandle cvarHandle = g_pCVar->FindConVar(KZ::mode::modeCvarNames[i]);
		if (!cvarHandle.IsValid()) continue;
		modeCvars[i] = g_pCVar->GetConVar(cvarHandle);
	}
}

void KZ::mode::InitModeService(KZPlayer *player)
{
	player->modeService = new KZVanillaModeService(player);
}

void KZ::mode::DisableReplicatedModeCvars()
{
	return;
	for (u32 i = 0; i < numCvar; ++i)
	{
		assert(modeCvars[i]);
		modeCvars[i]->flags &= ~FCVAR_REPLICATED;
	}
}

void KZ::mode::EnableReplicatedModeCvars()
{
	return;
	for (u32 i = 0; i < numCvar; ++i)
	{
		assert(modeCvars[i]);
		modeCvars[i]->flags |= FCVAR_REPLICATED;
	}
}