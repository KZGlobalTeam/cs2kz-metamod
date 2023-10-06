#include "playermanager.h"

MovementPlayer *CPlayerManager::ToPlayer(CCSPlayer_MovementServices *ms)
{
	return this->players[ms->pawn->m_hController.GetEntryIndex()];
}

MovementPlayer *CPlayerManager::ToPlayer(CCSPlayerController *controller)
{
	return this->players[controller->m_pEntity->m_EHandle.GetEntryIndex()];
}

MovementPlayer *CPlayerManager::ToPlayer(CCSPlayerPawn *pawn)
{
	return this->players[pawn->m_hController.GetEntryIndex()];
}

MovementPlayer *CPlayerManager::ToPlayer(CPlayerSlot slot)
{
	return this->players[slot.Get() + 1];
}

MovementPlayer *CPlayerManager::ToPlayer(CEntityIndex entIndex)
{
	if (!g_pEntitySystem) return nullptr;
	return this->players[g_pEntitySystem->GetBaseEntity(entIndex)->m_pEntity->m_EHandle.GetEntryIndex()];
}