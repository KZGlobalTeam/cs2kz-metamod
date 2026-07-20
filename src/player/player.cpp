#include "player.h"
#include "networksystem/inetworkmessages.h"
#include "gameevents.pb.h"
#include "sdk/recipientfilters.h"

#include "steam/steam_gameserver.h"
#include "cstrike15_gcmessages.pb.h"

class CEconPersonaDataPublic // GCSDK::CProtoBufSharedObject < CSOPersonaDataPublic, k_EEconTypePersonaDataPublic >
{
public:
	void **vtable;
	CSOPersonaDataPublic data;
};

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
	auto steamID = this->GetClient()->GetClientSteamID();
	if (!g_steamAPI.SteamGameServer())
	{
		g_steamAPI.Init();
	}
	if (CommandLine()->HasParm("-dedicated"))
	{
		this->hasPrime = (g_steamAPI.SteamGameServer()
						  && g_steamAPI.SteamGameServer()->UserHasLicenseForApp(steamID, 624820) == k_EUserHasLicenseResultHasLicense);
	}
	CEconPersonaDataPublic *persona = this->GetController()->m_pInventoryServices()->GetPublicPersonaData();
	if (persona && persona->data.elevated_state())
	{
		this->hasPrime = true;
	}
}

void Player::SetName(const char *name)
{
	if (this->GetClient())
	{
		this->GetClient()->m_ConVars->SetString("name", name);
		this->GetClient()->UpdateUserSettings();
		this->GetClient()->SetName(name);
	}
}

void Player::SetClan(const char *clan)
{
	if (this->GetController())
	{
		this->GetController()->SetClan(clan);
		char newName[128];
		V_strncpy(newName, this->GetController()->m_iszPlayerName(), sizeof(newName));
		i32 length = V_strlen(newName);
		if (length > 0 && newName[length - 1] == ' ')
		{
			newName[length - 1] = '\0';
		}
		else
		{
			V_snprintf(newName, sizeof(newName), "%s ", this->GetController()->m_iszPlayerName());
		}
		this->SetName(newName);
	}
}

bool Player::CheckPrime()
{
	if (this->hasPrime)
	{
		return true;
	}
	auto steamID = this->GetClient()->GetClientSteamID();

	if (CommandLine()->HasParm("-dedicated"))
	{
		this->hasPrime |= (g_steamAPI.SteamGameServer()
						   && g_steamAPI.SteamGameServer()->UserHasLicenseForApp(steamID, 624820) == k_EUserHasLicenseResultHasLicense);
	}
	CEconPersonaDataPublic *persona = this->GetController()->m_pInventoryServices()->GetPublicPersonaData();
	this->hasPrime |= (persona && persona->data.elevated_state());
	if (this->hasPrime)
	{
		this->OnPrimeStatusConfirmed();
	}
	return this->hasPrime;
}
