#include "kz.h"

#include "tier0/memdbgon.h"
CKZPlayerManager g_KZPlayerManager;
CMovementPlayerManager *g_pPlayerManager = dynamic_cast<CMovementPlayerManager *>(&g_KZPlayerManager);

CKZPlayerManager *KZ::GetKZPlayerManager()
{
	return static_cast<CKZPlayerManager *>(g_pPlayerManager);
}

KZPlayer *CKZPlayerManager::ToPlayer(CCSPlayer_MovementServices *ms)
{
	return static_cast<KZPlayer *>(CMovementPlayerManager::ToPlayer(ms));
}

KZPlayer *CKZPlayerManager::ToPlayer(CCSPlayerController *controller)
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

KZPlayer* CKZPlayerManager::ToPlayer(u32 index)
{
	return static_cast<KZPlayer*>(CMovementPlayerManager::players[index]);
}