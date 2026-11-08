#pragma once
#include "kz/kz.h"
#include "sdk/entity/cbaseplayerweapon.h"

class CUserCmd;

// A player who loses their connection or goes to spectate keeps a persistent pawn.
// That way, interactions with the world and other players can continue, and the player can come back to it.
// Member functions are in kz_away.cpp, the static ones that handle server-wide events in events.cpp.
class KZAwayService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	// Slot of the player who stays away through the disconnect CS2 is handling.
	inline static i32 leavingSlot = -1;
	// Whether that player's pawn was marked as not alive.
	inline static bool protectedPawn {};
	// Slot of the returning player whose team is hidden while CS2 activates their client, and that team.
	inline static i32 activatingSlot = -1;
	inline static i32 activatingTeam {};
	// Whether KZ's per-tick logic runs around the command of a pawn the controller did not drive.
	inline static bool simulatingPersistentPawn {};
	// Whether the engine is handling a connection request. A client it disconnects meanwhile is the prior connection of the
	// same player coming back from the same address.
	inline static bool connectingClient {};

	// Away only. The realtime at which the client left, 0 while the player is here.
	f64 awaySince {};
	// Away only. Set when an away player's client comes back, until the client is active again.
	bool reconnecting {};
	// Spectate only. Set while the player views through their observer pawn and their own pawn persists, which makes KZ
	// simulate and revive that pawn and hide it from its owner.
	bool observingWithPersistentPawn {};

	// What the game takes off a leaving player's pawn, given back as it was.
	QAngle persistentAngles {0.0f, 0.0f, 0.0f};
	bool leftHanded {};
	// Away only. CS2 strips the weapons of a leaving player.
	std::vector<EconInfo> weapons;
	i32 activeWeapon = -1;
	// The weapon the pawn holds, selected by the commands it runs while the player is not controlling it.
	CHandle<CBasePlayerWeapon> heldWeapon;
	// The angles still have to reach the client once the player controls the pawn again.
	bool snapAngles {};
	// Spectate only. The team of the persistent pawn, while the controller shows as a spectator.
	i32 persistentTeam {};
	// Spectate only. Until the client applies the view angle snap with this index, its commands still come from the observer
	// camera, which is right-handed. -1 until the snap is sent.
	i64 snapIndex = -1;
	// Spectate only. Commands that still keep the pawn's hand, in case the client never reports the snap.
	i32 holdHandCommands {};
	// Server frame in which KZ's per-tick logic last ran for the player.
	i32 simulatedFrame {};

	void SaveLoadout();
	void RestoreLoadout();
	void RevivePersistentPawn();
	// Whether the player's pawn and state persist when their client leaves.
	bool CanStayAway();
	// Lets go of the persistent pawn and state, and the slot held for the player.
	void ReleasePersistentState();
	static void ReleaseHeldSlot(CPlayerSlot slot);
	static void KeepSlotFree();

public:
	virtual void Reset() override;

	// The client left, but the pawn and the player's state persist for a reconnect.
	bool IsAway()
	{
		return this->awaySince != 0.0;
	}

	bool IsObservingWithPersistentPawn()
	{
		return this->observingWithPersistentPawn;
	}

	// Whether the player controls their pawn, rather than it persisting while they are away or spectating.
	bool CanControlPawn(bool showError = false);
	// Whether going to spectate leaves the player's pawn in the world instead of removing it.
	bool CanSpectateWithPersistentPawn();
	// Switches the player's view to their observer pawn and leaves the player pawn to run on its own.
	void StartSpectating();
	// Gives a spectating player control of their persistent pawn back. leaving is set when the player disconnects instead.
	void RestorePersistentPawn(bool leaving = false);

	// Returns whether the player's client came back and is now active, after handing the persistent pawn to it.
	bool OnReconnect();
	// Returns whether KZ's per-tick logic runs around the controller's commands this tick.
	bool OnControllerSimulatePre();
	void OnControllerSimulatePost();
	void OnPlayerRunCommandPre(CUserCmd *cmd);
	void OnPlayerRunCommandPost();
	void OnTeleport(const QAngle *angles);

	static void OnConnectClientPre()
	{
		KZAwayService::connectingClient = true;
	}

	static void OnConnectClientPost()
	{
		KZAwayService::connectingClient = false;
	}

	static void OnClientConnect(CPlayerSlot slot, u64 xuid);
	static void OnClientDisconnectPre(CPlayerSlot slot, ENetworkDisconnectionReason reason);
	// Returns whether the player stays away instead of leaving.
	static bool OnClientDisconnectPost(CPlayerSlot slot);
	// Returns whether the team change is blocked.
	static bool OnChangeTeamPre(CCSPlayerController *controller, i32 team);
	// Hides a returning player's team while CS2 activates their client, which would otherwise respawn the persistent pawn.
	static void OnClientActivePre(CPlayerSlot slot);
	static void OnClientActivePost(CPlayerSlot slot);
	static void OnGameFrame();
	static void OnServerGamePostSimulate();

	// Seconds until every away player has been away for the given time, 0 once they all have.
	static f64 GetAwayTimeLeft(f64 limit);
	// Lets go of every away player, whose pawns do not survive a map change.
	static void ReleaseAwayPlayers();
	static void Shutdown();
};
