#include "movement.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

MovementPlayer *CMovementPlayerManager::ToPlayer(CPlayerPawnComponent *component)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(component));
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CBasePlayerController *controller)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(controller));
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CBasePlayerPawn *pawn)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(pawn));
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CPlayerSlot slot)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(slot));
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CEntityIndex entIndex)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(entIndex));
}

MovementPlayer *CMovementPlayerManager::ToPlayer(CPlayerUserId userID)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(userID));
}

MovementPlayer *CMovementPlayerManager::ToPlayer(u32 index)
{
	return static_cast<MovementPlayer *>(PlayerManager::players[index]);
}
