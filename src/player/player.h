#pragma once
#include "common.h"
#include "steam/isteamuser.h"
#include "engine2/serversideclient.h"
#include "sdk/datatypes.h"
#include "sdk/services.h"
#include "sdk/entity/ccsplayercontroller.h"
#include "utils/utils.h"

class ns_address;
class C2S_CONNECT_Message;

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
		hasPrime = false;
		authPending = false;
		authAuthorized = false;
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
		return IsConnected() && !IsFakeClient() && interfaces::pEngine->IsClientFullyAuthenticated(GetPlayerSlot());
	}

	bool IsConnected()
	{
		return GetClient() ? GetClient()->IsConnected() : false;
	}

	// Active clients still lose their controller for a moment during a map change.
	bool IsInGame()
	{
		return GetClient() && GetClient()->IsActive() && GetController();
	}

	bool IsFakeClient()
	{
		return GetClient() ? GetClient()->IsFakeClient() : false;
	}

	bool IsCSTV()
	{
		return GetClient() ? GetClient()->IsHLTV() : false;
	};

	void Kick(const char *internalReason = "", ENetworkDisconnectionReason reason = NETWORK_DISCONNECT_KICKED);

	std::string sanitizedName;

	const char *GetName()
	{
		sanitizedName = GetClient() ? GetClient()->GetClientName() : "<blank>";
		utils::Trim(sanitizedName);
		return sanitizedName.c_str();
	}

	void SetName(const char *name);

	void SetClan(const char *clan);

	const char *GetIpAddress()
	{
		return GetClient() ? GetClient()->GetRemoteAddress()->ToString(true) : nullptr;
	}

	const CSteamID GetSteamId(bool validated = true)
	{
		static_persist const CSteamID invalidId = k_steamIDNil;
		if (validated && !IsAuthenticated())
		{
			return invalidId;
		}
		CServerSideClient *client = GetClient();
		if (client && IsAuthenticated())
		{
			return client->GetClientSteamID();
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

	virtual void OnPlayerConnect(u64 steamID64) {}

	virtual void OnPlayerActive() {}

	virtual void OnPlayerFullyConnect() {}

	virtual void OnAuthorized();

	bool CheckPrime();

	// Called when the player has been confirmed to have prime status but after OnAuthorized has been called.
	// This can happen when the player's prime status is late updated after the player has already been authenticated.
	virtual void OnPrimeStatusConfirmed() {}

public:
	// General
	const i32 index;
	bool hasPrime {};
	bool authPending {};    // True if in auth queue waiting for validation
	bool authAuthorized {}; // True if authorization completed

private:
	CSteamID unauthenticatedSteamID = k_steamIDNil;
};

// Iterates every player slot, or only the slots whose player is in game.
template<typename T>
class PlayerSlotRange
{
public:
	class Iterator
	{
	public:
		Iterator(Player *const *entry, Player *const *last, bool inGameOnly) : entry(entry), last(last), inGameOnly(inGameOnly)
		{
			this->SkipFiltered();
		}

		T *operator*() const
		{
			return static_cast<T *>(*entry);
		}

		Iterator &operator++()
		{
			entry++;
			this->SkipFiltered();
			return *this;
		}

		bool operator!=(const Iterator &other) const
		{
			return entry != other.entry;
		}

	private:
		void SkipFiltered()
		{
			while (inGameOnly && entry != last && !(*entry)->IsInGame())
			{
				entry++;
			}
		}

		Player *const *entry;
		Player *const *last;
		bool inGameOnly;
	};

	PlayerSlotRange(Player *const *players, bool inGameOnly) : players(players), inGameOnly(inGameOnly) {}

	Iterator begin() const
	{
		return Iterator(players, players + MAXPLAYERS, inGameOnly);
	}

	Iterator end() const
	{
		return Iterator(players + MAXPLAYERS, players + MAXPLAYERS, inGameOnly);
	}

private:
	Player *const *players;
	bool inGameOnly;
};

class PlayerManager
{
public:
	PlayerManager()
	{
		for (i32 i = 0; i < MAXPLAYERS; i++)
		{
			delete players[i];
			players[i] = new Player(i + 1);
			players[i]->Reset();
		}
	}

	virtual ~PlayerManager()
	{
		for (Player *player : players)
		{
			delete player;
		}
	}

public:
	Player *ToPlayer(CPlayerPawnComponent *component);
	Player *ToPlayer(CBasePlayerController *controller);
	Player *ToPlayer(CBasePlayerPawn *pawn);
	Player *ToPlayer(CPlayerSlot slot);
	Player *ToPlayer(CEntityIndex entIndex);
	Player *ToPlayer(CPlayerUserId userID);
	Player *SteamIdToPlayer(u64 steamID, bool validated = true);

	PlayerSlotRange<Player> GetAllSlots()
	{
		return PlayerSlotRange<Player>(players, false);
	}

	PlayerSlotRange<Player> GetInGamePlayers()
	{
		return PlayerSlotRange<Player>(players, true);
	}

	virtual void ResetPlayers()
	{
		for (Player *player : players)
		{
			player->Reset();
		}
	}

	void Cleanup();
	void OnLateLoad();

	void OnSteamAPIActivated();

	/*
		The connect events actually happen in this order.
		Only these functions are called for bots: OnClientConnected, OnClientPutInServer, OnClientActive, OnClientDisconnect
	*/

	// These following functions are NOT called upon map change.
	// Note: ConnectClient can be triggered multiple times with MultiAddonsManager.
	void OnConnectClient(const char *pszName, ns_address *pAddr, uint32 steam_handle, C2S_CONNECT_Message *pConnectMsg, const char *pszChallenge,
						 const byte *pAuthTicket, int nAuthTicketLength, bool bIsLowViolence);
	void OnClientConnected(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress, bool bFakePlayer);
	void OnConnectClientPost(const char *pszName, ns_address *pAddr, uint32 steam_handle, C2S_CONNECT_Message *pConnectMsg, const char *pszChallenge,
							 const byte *pAuthTicket, int nAuthTicketLength, bool bIsLowViolence);

	// These following functions are called upon map change.
	void OnClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason);
	void OnClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid);
	void OnClientActive(CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid);
	void OnClientFullyConnect(CPlayerSlot slot);
	void OnClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID);

	void OnClientVoice(CPlayerSlot slot);
	void OnClientSettingsChanged(CPlayerSlot slot);

	STEAM_GAMESERVER_CALLBACK_MANUAL(PlayerManager, OnValidateAuthTicket, ValidateAuthTicketResponse_t, m_CallbackValidateAuthTicketResponse);

	void RegisterSteamAPICallback()
	{
		if (callbackRegistered)
		{
			return;
		}
		callbackRegistered = true;
		m_CallbackValidateAuthTicketResponse.Register(this, &PlayerManager::OnValidateAuthTicket);
	}

	// Auth queue management methods
	void AddToAuthQueue(CPlayerSlot slot);
	void RemoveFromAuthQueue(CPlayerSlot slot);
	void PerformAuthChecks();
	void AuthorizeClient(CPlayerSlot slot);

private:
	bool callbackRegistered {};
	std::vector<CPlayerSlot> authQueue; // Queue of players pending auth validation

public:
	// Indexed by player slot, and zero-initialized because PlayerManager's constructor deletes every slot before allocating.
	Player *players[MAXPLAYERS] {};
};

extern PlayerManager *g_pPlayerManager;
