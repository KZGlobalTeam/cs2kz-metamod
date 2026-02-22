/*
	Detector for invalid cvar values on clients.
	Some of them will only result in kicks, while the ones that can only be changed via cheats will result in bans.
*/
#include "common.h"
#include "convar.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"
#include "utils/ctimer.h"

#include <vendor/ClientCvarValue/public/iclientcvarvalue.h>

CConVarRef<bool> sv_cheats("sv_cheats");

// Checks for sv_cheats protected commands will only kick in after this delay

bool KZAnticheatService::ShouldRunDetections() const
{
	if (!sv_cheats.IsValidRef() || !sv_cheats.IsConVarDataAvailable())
	{
		return true;
	}
	return !sv_cheats.Get();
}

extern IClientCvarValue *g_pClientCvarValue;

#define INTEGRITY_CHECK_MIN_INTERVAL    1.0f
#define INTEGRITY_CHECK_MAX_INTERVAL    5.0f
#define KICK_DELAY                      5.0f
#define MINIMUM_FPS_MAX                 64.0f
#define MAXIMUM_M_YAW                   0.3f
#define SV_CHEATS_MAX_PROPAGATION_DELAY 30.0f

static_global const char *cvarNames[] = {
	"m_yaw",          // not capped, but should kick if it goes out of bounds
	"fps_max",        // This can never change during gameplay, also should kick if it goes under 64
	"sv_cheats",      // replicated
	"sensitivity",    // capped (0.0001f => 8.0f)
	"cl_showpos",     // cheat (default 0)
	"cam_showangles", // cheat (default 0)
	"cl_drawhud",     // cheat (default 1)
	"cl_pitchdown",   // should always be 89.0f
	"cl_pitchup",     // should always be 89.0f
	"cl_yawspeed",    // should always be 210.0f
	"fov_cs_debug"    // cheat (default 0)
};

static_global const char *userInfoCvarNames[] = {
	"sensitivity",
	"m_yaw",
};

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

void KZAnticheatService::InitSvCheatsWatcher()
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

void KZAnticheatService::CleanupSvCheatsWatcher()
{
	g_pCVar->RemoveGlobalChangeCallback(OnCvarChanged);
}

f64 KZAnticheatService::KickPlayerInvalidSettings(CPlayerUserId userID)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	if (player)
	{
		player->Kick("Invalid player settings");
	}
	return 0.0f;
}

