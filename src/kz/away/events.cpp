#include "kz_away.h"
#include "kz/global/kz_global.h"
#include "kz/recording/kz_recording.h"
#include "kz/spec/kz_spec.h"
#include "kz/timer/kz_timer.h"
#include "engine2/serversideclient.h"
#include "engine2/inetworkserverservice.h"
#include "engine2/networkgameserverbase.h"

#include "tier0/memdbgon.h"

// Only a client that dropped or quit on its own gets to come back, never one the server removed.
static_function bool IsClientSideDisconnect(ENetworkDisconnectionReason reason)
{
	switch (reason)
	{
		case NETWORK_DISCONNECT_DISCONNECT_BY_USER:
		case NETWORK_DISCONNECT_LOST:
		case NETWORK_DISCONNECT_STEAM_DROPPED:
		case NETWORK_DISCONNECT_TIMEDOUT:
		case NETWORK_DISCONNECT_CONNECTION_FAILURE:
		case NETWORK_DISCONNECT_RECONNECTION:
		case NETWORK_DISCONNECT_LOCALPROBLEM_MANYRELAYS:
		case NETWORK_DISCONNECT_LOCALPROBLEM_HOSTEDSERVERPRIMARYRELAY:
		case NETWORK_DISCONNECT_LOCALPROBLEM_NETWORKCONFIG:
		case NETWORK_DISCONNECT_LOCALPROBLEM_OTHER:
		case NETWORK_DISCONNECT_REMOTE_TIMEOUT:
		case NETWORK_DISCONNECT_REMOTE_OTHER:
			return true;
		default:
			return false;
	}
}

static_function bool IsKickOrBan(ENetworkDisconnectionReason reason)
{
	switch (reason)
	{
		case NETWORK_DISCONNECT_KICKED:
		case NETWORK_DISCONNECT_BANADDED:
		case NETWORK_DISCONNECT_KICKBANADDED:
		case NETWORK_DISCONNECT_REJECT_BANNED:
			return true;
		default:
			return reason >= NETWORK_DISCONNECT_KICKED_TEAMKILLING && reason <= NETWORK_DISCONNECT_KICKED_INSECURECLIENT;
	}
}

// Slots the engine can still give a new connection: ones it has not created yet, and ones nobody uses or holds.
static_function i32 CountFreeSlots()
{
	CUtlVector<CServerSideClientBase *> *clients = g_pKZUtils->GetClientList();
	INetworkGameServer *server = g_pNetworkServerService ? g_pNetworkServerService->GetIGameServer() : nullptr;
	if (!clients || !server)
	{
		return MAX_PLAYERS;
	}
	i32 free = server->GetMaxClients() - clients->Count();
	for (i32 i = 0; i < clients->Count(); i++)
	{
		CServerSideClientBase *client = clients->Element(i);
		if (client && !client->IsConnected() && client->GetSlotReservation() != CLIENT_SLOT_NOT_RECYCLABLE)
		{
			free++;
		}
	}
	return free;
}

void KZAwayService::ReleaseHeldSlot(CPlayerSlot slot)
{
	CUtlVector<CServerSideClientBase *> *clients = g_pKZUtils->GetClientList();
	if (!clients || slot.Get() < 0 || slot.Get() >= clients->Count())
	{
		return;
	}
	CServerSideClientBase *client = clients->Element(slot.Get());
	if (!client || client->m_nSlotReservation != CLIENT_SLOT_NOT_RECYCLABLE)
	{
		return;
	}
	client->m_nSlotReservation = CLIENT_SLOT_RECYCLABLE;
	// A client that reconnected for a map change still uses its address.
	if (client->IsConnected())
	{
		return;
	}
	// The slot counts as held while it still has an address, the engine clears it the same way for a recyclable slot.
	client->m_Addr = ns_address();
	reinterpret_cast<netadr_t *>(&client->m_Addr)->Clear();
}

// The engine turns a connection away once every slot is used or held, before any plugin hears of it. Keeping one slot free
// lets the player who has been away the longest go only when the server fills up.
void KZAwayService::KeepSlotFree()
{
	if (CountFreeSlots() > 0)
	{
		return;
	}
	KZPlayer *longestAway = nullptr;
	for (KZPlayer *player : g_pKZPlayerManager->GetAllSlots())
	{
		if (player->IsAway() && (!longestAway || player->awayService->awaySince < longestAway->awayService->awaySince))
		{
			longestAway = player;
		}
	}
	if (longestAway)
	{
		KZ_LOG_INFO(LogChannel::Player, "The server is full, letting go of %s (slot %d)\n", longestAway->GetName(),
					longestAway->GetPlayerSlot().Get());
		longestAway->awayService->ReleasePersistentState();
	}
}

