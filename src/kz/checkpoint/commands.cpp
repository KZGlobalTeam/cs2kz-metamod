#include "kz_checkpoint.h"
#include "utils/simplecmds.h"

SCMD(kz_checkpoint, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetCheckpoint();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_cp, kz_checkpoint);

SCMD(kz_teleport, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToCheckpoint();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_tp, kz_teleport);

SCMD(kz_undo, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->UndoTeleport();
	return MRES_SUPERCEDE;
}

SCMD(kz_prevcp, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToPrevCp();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_pcp, kz_prevcp);

SCMD(kz_nextcp, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToNextCp();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_ncp, kz_nextcp);

SCMD(kz_setstartpos, SCFL_CHECKPOINT | SCFL_MAP | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetStartPosition();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_ssp, kz_setstartpos);

SCMD(kz_clearstartpos, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->ClearStartPosition();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_csp, kz_clearstartpos);
