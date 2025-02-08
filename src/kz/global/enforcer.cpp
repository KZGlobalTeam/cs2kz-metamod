#include "kz_global.h"

enum EnforcedServerCvars
{
	ENFORCEDCVAR_FIRST = 0,
	ENFORCEDCVAR_SV_CHEATS = 0,
	ENFORCEDCVAR_MP_DROP_KNIFE_ENABLE,
	ENFORCEDCVAR_SV_TURBOPHYSICS,
	ENFORCEDCVAR_CQ_ENABLE,
	ENFORCEDCVAR_CQ_BUFFER_BLOAT_MSECS_MAX,
	ENFORCEDCVAR_CQ_DILATION_PERCENTAGE,
	ENFORCEDCVAR_SV_CQ_MIN_QUEUE,
	ENFORCEDCVAR_SV_CONDENSE_LATE_BUTTONS,
	ENFORCEDCVAR_SV_LATE_COMMANDS_ALLOWED,
	ENFORCEDCVAR_CQ_MAX_STARVED_SUBSTITUTE_COMMANDS,
	ENFORCEDCVAR_SV_CQ_TRIM_BLOAT_REMAINDER,
	ENFORCEDCVAR_SV_CQ_TRIM_BLOAT_SPACE,
	ENFORCEDCVAR_SV_CQ_TRIM_CATCHUP_REMAINDER,
	ENFORCEDCVAR_SV_RUNCMDS,
	ENFORCEDCVAR_COUNT
};

// clang-format off
static_global const char* enforcedServerCVars[] = 
{
	"sv_cheats", 
	"mp_drop_knife_enable",
	"sv_turbophysics",
    "cq_enable",
    "cq_buffer_bloat_msecs_max",
    "cq_dilation_percentage",
    "sv_cq_min_queue",
	"sv_condense_late_buttons",
	"sv_late_commands_allowed",
	"cq_max_starved_substitute_commands",
	"sv_cq_trim_bloat_remainder",
	"sv_cq_trim_bloat_space",
	"sv_cq_trim_catchup_remainder",
	"sv_runcmds"
};
static_assert(Q_ARRAYSIZE(enforcedServerCVars) == ENFORCEDCVAR_COUNT, "Array enforcedServerCVars length is not the same as ENFORCEDCVAR_COUNT!");

// clang-format on

i64 originalFlags[ENFORCEDCVAR_COUNT];

void KZGlobalService::EnforceConVars()
{
	for (u32 i = 0; i < ENFORCEDCVAR_COUNT; i++)
	{
		ConVarHandle cvarHandle = g_pCVar->FindConVar(enforcedServerCVars[i]);
		if (!cvarHandle.IsValid())
		{
			META_CONPRINTF("Failed to find %s!\n", enforcedServerCVars[i]);
			continue;
		}

		ConVar *cvar = g_pCVar->GetConVar(cvarHandle);
		assert(cvar);
		originalFlags[i] = cvar->flags;
		cvar->flags |= FCVAR_CHEAT;
	}
	// Revert all cvars to default if it hasn't been the case yet
	interfaces::pEngine->ServerCommand("sv_cheats 0");
}

void KZGlobalService::RestoreConVars()
{
	for (u32 i = 0; i < ENFORCEDCVAR_COUNT; i++)
	{
		ConVarHandle cvarHandle = g_pCVar->FindConVar(enforcedServerCVars[i]);
		if (!cvarHandle.IsValid())
		{
			META_CONPRINTF("Failed to find %s!\n", enforcedServerCVars[i]);
			continue;
		}

		ConVar *cvar = g_pCVar->GetConVar(cvarHandle);
		assert(cvar);
		cvar->flags = originalFlags[i];
	}
}
