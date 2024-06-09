#include "player.h"
#include "steam/steam_gameserver.h"

extern CSteamGameServerAPIContext g_steamAPI;

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

void Player::OnAuthorized()
{
	this->hasPrime = (g_steamAPI.SteamGameServer()->UserHasLicenseForApp(*this->GetClient()->GetClientSteamID(), 624820) == 0);
}
