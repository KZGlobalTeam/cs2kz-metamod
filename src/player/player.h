#include "common.h"
#include "sdk/serversideclient.h"
#include "sdk/datatypes.h"
#include "sdk/services.h"
#include "utils/utils.h"

// The index is the entity index associated with the player's controller's entity index.
// The player slot should always be this index minus 1.
class Player
{
public:
	Player(int i) : index(i) {}

	virtual ~Player() {}

	virtual void Init() {}

	virtual void Reset()
	{
		unauthenticatedSteamID = k_steamIDNil;
	}

	virtual CCSPlayerController *GetController();

	virtual CBasePlayerPawn *GetCurrentPawn()
	{
		return GetController() ? GetController()->GetCurrentPawn() : nullptr;
	};

	virtual CCSPlayerPawn *GetPlayerPawn()
	{
		return GetController() ? GetController()->GetPlayerPawn() : nullptr;
	}

	virtual CCSPlayerPawnBase *GetObserverPawn()
	{
		return GetController() ? GetController()->GetObserverPawn() : nullptr;
	}

	CPlayerSlot GetPlayerSlot()
	{
		return index - 1;
	}

	bool IsAuthenticated()
	{
		return interfaces::pEngine->IsClientFullyAuthenticated(GetPlayerSlot());
	}

	bool IsConnected()
	{
		return GetClient() ? GetClient()->IsConnected() : false;
	}

	bool IsInGame()
	{
		return GetClient() ? GetClient()->IsInGame() : false;
	}

	bool IsFakeClient()
	{
		return GetClient() ? GetClient()->IsFakePlayer() : false;
	}

	bool IsCSTV()
	{
		return GetClient() ? GetClient()->IsHLTV() : false;
	};

	void Kick(const char *internalReason = "", ENetworkDisconnectionReason reason = NETWORK_DISCONNECT_KICKED);

	const char *GetName()
	{
		return GetController() ? GetController()->GetPlayerName() : "<blank>";
	}

	const char *GetIpAddress()
	{
		return GetClient() ? GetClient()->GetRemoteAddress()->ToString(true) : nullptr;
	}

	const CSteamID &GetSteamId(bool validated = true)
	{
		static_persist const CSteamID invalidId = k_steamIDNil;
		if (validated && !IsAuthenticated())
		{
			return invalidId;
		}
		CServerSideClient *client = GetClient();
		if (client && IsAuthenticated())
		{
			return *client->GetClientSteamID();
		}
		return unauthenticatedSteamID;
	}

	u64 GetSteamId64(bool validated = true)
	{
		return GetSteamId(validated).ConvertToUint64();
	}

	CServerSideClient *GetClient()
	{
		return g_pKZUtils->GetClientBySlot(GetPlayerSlot());
	}

	void SetUnauthenticatedSteamID(u64 xuid)
	{
		unauthenticatedSteamID = CSteamID(xuid);
	}

public:
	// General
	const i32 index;

private:
	CSteamID unauthenticatedSteamID = k_steamIDNil;
};

class PlayerManager
{
public:
	PlayerManager()
	{
		for (int i = 0; i < MAXPLAYERS + 1; i++)
		{
			delete players[i];
			players[i] = new Player(i);
		}
	}

	virtual ~PlayerManager()
	{
		for (int i = 0; i < MAXPLAYERS + 1; i++)
		{
			delete players[i];
		}
	}

public:
	Player *ToPlayer(CPlayerPawnComponent *component);
	Player *ToPlayer(CBasePlayerController *controller);
	Player *ToPlayer(CBasePlayerPawn *pawn);
	Player *ToPlayer(CPlayerSlot slot);
	Player *ToPlayer(CEntityIndex entIndex);
	Player *ToPlayer(CPlayerUserId userID);

	void OnClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason);
	void OnClientConnected(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer);
	void OnClientFullyConnect(CPlayerSlot slot);
	void OnClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid);
	void OnClientActive(CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid);
	void OnClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID);
	void OnClientVoice(CPlayerSlot slot);
	void OnClientSettingsChanged(CPlayerSlot slot);

public:
	Player *players[MAXPLAYERS + 1];
};

extern PlayerManager *g_pPlayerManager;
