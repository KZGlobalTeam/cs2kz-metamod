#include "../kz.h"
#include "kz_anticheat.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"
#include "utils/ctimer.h"

#include <vendor/ClientCvarValue/public/iclientcvarvalue.h>

extern IClientCvarValue *g_pClientCvarValue;

#define INTEGRITY_CHECK_MIN_INTERVAL 1.0f
#define INTEGRITY_CHECK_MAX_INTERVAL 5.0f
#define KICK_DELAY                   5.0f
#define MINIMUM_FPS_MAX              64.0f
#define MAXIMUM_M_YAW                0.3f

static_function f64 KickPlayerInvalidSettings(CPlayerUserId userID)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	if (player)
	{
		player->Kick("Invalid player settings");
	}
	return 0.0f;
}

static_function void ValidateCvar(CPlayerSlot nSlot, ECvarValueStatus eStatus, const char *pszCvarName, const char *pszCvarValue)
{
	if (eStatus != ECvarValueStatus::ValueIntact)
	{
		return;
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(nSlot);
	if (KZ_STREQI(pszCvarName, "m_yaw"))
	{
		if (atof(pszCvarValue) > MAXIMUM_M_YAW)
		{
			player->languageService->PrintChat(true, false, "Kick Player m_yaw");
			player->languageService->PrintConsole(false, false, "Kick Player m_yaw (Console)");
			player->anticheatService->MarkHasInvalidCvars();
			player->timerService->TimerStop();
			StartTimer<CPlayerUserId>(KickPlayerInvalidSettings, player->GetClient()->GetUserID(), KICK_DELAY, true, true);
		}
	}
	else if (KZ_STREQI(pszCvarName, "fps_max"))
	{
		if (atof(pszCvarValue) != 0.0f && atof(pszCvarValue) < MINIMUM_FPS_MAX)
		{
			player->languageService->PrintChat(true, false, "Kick Player fps_max");
			player->languageService->PrintConsole(false, false, "Kick Player fps_max (Console)");
			player->anticheatService->MarkHasInvalidCvars();
			player->timerService->TimerStop();
			StartTimer<CPlayerUserId>(KickPlayerInvalidSettings, player->GetClient()->GetUserID(), KICK_DELAY, true, true);
		}
	}
}

static_function f64 CheckClientCvars(CPlayerUserId userID)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	if (!player || !g_pClientCvarValue || !player->anticheatService->ShouldCheckClientCvars())
	{
		return 0.0f;
	}
	g_pClientCvarValue->QueryCvarValue(player->GetPlayerSlot(), "m_yaw", ValidateCvar);
	g_pClientCvarValue->QueryCvarValue(player->GetPlayerSlot(), "fps_max", ValidateCvar);
	return RandomFloat(INTEGRITY_CHECK_MIN_INTERVAL, INTEGRITY_CHECK_MAX_INTERVAL);
}

void KZAnticheatService::OnPlayerFullyConnect()
{
	this->hasValidCvars = true;
	StartTimer<CPlayerUserId>(CheckClientCvars, this->player->GetClient()->GetUserID(),
							  RandomFloat(INTEGRITY_CHECK_MIN_INTERVAL, INTEGRITY_CHECK_MAX_INTERVAL), true, true);
}

void KZAnticheatService::OnSetupMove(PlayerCommand *pc)
{
    strafeOptDetector.DetectOptimization(this->player, pc);
}