static_function void ValidateQueriedCvar(CPlayerSlot nSlot, ECvarValueStatus eStatus, const char *pszCvarName, const char *pszCvarValue)
{
	if (eStatus != ECvarValueStatus::ValueIntact)
	{
		META_CONPRINTF("Warning: Could not retrieve cvar value for player slot %d cvar %s, status %d\n", nSlot.Get(), pszCvarName, (int)eStatus);
		return;
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(nSlot);
	if (!player || !player->IsInGame() || !player->anticheatService->ShouldCheckClientCvars())
	{
		return;
	}
	if (KZ_STREQI(pszCvarName, "m_yaw"))
	{
		if (atof(pszCvarValue) > MAXIMUM_M_YAW)
		{
			player->languageService->PrintChat(true, false, "Kick Player m_yaw");
			player->languageService->PrintConsole(false, false, "Kick Player m_yaw (Console)");
			player->anticheatService->MarkHasInvalidCvars();
			player->timerService->TimerStop();
			StartTimer<CPlayerUserId>(KZAnticheatService::KickPlayerInvalidSettings, player->GetClient()->GetUserID(), KICK_DELAY, true, true);
		}
	}
	else if (KZ_STREQI(pszCvarName, "fps_max"))
	{
		f64 fps = atof(pszCvarValue);
		if (player->anticheatService->currentMaxFps == 0.0f)
		{
			player->anticheatService->currentMaxFps = atof(pszCvarValue);
		}

		if (fps > 0.0f && fps < MINIMUM_FPS_MAX)
		{
			player->languageService->PrintChat(true, false, "Kick Player fps_max");
			player->languageService->PrintConsole(false, false, "Kick Player fps_max (Console)");
			player->anticheatService->MarkHasInvalidCvars();
			player->timerService->TimerStop();
			StartTimer<CPlayerUserId>(KZAnticheatService::KickPlayerInvalidSettings, player->GetClient()->GetUserID(), KICK_DELAY, true, true);
		}
		else if (fps != player->anticheatService->currentMaxFps)
		{
			// Player shouldn't be able to normally change fps_max during gameplay, so they must be using a cheat.
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = "Changed fps_max from " + std::to_string(player->anticheatService->currentMaxFps) + " to " + std::to_string(fps);
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
	else if (KZ_STREQI(pszCvarName, "sv_cheats"))
	{
		if (!ShouldEnforceCheatCvars())
		{
			return;
		}
		// Cvar should have been turned off for the client, but isn't.
		// It's possible that they might have a very, very bad internet, but it's most likely a cheat.
		if (KZ_STREQI(pszCvarValue, "1") || KZ_STREQI(pszCvarValue, "true"))
		{
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = "sv_cheats is enabled on client despite being disabled on server";
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
	else if (KZ_STREQI(pszCvarName, "sensitivity"))
	{
		f32 sens = atof(pszCvarValue);
		// The actual upper bound is 8.0f but this is to account for possible future updates.
		if (sens < 0.0001f || sens > 20.0f)
		{
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = "Invalid sensitivity value: " + std::string(pszCvarValue);
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
	else if (KZ_STREQI(pszCvarName, "cl_showpos") || KZ_STREQI(pszCvarName, "cam_showangles"))
	{
		if (!ShouldEnforceCheatCvars())
		{
			return;
		}
		if (!KZ_STREQI(pszCvarValue, "0") && !KZ_STREQI(pszCvarValue, "false"))
		{
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = std::string(pszCvarName) + " is enabled on client despite sv_cheats being disabled on server";
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
	else if (KZ_STREQI(pszCvarName, "cl_drawhud"))
	{
		if (!ShouldEnforceCheatCvars())
		{
			return;
		}
		if (KZ_STREQI(pszCvarValue, "0") || KZ_STREQI(pszCvarValue, "false"))
		{
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = "cl_drawhud is disabled on client despite sv_cheats being disabled on server";
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
	else if (KZ_STREQI(pszCvarName, "fov_cs_debug"))
	{
		if (!ShouldEnforceCheatCvars())
		{
			return;
		}
		f64 fovValue = atof(pszCvarValue);
		if (fovValue != 0.0f)
		{
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = "fov_cs_debug is enabled on client despite sv_cheats being disabled on server";
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
	else if (KZ_STREQI(pszCvarName, "cl_pitchdown"))
	{
		if (atof(pszCvarValue) != 89.0f)
		{
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = "cl_pitchdown has invalid value: " + std::string(pszCvarValue);
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
	else if (KZ_STREQI(pszCvarName, "cl_pitchup"))
	{
		if (atof(pszCvarValue) != 89.0f)
		{
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = "cl_pitchup has invalid value: " + std::string(pszCvarValue);
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
	else if (KZ_STREQI(pszCvarName, "cl_yawspeed"))
	{
		if (atof(pszCvarValue) != 210.0f)
		{
			player->anticheatService->MarkHasInvalidCvars();
			std::string reason = "cl_yawspeed has invalid value: " + std::string(pszCvarValue);
			player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
		}
	}
}

static_function f64 CheckUserInfoCvars(KZPlayer *player)
{
	if (!player->anticheatService->ShouldRunDetections())
	{
		return RandomFloat(INTEGRITY_CHECK_MIN_INTERVAL, INTEGRITY_CHECK_MAX_INTERVAL);
	}
	for (auto &name : userInfoCvarNames)
	{
		const char *value = interfaces::pEngine->GetClientConVarValue(player->GetPlayerSlot(), name);
		if (value == nullptr || KZ_STREQI(value, ""))
		{
			continue;
		}
		if (KZ_STREQI(name, "m_yaw"))
		{
			if (atof(value) > MAXIMUM_M_YAW || !utils::IsNumeric(value))
			{
				player->languageService->PrintChat(true, false, "Kick Player m_yaw");
				player->languageService->PrintConsole(false, false, "Kick Player m_yaw (Console)");
				player->anticheatService->MarkHasInvalidCvars();
				player->timerService->TimerStop();
				StartTimer<CPlayerUserId>(KZAnticheatService::KickPlayerInvalidSettings, player->GetClient()->GetUserID(), KICK_DELAY, true, true);
				return 0.0f;
			}
		}
		else if (KZ_STREQI(name, "sensitivity"))
		{
			f32 sens = atof(value);
			// The actual upper bound is 8.0f but this is to account for possible future updates.
			if (sens < 0.0001f || sens > 20.0f)
			{
				player->anticheatService->MarkHasInvalidCvars();
				std::string reason = "Invalid sensitivity value: " + std::string(value);
				player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::InvalidCvar, reason);
				return 0.0f;
			}
		}
	}
	return RandomFloat(INTEGRITY_CHECK_MIN_INTERVAL, INTEGRITY_CHECK_MAX_INTERVAL);
}

static_function f64 CheckClientCvars(CPlayerUserId userID)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	if (!player || !player->IsInGame())
	{
		return 0.0f;
	}
	if (!player->anticheatService->ShouldRunDetections())
	{
		return RandomFloat(INTEGRITY_CHECK_MIN_INTERVAL, INTEGRITY_CHECK_MAX_INTERVAL);
	}
	if (CheckUserInfoCvars(player) == 0.0f)
	{
		return 0.0f;
	}
	if (!g_pClientCvarValue || !player->anticheatService->ShouldCheckClientCvars())
	{
		return 0.0f;
	}
	for (auto &name : cvarNames)
	{
		g_pClientCvarValue->QueryCvarValue(player->GetPlayerSlot(), name, ValidateQueriedCvar);
	}
	return RandomFloat(INTEGRITY_CHECK_MIN_INTERVAL, INTEGRITY_CHECK_MAX_INTERVAL);
}

void KZAnticheatService::InitCvarMonitor()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	StartTimer<CPlayerUserId>(CheckClientCvars, this->player->GetClient()->GetUserID(),
							  RandomFloat(INTEGRITY_CHECK_MIN_INTERVAL, INTEGRITY_CHECK_MAX_INTERVAL), true, true);
}
