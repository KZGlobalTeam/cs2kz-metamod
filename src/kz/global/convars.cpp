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
	ENFORCEDCVAR_MP_IGNORE_ROUND_WIN_CONDITIONS,
	ENFORCEDCVAR_MP_MATCH_END_CHANGELEVEL,
	ENFORCEDCVAR_MP_MATCH_END_RESTART,
	ENFORCEDCVAR_SV_RUNCMDS,
	ENFORCEDCVAR_COUNT
};

// clang-format off
static_global ConVarRefAbstract *enforcedServerCVars[] = 
{
	new CConVarRef<bool>("sv_cheats"), 
	new CConVarRef<bool>("mp_drop_knife_enable"),
	new CConVarRef<bool>("sv_turbophysics"),
	new CConVarRef<bool>("cq_enable"),
	new CConVarRef<float32>("cq_buffer_bloat_msecs_max"),
	new CConVarRef<float32>("cq_dilation_percentage"),
	new CConVarRef<int32>("sv_cq_min_queue"),
	new CConVarRef<bool>("sv_condense_late_buttons"),
	new CConVarRef<int32>("sv_late_commands_allowed"),
	new CConVarRef<int32>("cq_max_starved_substitute_commands"),
	new CConVarRef<int32>("sv_cq_trim_bloat_remainder"),
	new CConVarRef<int32>("sv_cq_trim_bloat_space"),
	new CConVarRef<int32>("sv_cq_trim_catchup_remainder"),
	new CConVarRef<bool>("mp_ignore_round_win_conditions"),
	new CConVarRef<bool>("mp_match_end_changelevel"),
	new CConVarRef<bool>("mp_match_end_restart"),
	new CConVarRef<bool>("sv_runcmds")
};
static_assert(KZ_ARRAYSIZE(enforcedServerCVars) == ENFORCEDCVAR_COUNT, "Array enforcedServerCVars length is not the same as ENFORCEDCVAR_COUNT!");

// clang-format on

void KZGlobalService::EnforceConVars()
{
	for (u32 i = 0; i < ENFORCEDCVAR_COUNT; i++)
	{
		ConVarData *data = enforcedServerCVars[i]->GetConVarData();
		if (i == ENFORCEDCVAR_MP_MATCH_END_CHANGELEVEL)
		{
			data->UpdateDefaultValueString("true");
		}
		data->RemoveFlags(FCVAR_COMMANDLINE_ENFORCED);
		data->AddFlags(FCVAR_CHEAT);
	}

	g_pCVar->ResetConVarsToDefaultValuesByFlag(FCVAR_CHEAT);

	META_CONPRINTF("[KZ::Global] Enforced relevant ConVars.\n");
}

void KZGlobalService::RestoreConVars()
{
	for (u32 i = 0; i < ENFORCEDCVAR_COUNT; i++)
	{
		ConVarData *data = enforcedServerCVars[i]->GetConVarData();
		if (i == ENFORCEDCVAR_MP_MATCH_END_CHANGELEVEL)
		{
			data->UpdateDefaultValueString("false");
		}
		enforcedServerCVars[i]->GetConVarData()->RemoveFlags(FCVAR_CHEAT);
	}

	META_CONPRINTF("[KZ::Global] Restored relevant ConVars.\n");
}
