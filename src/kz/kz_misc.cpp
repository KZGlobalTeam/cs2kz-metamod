#include "common.h"
#include "kz.h"

CKZPlayerManager g_KZPlayerManager;
CMovementPlayerManager *g_pPlayerManager = dynamic_cast<CMovementPlayerManager *>(&g_KZPlayerManager);

void KZ::misc::EnableGodMode(CPlayerSlot slot)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(slot);
	CCSPlayerPawn *pawn = player->GetPawn();
	if (!pawn) return;
	pawn->m_bTakesDamage = false;
}

void KZ::misc::InitPlayerManager()
{
}