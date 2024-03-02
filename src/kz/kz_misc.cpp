#include "common.h"
#include "utils/utils.h"
#include "kz.h"
#include "utils/simplecmds.h"

#include "checkpoint/kz_checkpoint.h"
#include "jumpstats/kz_jumpstats.h"
#include "quiet/kz_quiet.h"
#include "mode/kz_mode.h"
#include "style/kz_style.h"
#include "hud/kz_hud.h"

internal SCMD_CALLBACK(Command_KzNoclip)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->ToggleNoclip();
	utils::CPrintChat(player->GetPawn(), "%s {grey}Noclip %s.", KZ_CHAT_PREFIX, player->IsNoclipping() ? "enabled" : "disabled");
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHidelegs)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->ToggleHideLegs();
	utils::CPrintChat(player->GetPawn(), "%s {grey}Player legs are now %s.", KZ_CHAT_PREFIX, player->HidingLegs() ? "hidden" : "shown");
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHide)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->quietService->ToggleHide();
	utils::CPrintChat(player->GetPawn(), "%s {grey}You are now %s other players.", KZ_CHAT_PREFIX, player->quietService->hideOtherPlayers ? "hiding" : "showing");
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzJSAlways)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->ToggleJSAlways();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzRestart)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	// TODO: rework timer
	player->InvalidateTimer();
	if (player->GetPawn()->IsAlive())
	{
		player->GetPawn()->Respawn();
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

internal SCMD_CALLBACK(Command_KzHideWeapon)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->quietService->ToggleHideWeapon();
	utils::CPrintChat(player->GetPawn(), "%s {grey}You are now %s your weapons.", KZ_CHAT_PREFIX, player->quietService->ShouldHideWeapon() ? "hiding" : "showing");
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_StopTimer)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (player->timerIsRunning)
	{
		utils::CPrintChat(player->GetPawn(), "%s {darkred}Timer stopped.", KZ_CHAT_PREFIX);	
	}
	player->InvalidateTimer();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_JoinTeam)
{
	KZ::misc::JoinTeam(g_pKZPlayerManager->ToPlayer(controller), atoi(args->Arg(1)), false);
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzPanel)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->hudService->TogglePanel();
	utils::CPrintChat(player->GetPawn(), "%s {grey}Your centre information panel has been %s.", KZ_CHAT_PREFIX, player->hudService->IsShowingPanel() ? "enabled" : "disabled");
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzToggleJumpstats)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->ToggleJumpstatsReporting();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_TestSound)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	utils::PlaySoundToClient(player->GetPlayerSlot(), KZ_SND_NEW_RECORD, 0.5f);
	for(i32 i = 2; i < 7; i++)
	{
		utils::PlaySoundToClient(player->GetPlayerSlot(), distanceTierSounds[i], 0.5f);
	}
	return MRES_SUPERCEDE;
}

// TODO: move command registration to the service class?
void KZ::misc::RegisterCommands()
{
	scmd::RegisterCmd("kz_test",		Command_TestSound,			"Test sound.");

	scmd::RegisterCmd("kz_panel",		Command_KzPanel,			"Toggle Panel display.");
	scmd::RegisterCmd("kz_togglestats",	Command_KzToggleJumpstats,	"Change Jumpstats print type.");
	scmd::RegisterCmd("kz_togglejs",	Command_KzToggleJumpstats,	"Change Jumpstats print type.");
	scmd::RegisterCmd("kz_noclip",		Command_KzNoclip,			"Toggle noclip.");
	scmd::RegisterCmd("kz_hidelegs",	Command_KzHidelegs,			"Hide your legs in first person.");
	scmd::RegisterCmd("kz_hide",		Command_KzHide,				"Hide other players.");
	scmd::RegisterCmd("kz_jsalways",	Command_KzJSAlways,			"Print jumpstats for invalid jumps.");
	scmd::RegisterCmd("kz_restart",		Command_KzRestart,			"Restart.");
	scmd::RegisterCmd("kz_r",			Command_KzRestart,			"Restart.");
	scmd::RegisterCmd("kz_hideweapon",	Command_KzHideWeapon,		"Hide weapon viewmodel.");
	scmd::RegisterCmd("kz_stop",		Command_StopTimer,			"Stop timer.");
	scmd::RegisterCmd("jointeam",		Command_JoinTeam,			"Jointeam interceptor", true);
	KZCheckpointService::RegisterCommands();
	KZ::mode::RegisterCommands();
	KZ::style::RegisterCommands();
}

void KZ::misc::OnClientActive(CPlayerSlot slot)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	player->Reset();
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
			//player.GetOrigin(savedOrigin[client]);
			//player.GetEyeAngles(savedAngles[client]);
			//savedOnLadder[client] = player.Movetype == MOVETYPE_LADDER;
			//hasSavedPosition[client] = true;
		}

		//if (!player.Paused && !player.CanPause)
		//{
		//	player.StopTimer();
		//}
		player->GetController()->ChangeTeam(CS_TEAM_SPECTATOR);
		//Call_GOKZ_OnJoinTeam(client, newTeam);
	}
	else if (newTeam == CS_TEAM_CT && currentTeam != CS_TEAM_CT
		|| newTeam == CS_TEAM_T && currentTeam != CS_TEAM_T)
	{
		player->GetPawn()->CommitSuicide(false, true);
		player->GetController()->SwitchTeam(newTeam);
		player->GetController()->Respawn();
		//if (restorePos && hasSavedPosition[client])
		//{
		//	TeleportPlayer(client, savedOrigin[client], savedAngles[client]);
		//	if (savedOnLadder[client])
		//	{
		//		player.Movetype = MOVETYPE_LADDER;
		//	}
		//}
		//else
		//{
		//	player.StopTimer();
		//	// Just joining a team alone can put you into weird invalid spawns. 
		//	// Need to teleport the player to a valid one.
		Vector spawnOrigin;
		QAngle spawnAngles;
		utils::FindValidSpawn(spawnOrigin, spawnAngles);
		player->GetPawn()->Teleport(&spawnOrigin, &spawnAngles, &vec3_origin);
		//}
		//hasSavedPosition[client] = false;
		//Call_GOKZ_OnJoinTeam(client, newTeam);
	}
}
