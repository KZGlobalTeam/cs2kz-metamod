#include "../kz.h"
#include "cs2kz.h"
#include "kz_hud.h"
#include "sdk/datatypes.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"

#include "kz/option/kz_option.h"
#include "kz/hud/layout/layout.h"
#include "kz/timer/kz_timer.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/language/kz_language.h"
#include "kz/replays/kz_replaysystem.h"
#include "sdk/entity/ccscustomhudlayout.h"

#include <vendor/MultiAddonManager/public/imultiaddonmanager.h>
extern IMultiAddonManager *g_pMultiAddonManager;

#include "tier0/memdbgon.h"

static_global CConVar<bool> kz_force_mhud("kz_force_mhud", FCVAR_NONE, "Force the MHUD layout even when MultiAddonManager is not available.", false);

static_global class KZTimerServiceEventListener_HUD : public KZTimerServiceEventListener
{
	virtual void OnTimerStopped(KZPlayer *player, u32 courseGUID) override;
	virtual void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed) override;
} timerEventListener;

static_global class KZOptionServiceEventListener_HUD : public KZOptionServiceEventListener
{
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player)
	{
		player->hudService->InvalidatePrefs();
		player->hudService->ResetShowPanel();
	}

	virtual void OnPlayerPreferenceChanged(KZPlayer *player, const char *optionName)
	{
		player->hudService->InvalidatePrefs();
	}
} optionEventListener;

void KZHUDService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
	KZOptionService::RegisterEventListener(&optionEventListener);
	KZHUDService::RegisterMenu();

	// A server launched with -addon mhud is already serving the HUD addon itself, so the layout
	// resolves client side with no MultiAddonManager in the picture.
	const char *addons = CommandLine()->ParmValue("-addon", "");
	if (addons && V_stristr(addons, "mhud"))
	{
		kz_force_mhud.Set(true);
	}
}

bool KZHUDService::IsLayoutHudAvailable()
{
	return g_pMultiAddonManager != nullptr || kz_force_mhud.Get();
}

void KZHUDService::OnProcessMovementPost()
{
	if (this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND)
	{
		fromDuckbug = false;
	}
	if (this->player->GetMoveType() == MOVETYPE_LADDER)
	{
		fromDuckbug = false;
		crouchJumping = false;
	}
}

void KZHUDService::Reset()
{
	this->showPanel = this->player->optionService->GetPreferenceBool("showPanel", true);
	this->timerStoppedTime = {};
	this->currentTimeWhenTimerStopped = {};
	this->jumpedThisTick = false;
	this->fromDuckbug = false;
	this->crouchJumping = false;
}

KZHUDService::SpeedInfo KZHUDService::GetSpeedInfo()
{
	SpeedInfo info {};
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn || !pawn->IsAlive())
	{
		// Probably in spectator mode.
		CCSPlayerPawnBase *observer = this->player->GetObserverPawn();
		if (observer)
		{
			info.speed = observer->m_vecAbsVelocity().Length2D();
		}
		return info;
	}
	Vector velocity, baseVelocity;
	this->player->GetVelocity(&velocity);
	this->player->GetBaseVelocity(&baseVelocity);
	velocity += baseVelocity;
	info.speed = velocity.Length2D();
	// Keep the takeoff velocity on for a while after landing so the speed values flicker less.
	if ((this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND
		 && g_pKZUtils->GetServerGlobals()->curtime - this->player->landingTime > KZ_HUD_ON_GROUND_THRESHOLD)
		|| (this->player->GetPlayerPawn()->m_MoveType == MOVETYPE_LADDER && !player->IsButtonPressed(IN_JUMP)))
	{
		return info;
	}
	info.showTakeoff = true;
	info.takeoffSpeed = this->player->takeoffVelocity.Length2D();
	info.crouchJump = this->crouchJumping;
	info.walkedOff = !this->player->jumped && !this->player->takeoffFromLadder;
	if (this->player->IsPerfing() && !this->player->possibleLadderHop && !this->player->takeoffFromLadder)
	{
		info.jumpbug = this->fromDuckbug;
		info.perf = !this->fromDuckbug;
	}
	return info;
}

CPlayer_MovementServices *KZHUDService::GetHudMoveServices()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (pawn && pawn->IsAlive())
	{
		return this->player->GetMoveServices();
	}
	// Dead or spectating.
	CCSPlayerPawnBase *observer = this->player->GetObserverPawn();
	return observer ? observer->m_pMovementServices() : nullptr;
}

