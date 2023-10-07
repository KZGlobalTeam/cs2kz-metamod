#include "common.h"
#include "kz.h"

void KZ::misc::EnableGodMode(CPlayerSlot slot)
{
	KZPlayer *player = GetKZPlayerManager()->ToPlayer(slot);
	CCSPlayerPawn *pawn = player->GetPawn();
	if (!pawn) return;
	pawn->m_bTakesDamage = false;
	pawn->m_nMyCollisionGroup = COLLISION_GROUP_DEBRIS_TRIGGER;
	pawn->m_pCollision->m_CollisionGroup = COLLISION_GROUP_DEBRIS_TRIGGER;
}