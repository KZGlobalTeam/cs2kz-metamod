#include "common.h"
#include "utils/utils.h"
#include "kz/kz.h"
#include "utils/simplecmds.h"

#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/quiet/kz_quiet.h"
#include "kz/mode/kz_mode.h"
#include "kz/language/kz_language.h"
#include "kz/style/kz_style.h"
#include "kz/noclip/kz_noclip.h"
#include "kz/hud/kz_hud.h"
#include "kz/spec/kz_spec.h"
#include "kz/timer/kz_timer.h"
#include "kz/tip/kz_tip.h"

static_function SCMD_CALLBACK(Command_KzHidelegs)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->ToggleHideLegs();
	if (player->HidingLegs())
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Player Legs - Disable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Player Legs - Enable");
	}
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzHide)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->quietService->ToggleHide();
	if (player->quietService->hideOtherPlayers)
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Players - Disable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Players - Enable");
	}
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzRestart)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->timerService->OnTeleportToStart();
	if (player->GetPlayerPawn()->IsAlive())
	{
		// Fix players spawning 500u under spawn positions.
		if (player->noclipService->IsNoclipping())
		{
			player->noclipService->DisableNoclip();
			player->noclipService->HandleNoclip();
		}
		player->GetPlayerPawn()->Respawn();
	}
	else
	{
		controller->Respawn();
	}
	if (player->checkpointService->HasCustomStartPosition())
	{
		player->checkpointService->TpToStartPosition();
	}
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzHideWeapon)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->quietService->ToggleHideWeapon();
	if (player->quietService->ShouldHideWeapon())
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Weapon - Disable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Weapon - Enable");
	}
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_JoinTeam)
{
	KZ::misc::JoinTeam(g_pKZPlayerManager->ToPlayer(controller), atoi(args->Arg(1)), false);
	return MRES_SUPERCEDE;
}

void KZ::misc::OnServerActivate()
{
	static_persist bool infiniteAmmoUnlocked {};
	if (!infiniteAmmoUnlocked)
	{
		infiniteAmmoUnlocked = true;
		auto cvarHandle = g_pCVar->FindConVar("sv_infinite_ammo");
		if (cvarHandle.IsValid())
		{
			g_pCVar->GetConVar(cvarHandle)->flags &= ~FCVAR_CHEAT;
		}
		else
		{
			META_CONPRINTF("Warning: sv_infinite_ammo is not found!\n");
		}
	}
	g_pKZUtils->UpdateCurrentMapMD5();
	interfaces::pEngine->ServerCommand("exec cs2kz.cfg");
}

// TODO: move command registration to the service class?
void KZ::misc::RegisterCommands()
{
	scmd::RegisterCmd("kz_hidelegs", Command_KzHidelegs);
	scmd::RegisterCmd("kz_hide", Command_KzHide);
	scmd::RegisterCmd("kz_restart", Command_KzRestart);
	scmd::RegisterCmd("kz_r", Command_KzRestart);
	scmd::RegisterCmd("kz_hideweapon", Command_KzHideWeapon);
	scmd::RegisterCmd("jointeam", Command_JoinTeam, true);
	// TODO: Fullupdate spectators on spec_mode/spec_next/spec_player/spec_prev
	KZCheckpointService::RegisterCommands();
	KZJumpstatsService::RegisterCommands();
	KZTimerService::RegisterCommands();
	KZNoclipService::RegisterCommands();
	KZHUDService::RegisterCommands();
	KZLanguageService::RegisterCommands();
	KZ::mode::RegisterCommands();
	KZ::style::RegisterCommands();
}

void KZ::misc::JoinTeam(KZPlayer *player, int newTeam, bool restorePos)
{
	int currentTeam = player->GetController()->GetTeam();

	// Don't use CS_TEAM_NONE
	if (newTeam == CS_TEAM_NONE)
	{
		newTeam = CS_TEAM_SPECTATOR;
	}

	if (newTeam == CS_TEAM_SPECTATOR && currentTeam != CS_TEAM_SPECTATOR)
	{
		if (currentTeam != CS_TEAM_NONE)
		{
			player->specService->SavePosition();
		}

		if (!player->timerService->GetPaused() && !player->timerService->CanPause())
		{
			player->timerService->TimerStop();
		}
		player->GetController()->ChangeTeam(CS_TEAM_SPECTATOR);
		player->quietService->SendFullUpdate();
		// TODO: put spectators of this player to freecam, and send them full updates
	}
	else if (newTeam == CS_TEAM_CT && currentTeam != CS_TEAM_CT || newTeam == CS_TEAM_T && currentTeam != CS_TEAM_T)
	{
		player->GetPlayerPawn()->CommitSuicide(false, true);
		player->GetController()->SwitchTeam(newTeam);
		player->GetController()->Respawn();
		if (restorePos && player->specService->HasSavedPosition())
		{
			player->specService->LoadPosition();
		}
		else
		{
			player->timerService->TimerStop();
			// Just joining a team alone can put you into weird invalid spawns.
			// Need to teleport the player to a valid one.
			Vector spawnOrigin;
			QAngle spawnAngles;
			utils::FindValidSpawn(spawnOrigin, spawnAngles);
			player->GetPlayerPawn()->Teleport(&spawnOrigin, &spawnAngles, &vec3_origin);
		}
		player->specService->ResetSavedPosition();
	}
}
