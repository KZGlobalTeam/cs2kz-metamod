#include "player.h"
#include "utils/utils.h"
#include "iserver.h"

Player *PlayerManager::ToPlayer(CPlayerPawnComponent *component)
{
	return this->ToPlayer(component->pawn);
}

Player *PlayerManager::ToPlayer(CBasePlayerController *controller)
{
	if (!controller)
	{
		return nullptr;
	}
	int index = controller->m_pEntity->m_EHandle.GetEntryIndex();
	assert(index >= 0 && index < MAXPLAYERS + 1);
	return this->players[index];
}

Player *PlayerManager::ToPlayer(CBasePlayerPawn *pawn)
{
	if (!pawn)
	{
		return nullptr;
	}
	CBasePlayerController *controller = utils::GetController(pawn);
	if (!controller)
	{
		return nullptr;
	}
	return this->ToPlayer(controller);
}

Player *PlayerManager::ToPlayer(CPlayerSlot slot)
{
	int index = slot.Get() + 1;
	assert(index >= 0 && index < MAXPLAYERS + 1);
	return this->players[index];
}

Player *PlayerManager::ToPlayer(CEntityIndex entIndex)
{
	if (!GameEntitySystem())
	{
		return nullptr;
	}
	CBaseEntity *ent = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(entIndex));
	if (!ent)
	{
		return nullptr;
	}
	if (ent->IsPawn())
	{
		return this->ToPlayer(static_cast<CBasePlayerPawn *>(ent));
	}
	if (ent->IsController())
	{
		return this->ToPlayer(static_cast<CBasePlayerController *>(ent));
	}
	return nullptr;
}

Player *PlayerManager::ToPlayer(CPlayerUserId userID)
{
	// Untested, careful!
	for (int i = 0; i < g_pKZUtils->GetServerGlobals()->maxClients; i++)
	{
		if (interfaces::pEngine->GetPlayerUserId(i) == userID.Get())
		{
			return this->players[i + 1];
		}
	}
	return nullptr;
}

void PlayerManager::OnClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1,
									CBufferString *pRejectReason)
{
}

void PlayerManager::OnClientConnected(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress,
									  bool bFakePlayer)
{
	this->ToPlayer(slot)->Reset();
	this->ToPlayer(slot)->SetUnauthenticatedSteamID(xuid);
}

void PlayerManager::OnClientFullyConnect(CPlayerSlot slot) {}

void PlayerManager::OnClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid) {}

void PlayerManager::OnClientActive(CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid)
{
	this->ToPlayer(slot)->OnPlayerActive();
}

void PlayerManager::OnClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid,
									   const char *pszNetworkID)
{
}

void PlayerManager::OnClientVoice(CPlayerSlot slot) {}

void PlayerManager::OnClientSettingsChanged(CPlayerSlot slot) {}

void PlayerManager::Cleanup()
{
	if (callbackRegistered)
	{
		m_CallbackValidateAuthTicketResponse.Unregister();
	}
}

void PlayerManager::OnLateLoad()
{
	if (!g_pNetworkServerService)
	{
		META_CONPRINTF("Warning: Plugin lateloaded but g_pNetworkServerService is not available. Auth callbacks will not be registered.\n");
		return;
	}
	if (g_pNetworkServerService->IsActiveInGame())
	{
		this->RegisterSteamAPICallback();
	}
	for (auto player : players)
	{
		if (player->IsAuthenticated())
		{
			player->OnAuthorized();
		}
	}
}

void PlayerManager::OnSteamAPIActivated()
{
	this->RegisterSteamAPICallback();
}

void PlayerManager::OnValidateAuthTicket(ValidateAuthTicketResponse_t *pResponse)
{
	if (pResponse->m_eAuthSessionResponse != k_EAuthSessionResponseOK)
	{
		return;
	}
	uint64 iSteamId = pResponse->m_SteamID.ConvertToUint64();

	for (Player *player : players)
	{
		CServerSideClient *cl = player->GetClient();
		if (cl && *cl->GetClientSteamID() == pResponse->m_SteamID)
		{
			player->OnAuthorized();
			return;
		}
	}
}