void KZAwayService::OnClientConnect(CPlayerSlot slot, u64 xuid)
{
	for (KZPlayer *other : g_pKZPlayerManager->GetAllSlots())
	{
		// A player who comes back in another slot leaves their persistent pawn behind.
		if (other->IsAway() && other->GetPlayerSlot() != slot && other->GetSteamId64(false) == xuid)
		{
			other->awayService->ReleasePersistentState();
		}
	}

	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (!player->IsAway())
	{
		return;
	}
	if (player->GetSteamId64(false) != xuid)
	{
		player->awayService->ReleasePersistentState();
		return;
	}
	player->awayService->awaySince = 0.0;
	player->awayService->reconnecting = true;
	// The new client is authorized like any other, which joins the API again. Preferences already loaded are kept.
	player->authAuthorized = false;
	KZ_LOG_INFO(LogChannel::Player, "Player %s (slot %d) came back, resuming their persistent pawn and state\n", player->GetName(), slot.Get());
}

void KZAwayService::OnClientDisconnectPre(CPlayerSlot slot, ENetworkDisconnectionReason reason)
{
	KZAwayService::leavingSlot = -1;
	KZAwayService::protectedPawn = false;
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (!player)
	{
		return;
	}
	KZAwayService *away = player->awayService;
	// CS2 removes the pawn of a leaving spectator, a spectating player leaves as the player of their persistent pawn.
	away->RestorePersistentPawn(true);
	// The engine kicks the old connection of a player who connects again from the same address and SteamID before it notices
	// the old one is gone, then hands the slot to the new one.
	if (KZAwayService::connectingClient && reason == NETWORK_DISCONNECT_KICKED)
	{
		KZ_LOG_INFO(LogChannel::Player, "Player %s (slot %d) is replaced by a new connection of their own\n", player->GetName(), slot.Get());
		reason = NETWORK_DISCONNECT_RECONNECTION;
	}
	// A reconnecting client is reported as disconnecting twice, the second time with a reason that would not keep the player.
	// A client that is coming back can also drop again while it loads, for example to load an addon.
	bool stayAway = away->IsAway() || (away->reconnecting && !IsKickOrBan(reason)) || (IsClientSideDisconnect(reason) && away->CanStayAway());
	// A player who already plays in another slot under the same SteamID has nothing to come back to.
	for (KZPlayer *other : g_pKZPlayerManager->GetAllSlots())
	{
		stayAway &= other == player || !other->IsConnected() || other->IsFakeClient() || other->GetSteamId64(false) != player->GetSteamId64(false);
	}
	if (!stayAway)
	{
		KZ_LOG_INFO(LogChannel::Player, "Player %s (slot %d) disconnected with reason %d\n", player->GetName(), slot.Get(), reason);
		return;
	}
	KZAwayService::leavingSlot = slot.Get();
	// Protecting the pawn makes it count as dead.
	bool alive = player->IsAlive();
	// CS2 kills the alive pawn of a leaving player, but leaves one alone that already counts as dead.
	if (alive)
	{
		player->GetPlayerPawn()->m_lifeState(LIFE_DYING);
		KZAwayService::protectedPawn = true;
	}
	if (!away->IsAway() && !away->reconnecting && alive)
	{
		away->SaveLoadout();
		player->recordingService->OnControlChange(RpEvent::RpEventData::ControlEvent::CONTROL_AWAY);
	}
	if (!away->IsAway())
	{
		KZ_LOG_INFO(LogChannel::Player, "Player %s (slot %d) disconnected with reason %d, keeping their pawn and state\n", player->GetName(),
					slot.Get(), reason);
		// The API keeps the preferences a leave carries, so the player leaves it now instead of when the persistent state goes.
		player->globalService->OnClientDisconnect();
	}
}

