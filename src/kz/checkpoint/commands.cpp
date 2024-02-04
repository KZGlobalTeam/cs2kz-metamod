#include "kz_checkpoint.h"
#include "utils/simplecmds.h"

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

internal SCMD_CALLBACK(Command_SetStartPos)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetStartPosition();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_ClearStartPos)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->ClearStartPosition();
	return MRES_SUPERCEDE;
}

void KZCheckpointService::RegisterCommands()
{
	scmd::RegisterCmd("kz_checkpoint", Command_KzCheckpoint, "Make a checkpoint on ground or on a ladder.");
	scmd::RegisterCmd("kz_cp", Command_KzCheckpoint, "Make a checkpoint on ground or on a ladder.");
	scmd::RegisterCmd("kz_teleport", Command_KzTeleport, "Teleport to the current checkpoint.");
	scmd::RegisterCmd("kz_tp", Command_KzTeleport, "Teleport to the current checkpoint.");
	scmd::RegisterCmd("kz_prevcp", Command_KzPrevcp, "Teleport to the last checkpoint.");
	scmd::RegisterCmd("kz_nextcp", Command_KzNextcp, "Teleport to the next checkpoint.");
	scmd::RegisterCmd("kz_pcp", Command_KzPrevcp, "Teleport to the last checkpoint.");
	scmd::RegisterCmd("kz_ncp", Command_KzNextcp, "Teleport to the next checkpoint.");
	scmd::RegisterCmd("kz_setstartpos", Command_SetStartPos, "Set your custom start position to your current position.");
	scmd::RegisterCmd("kz_ssp", Command_SetStartPos, "Set your custom start position to your current position.");
	scmd::RegisterCmd("kz_clearstartpos", Command_ClearStartPos, "Clear your custom start position.");
	scmd::RegisterCmd("kz_csp", Command_ClearStartPos, "Clear your custom start position.");
}

