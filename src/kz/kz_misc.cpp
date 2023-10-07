#include "common.h"
#include "kz.h"

void KZ::misc::EnableGodMode(CPlayerSlot slot)
{
	KZPlayer *player = GetKZPlayerManager()->ToPlayer(slot);
	CCSPlayerPawn *pawn = player->GetPawn();
	if (!pawn) return;
	pawn->m_bTakesDamage = false;
	pawn->m_nMyCollisionGroup = 5;
	pawn->m_pCollision->m_CollisionGroup = 5;
}