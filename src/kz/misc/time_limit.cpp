
#include "common.h"
#include "kz/kz.h"
#include "kz/option/kz_option.h"
// Make sure that the server can't run for too long.
static_global ConVar *mp_roundtime {};
static_global ConVar *mp_timelimit {};

void KZ::misc::EnforceTimeLimit()
{
	if (!mp_roundtime)
	{
		mp_roundtime = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_roundtime"));
	}
	if (!mp_timelimit)
	{
		mp_timelimit = g_pCVar->GetConVar(g_pCVar->FindConVar("mp_timelimit"));
	}
	if (mp_roundtime)
	{
		mp_roundtime->m_cvvMaxValue->m_flValue = 1440.0f;
	}
	if (mp_timelimit)
	{
		*(f32 *)(&mp_timelimit->values) = *(f32 *)(&mp_roundtime->values);
	}
}

void KZ::misc::InitTimeLimit()
{
	f32 timeLimit = KZOptionService::GetOptionFloat("defaultTimeLimit", 60.0f);
	char command[32];
	V_snprintf(command, sizeof(command), "mp_roundtime %f", timeLimit);
	interfaces::pEngine->ServerCommand(command);
}
