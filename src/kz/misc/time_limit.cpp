
#include "common.h"
#include "kz/kz.h"
#include "kz/option/kz_option.h"
#include "sdk/gamerules.h"
// Make sure that the server can't run for too long.

// Original value
static_global CVValue_t *mp_timelimit_cvvalue;
static_global CVValue_t *mp_roundtime_cvvalue;
static_global CVValue_t *mp_roundtime_defuse_cvvalue;
static_global CVValue_t *mp_roundtime_hostage_cvvalue;

// Original max value. mp_timelimit has no max value.
static_global CVValue_t *mp_roundtime_cvvalue_max;
static_global CVValue_t *mp_roundtime_defuse_cvvalue_max;
static_global CVValue_t *mp_roundtime_hostage_cvvalue_max;

static_global CVValue_t hardcodedTimeLimit(1440.0f);
static_global bool cvarLoaded = false;
CConVarRef<float> mp_timelimit("mp_timelimit");
CConVarRef<float> mp_roundtime("mp_roundtime");
CConVarRef<float> mp_roundtime_defuse("mp_roundtime_defuse");
CConVarRef<float> mp_roundtime_hostage("mp_roundtime_hostage");
CConVarRef<CUtlString> nextlevel("nextlevel");
ConVarRefAbstract *convars[] = {&mp_timelimit, &mp_roundtime, &mp_roundtime_defuse, &mp_roundtime_hostage};

static_global void OnCvarChanged(ConVarRefAbstract *ref, CSplitScreenSlot nSlot, const char *pNewValue, const char *pOldValue, void *__unk01)
{
	assert(mp_timelimit.IsValidRef() && mp_timelimit.IsConVarDataAvailable());
	assert(mp_roundtime.IsValidRef() && mp_roundtime.IsConVarDataAvailable());
	assert(mp_roundtime_defuse.IsValidRef() && mp_roundtime_defuse.IsConVarDataAvailable());
	assert(mp_roundtime_hostage.IsValidRef() && mp_roundtime_hostage.IsConVarDataAvailable());

	u16 refIndex = ref->GetAccessIndex();
	bool replicate = false;

	for (ConVarRefAbstract *cvar : convars)
	{
		if (cvar->GetAccessIndex() == refIndex)
		{
			replicate = true;
			break;
		}
	}
	if (!replicate)
	{
		return;
	}
	for (ConVarRefAbstract *cvar : convars)
	{
		if (!cvar->IsFlagSet(FCVAR_PERFORMING_CALLBACKS))
		{
			cvar->SetString(pNewValue);
		}
	}

	// Reflect the change in value to the HUD as well.
	CCSGameRules *gameRules = g_pKZUtils->GetGameRules();
	i32 newRoundTime = int(atof(pNewValue) * 60.0f);
	if (gameRules && gameRules->m_iRoundTime() != newRoundTime)
	{
		gameRules->m_iRoundTime(newRoundTime);
	}
}

void KZ::misc::EnforceTimeLimit()
{
	if (cvarLoaded || !mp_timelimit.IsValidRef() || !mp_roundtime.IsValidRef() || !mp_roundtime_defuse.IsValidRef()
		|| !mp_roundtime_hostage.IsValidRef())
	{
		return;
	}
	cvarLoaded = true;
	mp_timelimit_cvvalue = mp_timelimit.GetConVarData()->Value(-1);
	mp_roundtime_cvvalue = mp_roundtime.GetConVarData()->Value(-1);
	mp_roundtime_defuse_cvvalue = mp_roundtime_defuse.GetConVarData()->Value(-1);
	mp_roundtime_hostage_cvvalue = mp_roundtime_hostage.GetConVarData()->Value(-1);

	mp_roundtime_cvvalue_max = mp_roundtime.GetConVarData()->MaxValue();
	mp_roundtime_defuse_cvvalue_max = mp_roundtime_defuse.GetConVarData()->MaxValue();
	mp_roundtime_hostage_cvvalue_max = mp_roundtime_hostage.GetConVarData()->MaxValue();

	mp_timelimit.GetConVarData()->SetMaxValue(&hardcodedTimeLimit);
	mp_roundtime.GetConVarData()->SetMaxValue(&hardcodedTimeLimit);
	mp_roundtime_defuse.GetConVarData()->SetMaxValue(&hardcodedTimeLimit);
	mp_roundtime_hostage.GetConVarData()->SetMaxValue(&hardcodedTimeLimit);
	nextlevel.Set(g_pKZUtils->GetCurrentMapName());
	g_pCVar->InstallGlobalChangeCallback(OnCvarChanged);
}

void KZ::misc::UnrestrictTimeLimit()
{
	if (!cvarLoaded)
	{
		return;
	}

	mp_timelimit.GetConVarData()->RemoveMaxValue();
	mp_roundtime.GetConVarData()->SetMaxValue(mp_roundtime_cvvalue_max);
	mp_roundtime_defuse.GetConVarData()->SetMaxValue(mp_roundtime_defuse_cvvalue_max);
	mp_roundtime_hostage.GetConVarData()->SetMaxValue(mp_roundtime_hostage_cvvalue_max);
	g_pCVar->RemoveGlobalChangeCallback(OnCvarChanged);
}

void KZ::misc::InitTimeLimit()
{
	mp_timelimit.Set(KZOptionService::GetOptionFloat("defaultTimeLimit", 60.0f));
}
