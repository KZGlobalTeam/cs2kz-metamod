#include "kz_telemetry.h"
#include "utils/simplecmds.h"
#include "kz/language/kz_language.h"
#include "sdk/usercmd.h"

#define AFK_THRESHOLD 30.0f
f64 KZTelemetryService::lastActiveCheckTime = 0.0f;

void KZTelemetryService::OnPhysicsSimulatePost()
{
	// AFK check
	if (!this->player->GetMoveServices())
	{
		return;
	}
	if (this->player->GetMoveServices()->m_nButtons()->m_pButtonStates[1] != 0
		|| this->player->GetMoveServices()->m_nButtons()->m_pButtonStates[2] != 0)
	{
		this->activeStats.lastActionTime = g_pKZUtils->GetServerGlobals()->realtime;
		return;
	}
}

void KZTelemetryService::ActiveCheck()
{
	f64 currentTime = g_pKZUtils->GetServerGlobals()->realtime;
	f64 duration = currentTime - KZTelemetryService::lastActiveCheckTime;
	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player->IsInGame() || player->IsFakeClient() || player->IsCSTV())
		{
			continue;
		}
		player->telemetryService->activeStats.timeSpentInServer += duration;
		if (player->IsAlive())
		{
			if (currentTime - player->telemetryService->activeStats.lastActionTime > AFK_THRESHOLD)
			{
				player->telemetryService->activeStats.afkDuration += duration;
			}
			else
			{
				player->telemetryService->activeStats.activeTime += duration;
			}
		}
	}
	KZTelemetryService::lastActiveCheckTime = currentTime;
}

SCMD_CALLBACK(Command_KzStats)
{
	return MRES_SUPERCEDE;
}

void KZTelemetryService::RegisterCommands()
{
	scmd::RegisterCmd("kz_stats", Command_KzStats);
}
