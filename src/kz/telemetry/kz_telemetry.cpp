#include "kz_telemetry.h"
#include "kz/anticheat/kz_anticheat.h"
#include "utils/simplecmds.h"
#include "kz/language/kz_language.h"
#include "sdk/usercmd.h"

#define AFK_THRESHOLD 30.0f
f64 KZTelemetryService::lastActiveCheckTime = 0.0f;

void KZTelemetryService::OnJumpModern()
{
	this->preOutWishVel = this->player->currentMoveData->m_outWishVel;
	CCSPlayerModernJump &modernJump = this->player->GetMoveServices()->m_ModernJump();
	f32 lastUsableJumpPress = modernJump.m_nLastUsableJumpPressTick() + modernJump.m_flLastUsableJumpPressFrac();
	f32 modernLandTime = modernJump.m_nLastLandedTick() + modernJump.m_flLastLandedFrac();

	this->mightBeModernPerf = lastUsableJumpPress < modernLandTime;
}

void KZTelemetryService::OnJumpModernPost()
{
	// Didn't jump.
	if (this->preOutWishVel == this->player->currentMoveData->m_outWishVel)
	{
		return;
	}

	this->player->anticheatService->OnJump();
	// Not a bhop attempt if more than 100ms have passed since landing.
	if (g_pKZUtils->GetServerGlobals()->curtime - this->player->landingTimeServer > 0.1f)
	{
		return;
	}

	// Don't count jumps when autobhop is active.
	if (this->player->GetCvarValueFromModeStyles("sv_autobunnyhopping")->m_bValue)
	{
		return;
	}

	if (!this->player->inPerf)
	{
		this->modernBhopStats.numNonPerfs++;
	}
	else
	{
		if (this->player->IsPerfing(true) && !this->mightBeModernPerf)
		{
			this->modernBhopStats.numTruePerfs++;
		}
		else
		{
			this->mightBeModernPerf ? this->modernBhopStats.numModernPerfsEarly++ : this->modernBhopStats.numModernPerfsLate++;
		}
	}
}

void KZTelemetryService::OnJumpLegacy()
{
	this->preOutWishVel = this->player->currentMoveData->m_outWishVel;
}

void KZTelemetryService::OnJumpLegacyPost()
{
	// Didn't jump.
	if (this->preOutWishVel == this->player->currentMoveData->m_outWishVel)
	{
		return;
	}

	this->player->anticheatService->OnJump();
	// Not a bhop attempt if more than 100ms have passed since landing.
	if (g_pKZUtils->GetServerGlobals()->curtime - this->player->landingTimeServer > 0.1f)
	{
		return;
	}

	// Don't count jumps when autobhop is active.
	if (this->player->GetCvarValueFromModeStyles("sv_autobunnyhopping")->m_bValue)
	{
		return;
	}

	if (this->player->IsPerfing(true))
	{
		this->legacyBhopStats.numPerfs++;
	}
	else
	{
		this->legacyBhopStats.numNonPerfs++;
	}
}

void KZTelemetryService::PrintLegacyBhopStats()
{
	// Bhops: 200 | 25% Perf (50)
	this->player->languageService->PrintChat(true, false, "Telemetry - Legacy Bhop Stats", this->legacyBhopStats.GetTotalJumps(),
											 this->legacyBhopStats.GetPerfRatio() * 100.0f, this->legacyBhopStats.numPerfs);
}

void KZTelemetryService::PrintModernBhopStats()
{
	// Bhops: 200 | 25% True (50) | 10% Modern (20)
	auto &modernStats = this->modernBhopStats;
	f32 truePerfPercent = 0;
	f32 modernPerfEarlyPercent = 0;
	f32 modernPerfLatePercent = 0;
	if (modernStats.GetTotalJumps() > 0)
	{
		truePerfPercent = (f32)modernStats.numTruePerfs / (f32)modernStats.GetTotalJumps() * 100.0f;
		modernPerfEarlyPercent = (f32)modernStats.numModernPerfsEarly / (f32)modernStats.GetTotalJumps() * 100.0f;
		modernPerfLatePercent = (f32)modernStats.numModernPerfsLate / (f32)modernStats.GetTotalJumps() * 100.0f;
	}
	this->player->languageService->PrintChat(true, false, "Telemetry - Modern Bhop Stats", modernStats.GetTotalJumps(), truePerfPercent,
											 modernStats.numTruePerfs, modernPerfEarlyPercent, modernStats.numModernPerfsEarly, modernPerfLatePercent,
											 modernStats.numModernPerfsLate);
}

void KZTelemetryService::OnPhysicsSimulatePost()
{
	// AFK check
	if (!this->player->GetMoveServices())
	{
		return;
	}
	if (this->player->GetMoveServices()->m_nButtons().m_pButtonStates[1] != 0
		|| this->player->GetMoveServices()->m_nButtons().m_pButtonStates[2] != 0)
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

SCMD(kz_modernperfstats, SCFL_PLAYER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->telemetryService->PrintModernBhopStats();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_mps, kz_modernperfstats);

SCMD(kz_legacyperfstats, SCFL_PLAYER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->telemetryService->PrintLegacyBhopStats();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_lps, kz_legacyperfstats);

SCMD(kz_resetperfstats, SCFL_PLAYER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->telemetryService->ResetModernBhopStats();
	player->telemetryService->ResetLegacyBhopStats();
	player->languageService->PrintChat(true, false, "Telemetry - Reset Bhop Stats");
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_rps, kz_resetperfstats);
