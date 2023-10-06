#include "misc.h"
#include "common.h"
#include <playermanager.h>

void KZ::misc::EnableGodMode(CPlayerSlot slot)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(slot);
	CCSPlayerPawn *pawn = player->GetPawn();
	if (!pawn) return;
	pawn->m_bTakesDamage = false;
}