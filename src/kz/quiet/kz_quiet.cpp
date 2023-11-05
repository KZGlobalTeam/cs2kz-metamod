#include "kz_quiet.h"

void KZ::quiet::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
{
	for (int i = 0; i < infoCount; i++)
	{
		// Cast it to our own TransmitInfo struct because CCheckTransmitInfo isn't correct.
		TransmitInfo *pTransmitInfo = reinterpret_cast<TransmitInfo*>(pInfo[i]);
		
		// Find out who this info will be sent to.
		CPlayerSlot targetSlot = pTransmitInfo->m_nClientEntityIndex;
		KZPlayer *targetPlayer = KZ::GetKZPlayerManager()->ToPlayer(targetSlot);

		// Don't hide if player is dead/spectating or if they aren't hiding other players.
		if (!targetPlayer->quietService->hideOtherPlayers) continue;
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

void KZQuietService::Reset()
{
	this->hideOtherPlayers = false;
}