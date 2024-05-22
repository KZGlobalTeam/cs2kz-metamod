#include "player.h"

CCSPlayerController *Player::GetController()
{
	if (!GameEntitySystem())
	{
		return nullptr;
	}
	CBaseEntity *ent = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(CEntityIndex(this->index)));
	if (!ent)
	{
		return nullptr;
	}
	return ent->IsController() ? static_cast<CCSPlayerController *>(ent) : nullptr;
}

void Player::Kick(const char *internalReason, ENetworkDisconnectionReason reason)
{
	interfaces::pEngine->KickClient(this->GetPlayerSlot(), internalReason, reason);
}
