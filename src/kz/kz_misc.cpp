#include "usermessages.pb.h"

#include "common.h"
#include "utils/utils.h"
#include "kz.h"
#include "utils/simplecmds.h"
#include "public/networksystem/inetworkmessages.h"

#include "checkpoint/kz_checkpoint.h"
#include "jumpstats/kz_jumpstats.h"
#include "quiet/kz_quiet.h"

#include "tier0/memdbgon.h"
#include <utils/recipientfilters.h>

internal SCMD_CALLBACK(Command_KzNoclip)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->ToggleNoclip();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHidelegs)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->ToggleHideLegs();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzCheckpoint)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->checkpointService->SetCheckpoint();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzTeleport)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->checkpointService->TpToCheckpoint();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzPrevcp)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->checkpointService->TpToPrevCp();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzNextcp)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->checkpointService->TpToNextCp();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHide)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->quietService->ToggleHide();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzJSAlways)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->jumpstatsService->ToggleJSAlways();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzRestart)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	CALL_VIRTUAL(void, offsets::Respawn, player->GetPawn());
	utils::SendConVarValue(utils::GetEntityPlayerSlot(controller), nullptr, "true");
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
}

void KZ::misc::OnClientPutInServer(CPlayerSlot slot)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(slot);
	player->Reset();
}