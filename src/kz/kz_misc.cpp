#include "common.h"
#include "utils/utils.h"
#include "kz.h"
#include "utils/simplecmds.h"

#include "checkpoint/kz_checkpoint.h"
#include "jumpstats/kz_jumpstats.h"
#include "quiet/kz_quiet.h"
#include "mode/kz_mode.h"

internal SCMD_CALLBACK(Command_KzNoclip)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->ToggleNoclip();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHidelegs)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->ToggleHideLegs();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzCheckpoint)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetCheckpoint();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzTeleport)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToCheckpoint();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzPrevcp)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToPrevCp();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzNextcp)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToNextCp();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHide)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->quietService->ToggleHide();
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
	CALL_VIRTUAL(void, offsets::Respawn, player->GetPawn());
	return MRES_SUPERCEDE;
}

// TODO: move command registration to the service class?
void KZ::misc::RegisterCommands()
{
	scmd::RegisterCmd("kz_noclip",     Command_KzNoclip,     "Toggle noclip.");
	scmd::RegisterCmd("kz_hidelegs",   Command_KzHidelegs,   "Hide your legs in first person.");
	scmd::RegisterCmd("kz_checkpoint", Command_KzCheckpoint, "Make a checkpoint on ground or on a ladder.");
	scmd::RegisterCmd("kz_cp",         Command_KzCheckpoint, "Make a checkpoint on ground or on a ladder.");
	scmd::RegisterCmd("kz_teleport",   Command_KzTeleport,   "Teleport to the current checkpoint.");
	scmd::RegisterCmd("kz_tp",         Command_KzTeleport,   "Teleport to the current checkpoint.");
	scmd::RegisterCmd("kz_prevcp",     Command_KzPrevcp,     "Teleport to the last checkpoint.");
	scmd::RegisterCmd("kz_nextcp",     Command_KzNextcp,     "Teleport to the next checkpoint.");
	scmd::RegisterCmd("kz_hide",	   Command_KzHide,       "Hide other players.");
	scmd::RegisterCmd("kz_jsalways",   Command_KzJSAlways,   "Print jumpstats for invalid jumps.");
	scmd::RegisterCmd("kz_respawn",    Command_KzRestart,    "Respawn.");
	KZ::mode::RegisterCommands();
}

void KZ::misc::OnClientPutInServer(CPlayerSlot slot)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	player->Reset();
}
