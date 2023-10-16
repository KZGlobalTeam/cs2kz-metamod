#include "common.h"
#include "utils/utils.h"
#include "kz.h"
#include "utils/simplecmds.h"

#include "tier0/memdbgon.h"

internal SCMD_CALLBACK(Command_KzNoclip)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->ToggleNoclip();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHidelegs)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->UpdatePlayerModelAlpha();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzCheckpoint)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->SetCheckpoint();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzTeleport)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->TpToCheckpoint();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzPrevcp)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->TpToPrevCp();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzNextcp)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->TpToNextCp();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHide)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->ToggleHide();
	return MRES_SUPERCEDE;
}

void KZ::misc::RegisterCommands()
{
	scmd::RegisterCmd("kz_noclip",     Command_KzNoclip);
	scmd::RegisterCmd("kz_hidelegs",   Command_KzHidelegs);
	scmd::RegisterCmd("kz_checkpoint", Command_KzCheckpoint);
	scmd::RegisterCmd("kz_cp",         Command_KzCheckpoint);
	scmd::RegisterCmd("kz_teleport",   Command_KzTeleport);
	scmd::RegisterCmd("kz_tp",         Command_KzTeleport);
	scmd::RegisterCmd("kz_prevcp",     Command_KzPrevcp);
	scmd::RegisterCmd("kz_nextcp",     Command_KzNextcp);
	scmd::RegisterCmd("kz_hide",	   Command_KzHide);
}

void KZ::misc::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
{
	for (int i = 0; i < infoCount; i++)
	{
		// Cast it to our own TransmitInfo struct because CCheckTransmitInfo isn't correct.
		TransmitInfo *pTransmitInfo = reinterpret_cast<TransmitInfo*>(pInfo[i]);
		
		// Find out who this info will be sent to.
		CPlayerSlot targetSlot = pTransmitInfo->m_nClientEntityIndex;
		KZPlayer *targetPlayer = KZ::GetKZPlayerManager()->ToPlayer(targetSlot);

		// Don't hide if player is dead/spectating or if they aren't hiding other players.
		if (!targetPlayer->hideOtherPlayers) continue;
		if (targetPlayer->GetPawn()->m_lifeState() != LIFE_ALIVE) continue;

		// Loop through the list of players and see if they need to be hidden away from our target player.
		for (int j = 0; j < MAXPLAYERS; j++)
		{
			if (j == targetPlayer->GetController()->entindex()) continue;
			CBasePlayerPawn *pawn = KZ::GetKZPlayerManager()->players[j]->GetPawn();
			if (!pawn) continue;
			if (pTransmitInfo->m_pTransmitEdict->IsBitSet(pawn->entindex()))
			{
				pTransmitInfo->m_pTransmitEdict->Clear(pawn->entindex());
			}
		}
	}
}

void KZ::misc::OnClientPutInServer(CPlayerSlot slot)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(slot);
	player->Reset();
}