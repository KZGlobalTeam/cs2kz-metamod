#include "kz_checkpoint.h"
#include "utils/simplecmds.h"

static_function SCMD_CALLBACK(Command_KzUndoTeleport)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->UndoTeleport();
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzCheckpoint)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetCheckpoint();
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzTeleport)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToCheckpoint();
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzPrevcp)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToPrevCp();
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzNextcp)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToNextCp();
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_SetStartPos)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetStartPosition();
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_ClearStartPos)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->ClearStartPosition();
	return MRES_SUPERCEDE;
}

void KZCheckpointService::RegisterCommands()
{
	// clang-format off
	scmd::RegisterCmd("kz_undo", Command_KzUndoTeleport);
	scmd::RegisterCmd("kz_checkpoint", Command_KzCheckpoint);
	scmd::RegisterCmd("kz_cp", Command_KzCheckpoint);
	scmd::RegisterCmd("kz_teleport", Command_KzTeleport);
	scmd::RegisterCmd("kz_tp", Command_KzTeleport);
	scmd::RegisterCmd("kz_prevcp", Command_KzPrevcp);
	scmd::RegisterCmd("kz_nextcp", Command_KzNextcp);
	scmd::RegisterCmd("kz_pcp", Command_KzPrevcp);
	scmd::RegisterCmd("kz_ncp", Command_KzNextcp);
	scmd::RegisterCmd("kz_setstartpos", Command_SetStartPos);
	scmd::RegisterCmd("kz_ssp", Command_SetStartPos);
	scmd::RegisterCmd("kz_clearstartpos", Command_ClearStartPos);
	scmd::RegisterCmd("kz_csp", Command_ClearStartPos);
	// clang-format on
}
