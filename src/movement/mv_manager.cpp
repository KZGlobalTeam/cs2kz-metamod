#include "movement.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"
extern CEntitySystem *g_pEntitySystem;

MovementPlayer *CMovementPlayerManager::ToPlayer(CCSPlayer_MovementServices *ms)
{
	return this->players[ms->pawn->m_hController().GetEntryIndex()];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CCSPlayerController *controller)
{
	return this->players[controller->m_pEntity->m_EHandle.GetEntryIndex()];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CBasePlayerPawn *pawn)
{
	return this->players[pawn->m_hController().GetEntryIndex()];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CPlayerSlot slot)
{
	return this->players[slot.Get() + 1];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CEntityIndex entIndex)
{
	if (!g_pEntitySystem) return nullptr;
	return this->players[g_pEntitySystem->GetBaseEntity(entIndex)->m_pEntity->m_EHandle.GetEntryIndex()];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CPlayerUserId userID)
{
	if (!g_pEntitySystem) return nullptr;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (interfaces::pEngine->GetPlayerUserId(i) == userID.Get())
		{
			return this->players[i+1];
		}
	}
	return nullptr;
}