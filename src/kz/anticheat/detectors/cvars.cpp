#pragma once
#include "common.h"
#include "convar.h"
#include "kz_anticheat.h"

CConVarRef<bool> sv_cheats("sv_cheats");
// Checks for sv_cheats protected commands will only kick in after this delay
#define SV_CHEATS_MAX_PROPAGATION_DELAY 30.0f
static_global f64 cheatCvarCheckerGraceUntil = -1.0f;

bool ShouldEnforceCheatCvars()
{
	assert(sv_cheats.IsValidRef() && sv_cheats.IsConVarDataAvailable());
	if (sv_cheats.Get())
	{
		return false;
	}
	time_t unixTime = 0;
	time(&unixTime);
	return cheatCvarCheckerGraceUntil < 0 || (f64)unixTime > cheatCvarCheckerGraceUntil;
}

static_global void OnCvarChanged(ConVarRefAbstract *ref, CSplitScreenSlot nSlot, const char *pNewValue, const char *pOldValue, void *__unk01)
{
	assert(sv_cheats.IsValidRef() && sv_cheats.IsConVarDataAvailable());
	time_t unixTime = 0;
	time(&unixTime);
	if (!sv_cheats.Get())
	{
		cheatCvarCheckerGraceUntil = (f64)unixTime + SV_CHEATS_MAX_PROPAGATION_DELAY;
		return;
	}
}

void KZ::anticheat::InitCvarChecker()
{
	g_pCVar->InstallGlobalChangeCallback(OnCvarChanged);
	assert(sv_cheats.IsValidRef() && sv_cheats.IsConVarDataAvailable());
	if (!sv_cheats.Get())
	{
		time_t unixTime = 0;
		time(&unixTime);
		cheatCvarCheckerGraceUntil = (f64)unixTime + SV_CHEATS_MAX_PROPAGATION_DELAY;
	}
}

enum CVarCheckFlags : u32
{
	CVCHECK_NONE = 0u,
	CVCHECK_USERINFO = (1u << 0), // Convar is flagged as userinfo

	CVCHECK_CHEAT = (1u << 1),   // If sv_cheats is required to change this convar.
	CVCHECK_NODELTA = (1u << 2), // Convar isn't supposed to change over time
	CVCHECK_LOCKED = (1u << 3),  // These convars are not modifyable under normal circumstances (even with cheats on)

	CVCHECK_MIN_BOUNDED = (1u << 4), // Convar has min bounds to be enforced
	CVCHECK_MAX_BOUNDED = (1u << 5), // Convar has max bounds to be enforced
	CVCHECK_BAN = (1u << 6),         // Violation of this convar should result in a ban
};

struct ConVarCheck
{
	const char *convarName;
	EConVarType type;
	u32 flags;

	CVValue_t lockedValue = CVValue_t::InvalidValue();
	CVValue_t minValue = CVValue_t::InvalidValue();
	CVValue_t maxValue = CVValue_t::InvalidValue();
};

ConVarCheck cvarChecks[] = {
	{"fps_max", EConVarType_Float32, CVCHECK_MIN_BOUNDED | CVCHECK_NODELTA, CVValue_t::InvalidValue(), CVValue_t(64.0f)},
	{"sv_cheats", EConVarType_Bool, CVCHECK_CHEAT | CVCHECK_BAN, CVValue_t(false)},
	// clang-format off
	{"sensitivity", EConVarType_Float32, CVCHECK_USERINFO | CVCHECK_MIN_BOUNDED | CVCHECK_MAX_BOUNDED | CVCHECK_BAN, CVValue_t::InvalidValue(), CVValue_t(0.0001f), CVValue_t(8.0f)},
	{"m_yaw", EConVarType_Float32, CVCHECK_USERINFO | CVCHECK_MIN_BOUNDED | CVCHECK_MAX_BOUNDED, CVValue_t::InvalidValue(), CVValue_t(0.0f), CVValue_t(0.3f)},
	// clang-format on
	{"cl_showpos", EConVarType_Int32, CVCHECK_CHEAT | CVCHECK_BAN, CVValue_t(0)},
	{"cam_showangles", EConVarType_Bool, CVCHECK_CHEAT | CVCHECK_BAN, CVValue_t(false)},
	{"cl_drawhud", EConVarType_Bool, CVCHECK_CHEAT | CVCHECK_BAN},
	{"cl_pitchdown", EConVarType_Float32, CVCHECK_CHEAT | CVCHECK_BAN, CVValue_t(89.0f)},
	{"cl_pitchup", EConVarType_Float32, CVCHECK_CHEAT | CVCHECK_BAN, CVValue_t(89.0f)},
	{"cl_yawspeed", EConVarType_Float32, CVCHECK_LOCKED | CVCHECK_BAN},
};