std::string KZHUDService::GetTimerText(const char *language)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		char timeText[128];

		f64 time = KZ::replaysystem::GetTime();
		bool paused = KZ::replaysystem::GetPaused();
		bool timerRunning = KZ::replaysystem::GetEndTime() == 0.0f;
		// Show timer if time is not 0 or end time is not 0.
		if (time == 0.0f && KZ::replaysystem::GetEndTime() == 0.0f)
		{
			return std::string("");
		}
		if (!timerRunning)
		{
			time = KZ::replaysystem::GetEndTime();
		}
		utils::FormatTime(time, timeText, sizeof(timeText));
		// clang-format off
		return KZLanguageService::PrepareMessageWithLang(language, "HUD - Timer Text",
			timeText,
			timerRunning ? "" : KZLanguageService::PrepareMessageWithLang(language, "HUD - Stopped Text").c_str(),
			paused ? KZLanguageService::PrepareMessageWithLang(language, "HUD - Paused Text").c_str() : ""
		);
		// clang-format on
	}
	if (this->player->timerService->GetTimerRunning() || this->ShouldShowTimerAfterStop())
	{
		char timeText[128];

		// clang-format off
		f64 time = this->player->timerService->GetTimerRunning()
				? player->timerService->GetTime()
				: this->currentTimeWhenTimerStopped;
		bool timerRunning = this->player->timerService->GetTimerRunning();
		bool paused = this->player->timerService->GetPaused();

		utils::FormatTime(time, timeText, sizeof(timeText));
		return KZLanguageService::PrepareMessageWithLang(language, "HUD - Timer Text",
			timeText,
			timerRunning ? "" : KZLanguageService::PrepareMessageWithLang(language, "HUD - Stopped Text").c_str(),
			paused ? KZLanguageService::PrepareMessageWithLang(language, "HUD - Paused Text").c_str() : ""
		);
		// clang-format on
	}
	return std::string("");
}

std::string KZHUDService::GetCheckpointText(const char *language)
{
	// clang-format off
	return KZLanguageService::PrepareMessageWithLang(language, "HUD - Checkpoint Text",
		KZ::replaysystem::IsReplayBot(this->player) ? KZ::replaysystem::GetCurrentCpIndex() : this->player->checkpointService->GetCurrentCpIndex(),
		KZ::replaysystem::IsReplayBot(this->player) ? KZ::replaysystem::GetCheckpointCount() : this->player->checkpointService->GetCheckpointCount(),
		KZ::replaysystem::IsReplayBot(this->player) ? KZ::replaysystem::GetTeleportCount() : this->player->checkpointService->GetTeleportCount()
	);
	// clang-format on
}

void KZHUDService::DrawPanels(KZPlayer *player, KZPlayer *target)
{
	if (target->IsFakeClient())
	{
		return;
	}
	target->hudService->UpdateHudLayout(player);

	KZHUDService::DrawLegacyPanels(player, target);
}

void KZHUDService::ResetShowPanel()
{
	this->showPanel = this->player->optionService->GetPreferenceBool("showPanel", true);
}

void KZHUDService::TogglePanel()
{
	this->showPanel = !this->showPanel;
	this->player->optionService->SetPreferenceBool("showPanel", this->showPanel);
	if (!this->showPanel)
	{
		utils::PrintAlert(this->player->GetController(), "#SFUI_EmptyString");
		utils::PrintCentre(this->player->GetController(), "#SFUI_EmptyString");
		this->player->languageService->PrintHTMLCentre(false, false, "HUD - HTML Panel Disabled");
	}
}

void KZHUDService::OnTimerStopped(f64 currentTimeWhenTimerStopped)
{
	// g_pKZUtils->GetServerGlobals() becomes invalid when the plugin is unloading.
	if (g_KZPlugin.unloading)
	{
		return;
	}
	this->timerStoppedTime = g_pKZUtils->GetServerGlobals()->curtime;
	this->currentTimeWhenTimerStopped = currentTimeWhenTimerStopped;
}

void KZTimerServiceEventListener_HUD::OnTimerStopped(KZPlayer *player, u32 courseGUID)
{
	player->hudService->OnTimerStopped(player->timerService->GetTime());
}

void KZTimerServiceEventListener_HUD::OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed)
{
	player->hudService->OnTimerStopped(time);
}

bool KZHUDService::IsCompactPanel()
{
	return this->GetPrefs().compactPanel;
}

void KZHUDService::ToggleCompactPanel()
{
	this->player->optionService->SetPreferenceBool("compactPanel", !this->IsCompactPanel());
}

SCMD(kz_panel, SCFL_HUD)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (args->ArgC() >= 2)
	{
		if (KZ_STREQI(args->Arg(1), "compact"))
		{
			player->hudService->ToggleCompactPanel();
			if (player->hudService->IsCompactPanel())
			{
				player->languageService->PrintChat(true, false, "HUD Option - Compact Panel - Enable");
			}
			else
			{
				player->languageService->PrintChat(true, false, "HUD Option - Compact Panel - Disable");
			}
			return MRES_SUPERCEDE;
		}
		else
		{
			player->languageService->PrintChat(true, false, "Panel Command Usage");
			return MRES_SUPERCEDE;
		}
	}
	player->hudService->TogglePanel();
	if (player->hudService->IsShowingPanel())
	{
		player->languageService->PrintChat(true, false, "HUD Option - Info Panel - Enable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "HUD Option - Info Panel - Disable");
	}
	return MRES_SUPERCEDE;
}
