#include "movement.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

MovementPlayer *MovementPlayerManager::ToPlayer(CPlayerPawnComponent *component)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(component));
}

MovementPlayer *MovementPlayerManager::ToPlayer(CBasePlayerController *controller)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(controller));
}

MovementPlayer *MovementPlayerManager::ToPlayer(CBasePlayerPawn *pawn)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(pawn));
}

MovementPlayer *MovementPlayerManager::ToPlayer(CPlayerSlot slot)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(slot));
}

MovementPlayer *MovementPlayerManager::ToPlayer(CEntityIndex entIndex)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(entIndex));
}

MovementPlayer *MovementPlayerManager::ToPlayer(CPlayerUserId userID)
{
	return static_cast<MovementPlayer *>(PlayerManager::ToPlayer(userID));
}

MovementPlayer *MovementPlayerManager::ToPlayer(u32 index)
{
	return static_cast<MovementPlayer *>(PlayerManager::players[index]);
}
