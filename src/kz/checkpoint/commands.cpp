#include "kz_checkpoint.h"
#include "utils/simplecmds.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"

SCMD(kz_checkpoint, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetCheckpoint();
	return true;
}

SCMD_LINK(kz_cp, kz_checkpoint);

SCMD(kz_teleport, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToCheckpoint();
	return true;
}

SCMD_LINK(kz_tp, kz_teleport);

SCMD(kz_undo, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->UndoTeleport();
	return true;
}

SCMD(kz_prevcp, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToPrevCp();
	return true;
}

SCMD_LINK(kz_pcp, kz_prevcp);

SCMD(kz_nextcp, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToNextCp();
	return true;
}

SCMD_LINK(kz_ncp, kz_nextcp);

SCMD(kz_setstartpos, SCFL_CHECKPOINT | SCFL_MAP | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetStartPosition();
	return true;
}

SCMD_LINK(kz_ssp, kz_setstartpos);

SCMD(kz_clearstartpos, SCFL_CHECKPOINT)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->ClearStartPosition();
	return true;
}

SCMD_LINK(kz_csp, kz_clearstartpos);

SCMD(kz_cpsound, SCFL_CHECKPOINT | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->optionService->SetPreferenceBool("checkpointSound", !player->optionService->GetPreferenceBool("checkpointSound", true));
	if (player->optionService->GetPreferenceBool("checkpointSound", true))
	{
		player->languageService->PrintChat(true, false, "Checkpoint Options - Checkpoint Sound - Enable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Checkpoint Options - Checkpoint Sound - Disable");
	}
	return true;
}

SCMD_LINK(kz_checkpointsound, kz_cpsound);

SCMD(kz_tpsound, SCFL_CHECKPOINT | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->optionService->SetPreferenceBool("teleportSound", !player->optionService->GetPreferenceBool("teleportSound", true));
	if (player->optionService->GetPreferenceBool("teleportSound", true))
	{
		player->languageService->PrintChat(true, false, "Checkpoint Options - Teleport Sound - Enable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Checkpoint Options - Teleport Sound - Disable");
	}
	return true;
}

SCMD_LINK(kz_teleportsound, kz_tpsound);

SCMD(kz_cpmessage, SCFL_CHECKPOINT | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	bool current = player->optionService->GetPreferenceBool("checkpointMessage", true);
	player->optionService->SetPreferenceBool("checkpointMessage", !current);
	if (!current)
	{
		player->languageService->PrintChat(true, false, "Checkpoint Options - Checkpoint Message - Enable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Checkpoint Options - Checkpoint Message - Disable");
	}
	return true;
}

SCMD_LINK(kz_cpmsg, kz_cpmessage);
