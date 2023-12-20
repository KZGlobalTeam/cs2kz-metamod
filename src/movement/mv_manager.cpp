#include "movement.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"
extern CEntitySystem *g_pEntitySystem;

MovementPlayer *CMovementPlayerManager::ToPlayer(CCSPlayer_MovementServices *ms)
{
	int index = ms->pawn->m_hController().GetEntryIndex();
	assert(index >= 0 && index < MAXPLAYERS + 1);
	return this->players[index];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CBasePlayerController *controller)
{
	int index = controller->m_pEntity->m_EHandle.GetEntryIndex();
	assert(index >= 0 && index < MAXPLAYERS + 1);
	return this->players[index];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CBasePlayerPawn *pawn)
{
	int index = pawn->m_hController().GetEntryIndex();
	assert(index >= 0 && index < MAXPLAYERS + 1);
	return this->players[index];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CPlayerSlot slot)
{
	int index = slot.Get() + 1;
	assert(index >= 0 && index < MAXPLAYERS + 1);
	return this->players[index];
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CEntityIndex entIndex)
{
	if (!g_pEntitySystem) return nullptr;
	int index = g_pEntitySystem->GetBaseEntity(entIndex)->m_pEntity->m_EHandle.GetEntryIndex();
	assert(index >= 0 && index < MAXPLAYERS + 1);
	return this->players[index];
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