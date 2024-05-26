#include "kz.h"

#include "tier0/memdbgon.h"
CKZPlayerManager g_KZPlayerManager;

CKZPlayerManager *g_pKZPlayerManager = &g_KZPlayerManager;
PlayerManager *g_pPlayerManager = dynamic_cast<PlayerManager *>(&g_KZPlayerManager);

KZPlayer *CKZPlayerManager::ToPlayer(CPlayerPawnComponent *component)
{
	return static_cast<KZPlayer *>(CMovementPlayerManager::ToPlayer(component));
}

KZPlayer *CKZPlayerManager::ToPlayer(CBasePlayerController *controller)
{
	return static_cast<KZPlayer *>(CMovementPlayerManager::ToPlayer(controller));
}

KZPlayer *CKZPlayerManager::ToPlayer(CBasePlayerPawn *pawn)
{
	return static_cast<KZPlayer *>(CMovementPlayerManager::ToPlayer(pawn));
}

KZPlayer *CKZPlayerManager::ToPlayer(CPlayerSlot slot)
{
	return static_cast<KZPlayer *>(CMovementPlayerManager::ToPlayer(slot));
}

KZPlayer *CKZPlayerManager::ToPlayer(CEntityIndex entIndex)
{
	return static_cast<KZPlayer *>(CMovementPlayerManager::ToPlayer(entIndex));
}

KZPlayer *CKZPlayerManager::ToPlayer(CPlayerUserId userID)
{
	return static_cast<KZPlayer *>(CMovementPlayerManager::ToPlayer(userID));
}

KZPlayer *CKZPlayerManager::ToPlayer(u32 index)
{
	return static_cast<KZPlayer *>(CMovementPlayerManager::players[index]);
}
