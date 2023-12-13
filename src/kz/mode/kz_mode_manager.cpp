#include "kz_mode.h"
#include "kz_mode_vnl.h"
#include "interfaces/interfaces.h"


void KZ::mode::InitModeService(KZPlayer *player)
{
	player->modeService = new KZVanillaModeService(player);
}

void KZ::mode::DisableReplicatedModeCvars()
{
	u32 numCvar = sizeof(KZ::mode::modeCvarNames) / sizeof(KZ::mode::modeCvarNames[0]);

	for (u32 i = 0; i < numCvar; ++i)
	{
		ConVarHandle cvarHandle = g_pCVar->FindConVar(KZ::mode::modeCvarNames[i]);
		if (!cvarHandle.IsValid()) continue;
		ConVar *cvar = g_pCVar->GetConVar(cvarHandle);

		cvar->flags &= ~FCVAR_REPLICATED;
	}
}

void KZ::mode::EnableReplicatedModeCvars()
{
	u32 numCvar = sizeof(KZ::mode::modeCvarNames) / sizeof(KZ::mode::modeCvarNames[0]);

	for (u32 i = 0; i < numCvar; ++i)
	{
		ConVarHandle cvarHandle = g_pCVar->FindConVar(KZ::mode::modeCvarNames[i]);
		if (!cvarHandle.IsValid()) continue;
		ConVar *cvar = g_pCVar->GetConVar(cvarHandle);

		cvar->flags |= FCVAR_REPLICATED;
	}
}