bool KZAwayService::OnClientDisconnectPost(CPlayerSlot slot)
{
	if (KZAwayService::leavingSlot != slot.Get())
	{
		return false;
	}
	KZAwayService::leavingSlot = -1;
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	KZAwayService *away = player->awayService;
	CCSPlayerController *controller = player->GetController();
	CCSPlayerPawn *pawn = player->GetPlayerPawn();
	// The pawn was only marked as dying to keep CS2 from killing it.
	if (pawn && KZAwayService::protectedPawn)
	{
		pawn->m_lifeState(LIFE_ALIVE);
	}
	if (!controller)
	{
		return false;
	}
	// CS2 strips the weapons of a leaving player.
	if (pawn && player->IsAlive())
	{
		away->RestoreLoadout();
	}
	// CS2 removes the pawns of a disconnected controller once sv_disconnected_players_cleanup_delay has passed since this time.
	controller->m_LastTimePlayerWasDisconnectedForPawnsRemove().SetTime(0.0f);

	// A slot that keeps its address on disconnect is skipped for everyone else and handed back to this player.
	CServerSideClient *client = player->GetClient();
	if (client && client->m_nSlotReservation == CLIENT_SLOT_RECYCLABLE)
	{
		client->m_nSlotReservation = CLIENT_SLOT_NOT_RECYCLABLE;
	}

	// The client's name stays with the player while they are away.
	player->RefreshName();
	g_pKZPlayerManager->RemoveFromAuthQueue(slot);
	player->authPending = false;
	away->reconnecting = false;
	// Persists until the player comes back, the map changes, or the server needs the slot.
	if (!away->IsAway())
	{
		away->awaySince = g_pKZUtils->GetServerGlobals()->realtime;
	}
	return true;
}

bool KZAwayService::OnChangeTeamPre(CCSPlayerController *controller, i32 team)
{
	// Outside competitive modes CS2 moves a leaving player to unassigned, and moving the player back would respawn the pawn.
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	return team == CS_TEAM_NONE && player && KZAwayService::leavingSlot == player->GetPlayerSlot().Get();
}

void KZAwayService::OnClientActivePre(CPlayerSlot slot)
{
	KZAwayService::activatingSlot = -1;
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (!player || !player->awayService->reconnecting || !player->IsAlive())
	{
		return;
	}
	// Activating a client respawns the pawn of a controller on a team, which would undo everything the persistent pawn did.
	CCSPlayerController *controller = player->GetController();
	KZAwayService::activatingSlot = slot.Get();
	KZAwayService::activatingTeam = controller->m_iTeamNum();
	controller->m_iTeamNum(CS_TEAM_NONE);
}

void KZAwayService::OnClientActivePost(CPlayerSlot slot)
{
	if (KZAwayService::activatingSlot != slot.Get())
	{
		return;
	}
	KZAwayService::activatingSlot = -1;
	CCSPlayerController *controller = g_pKZPlayerManager->ToPlayer(slot)->GetController();
	if (controller)
	{
		controller->m_iTeamNum(KZAwayService::activatingTeam);
	}
}

void KZAwayService::OnGameFrame()
{
	KZAwayService::KeepSlotFree();
	for (KZPlayer *player : g_pKZPlayerManager->GetAllSlots())
	{
		KZAwayService *away = player->awayService;
		// A spectator whose persistent pawn died is an ordinary spectator from then on.
		if (away->observingWithPersistentPawn && player->IsInGame() && !player->IsAlive())
		{
			away->observingWithPersistentPawn = false;
			player->GetController()->ChangeTeam(CS_TEAM_SPECTATOR);
			continue;
		}
		// The run is paused once the pawn stands completely still and the rules allow it, until then the pawn keeps going.
		// A spectator can turn that off.
		if (away->CanControlPawn() || !player->IsAlive() || player->timerService->GetPaused()
			|| (away->observingWithPersistentPawn && !player->timerService->GetAutoPauseWhileSpectating()))
		{
			continue;
		}
		Vector velocity;
		player->GetVelocity(&velocity);
		if (velocity.IsZero() && player->timerService->CanPause())
		{
			player->timerService->Pause(false);
		}
	}
}

void KZAwayService::OnServerGamePostSimulate()
{
	// The switch to the observer pawn happens during the controller's simulation. Reviving the persistent pawn before the tick is
	// networked keeps CSTV from ever seeing it dead or without a controller, which it would treat as a death.
	for (KZPlayer *player : g_pKZPlayerManager->GetAllSlots())
	{
		player->awayService->RevivePersistentPawn();
	}
}

void KZAwayService::ReleaseAwayPlayers()
{
	for (KZPlayer *player : g_pKZPlayerManager->GetAllSlots())
	{
		if (player->IsAway())
		{
			player->awayService->ReleasePersistentState();
		}
	}
}

void KZAwayService::Shutdown()
{
	// The player state goes away with the plugin, the held slots must not outlive it.
	for (KZPlayer *player : g_pKZPlayerManager->GetAllSlots())
	{
		if (player->IsAway())
		{
			KZAwayService::ReleaseHeldSlot(player->GetPlayerSlot());
		}
	}
}
