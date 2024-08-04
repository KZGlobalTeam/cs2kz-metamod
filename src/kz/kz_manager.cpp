#include "kz.h"

#include "tier0/memdbgon.h"
KZPlayerManager g_KZPlayerManager;

KZPlayerManager *g_pKZPlayerManager = &g_KZPlayerManager;
PlayerManager *g_pPlayerManager = dynamic_cast<PlayerManager *>(&g_KZPlayerManager);

KZPlayer *KZPlayerManager::ToPlayer(CPlayerPawnComponent *component)
{
	return static_cast<KZPlayer *>(MovementPlayerManager::ToPlayer(component));
}

KZPlayer *KZPlayerManager::ToPlayer(CBasePlayerController *controller)
{
	return static_cast<KZPlayer *>(MovementPlayerManager::ToPlayer(controller));
}

KZPlayer *KZPlayerManager::ToPlayer(CBasePlayerPawn *pawn)
{
	return static_cast<KZPlayer *>(MovementPlayerManager::ToPlayer(pawn));
}

KZPlayer *KZPlayerManager::ToPlayer(CPlayerSlot slot)
{
	return static_cast<KZPlayer *>(MovementPlayerManager::ToPlayer(slot));
}

KZPlayer *KZPlayerManager::ToPlayer(CEntityIndex entIndex)
{
	return static_cast<KZPlayer *>(MovementPlayerManager::ToPlayer(entIndex));
}

KZPlayer *KZPlayerManager::ToPlayer(CPlayerUserId userID)
{
	return static_cast<KZPlayer *>(MovementPlayerManager::ToPlayer(userID));
}

KZPlayer *KZPlayerManager::ToPlayer(u32 index)
{
	return static_cast<KZPlayer *>(MovementPlayerManager::players[index]);
}
