#include "kz_away.h"
#include "cs2kz.h"
#include "kz/language/kz_language.h"
#include "kz/racing/kz_racing.h"
#include "kz/recording/kz_recording.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/timer/kz_timer.h"
#include "sdk/entity/cteam.h"
#include "sdk/usercmd.h"

#include "tier0/memdbgon.h"

void KZAwayService::Reset()
{
	this->awaySince = 0.0;
	this->reconnecting = false;
	this->observingWithPersistentPawn = false;
	this->persistentAngles = {0.0f, 0.0f, 0.0f};
	this->leftHanded = false;
	this->weapons.clear();
	this->activeWeapon = -1;
	this->heldWeapon.Term();
	this->snapAngles = false;
	this->persistentTeam = 0;
	this->snapIndex = -1;
	this->holdHandCommands = 0;
	this->simulatedFrame = 0;
}

// CS2 itself keeps the controller and pawn of a player on a team, unless they were still loading.
bool KZAwayService::CanStayAway()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV() || !this->player->IsAlive())
	{
		return false;
	}
	// Without a run or a race there is nothing worth coming back to. A racer between attempts has no timer running.
	if (!this->player->timerService->GetTimerRunning() && !this->player->racingService->IsRacing())
	{
		return false;
	}
	CCSPlayerController *controller = this->player->GetController();
	if (!controller || controller->m_iConnected() == PlayerConnectedState::PlayerConnecting)
	{
		return false;
	}
	return controller->m_iTeamNum() == CS_TEAM_T || controller->m_iTeamNum() == CS_TEAM_CT;
}

void KZAwayService::SaveLoadout()
{
	this->persistentAngles = {0.0f, 0.0f, 0.0f};
	this->weapons.clear();
	this->activeWeapon = -1;
	this->heldWeapon.Term();
	this->player->GetAngles(&this->persistentAngles);
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	this->leftHanded = pawn->m_bLeftHanded();
	CBasePlayerWeapon *activeWeapon = pawn->m_pWeaponServices()->m_hActiveWeapon().Get();
	CUtlVector<CHandle<CBasePlayerWeapon>> *weapons = pawn->m_pWeaponServices()->m_hMyWeapons();
	for (i32 i = 0; i < weapons->Count(); i++)
	{
		CBasePlayerWeapon *weapon = weapons->Element(i).Get();
		if (!weapon)
		{
			continue;
		}
		if (weapon == activeWeapon)
		{
			this->activeWeapon = (i32)this->weapons.size();
			this->heldWeapon = weapon;
		}
		this->weapons.push_back(EconInfo(weapon));
	}
}

void KZAwayService::RestoreLoadout()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	this->heldWeapon.Term();
	pawn->m_pItemServices()->RemoveAllItems(false);
	for (i32 i = 0; i < (i32)this->weapons.size(); i++)
	{
		std::string name = KZ::replaysystem::item::GetWeaponName(this->weapons[i].mainInfo.itemDef);
		CBasePlayerWeapon *weapon = name.empty() ? nullptr : pawn->m_pItemServices()->GiveNamedItem(name.c_str());
		if (!weapon)
		{
			continue;
		}
		KZ::replaysystem::item::ApplyItemAttributesToWeapon(*weapon, this->weapons[i]);
		if (i == this->activeWeapon)
		{
			this->heldWeapon = weapon;
		}
	}
}

// The controller kills the pawn it switches away from and puts it to sleep, which also stops it from touching triggers.
void KZAwayService::RevivePersistentPawn()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	CCSPlayerController *controller = this->player->GetController();
	if (!this->observingWithPersistentPawn || !pawn || controller->GetCurrentPawn() == pawn)
	{
		return;
	}
	// The switch also takes the controller away from the pawn, and the game runs no command for a pawn without one.
	if (pawn->m_hController().Get() != controller)
	{
		pawn->m_hController(controller);
	}
	if (pawn->m_iPlayerState() != PLAYER_STATE_DORMANT)
	{
		return;
	}
	pawn->m_lifeState(LIFE_ALIVE);
	pawn->SetPlayerState(PLAYER_STATE_ACTIVE);
	// Entering the active state copies the pawn's team to the controller, and a round restart would then move it to that team.
	controller->m_iPendingTeamNum(CS_TEAM_SPECTATOR);
	// Entering the active state makes the pawn walk again.
	if (this->player->timerService->GetPaused())
	{
		this->player->SetMoveType(MOVETYPE_NONE);
	}
}

void KZAwayService::ReleasePersistentState()
{
	CPlayerSlot slot = this->player->GetPlayerSlot();
	KZ_LOG_INFO(LogChannel::Player, "Releasing the persistent pawn and state of %s (slot %d)\n", this->player->GetName(), slot.Get());
	this->player->CleanupPlayer();
	g_pKZPlayerManager->RemoveFromAuthQueue(slot);
	// Resets this service too.
	this->player->Reset();
	KZAwayService::ReleaseHeldSlot(slot);
}

bool KZAwayService::CanControlPawn(bool showError)
{
	if (!this->IsAway() && !this->reconnecting && !this->observingWithPersistentPawn)
	{
		return true;
	}
	if (showError)
	{
		this->player->languageService->PrintChat(true, false, "Not In Control");
		this->player->PlayErrorSound();
	}
	return false;
}

bool KZAwayService::CanSpectateWithPersistentPawn()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV() || !this->CanControlPawn() || !this->player->IsAlive())
	{
		return false;
	}
	i32 team = this->player->GetController()->GetTeam();
	return team == CS_TEAM_T || team == CS_TEAM_CT;
}

// A scoreboard row hides until its next team change if it reaches a team table the scoreboard has not created yet, and the spectator table is
// only created after the scoreboard has seen a spectator. Clients drop all rows on this event, so the tables come first when they are rebuilt.
static_function void RebuildScoreboards()
{
	IGameEvent *event = interfaces::pGameEventManager->CreateEvent("nextlevel_changed");
	if (!event)
	{
		return;
	}
	event->SetString("nextlevel", "");
	event->SetString("mapgroup", "");
	event->SetString("skirmishmode", "");
	for (KZPlayer *player : g_pKZPlayerManager->GetInGamePlayers())
	{
		IGameEventListener2 *listener = player->IsFakeClient() ? nullptr : g_pKZUtils->GetLegacyGameEventListener(player->GetPlayerSlot());
		if (listener)
		{
			listener->FireGameEvent(event);
		}
	}
	interfaces::pGameEventManager->FreeEvent(event);
}

void KZAwayService::StartSpectating()
{
	this->SaveLoadout();
	this->observingWithPersistentPawn = true;
	// The controller views through its observer pawn whenever a desired observer mode is set, whatever its team.
	CCSPlayerController *controller = this->player->GetController();
	controller->m_DesiredObserverMode(OBS_MODE_ROAMING);
	controller->m_hDesiredObserverTarget(this->player->GetPlayerPawn());
	// The client's team menu only sends a team the client is not already on, so the controller shows as a spectator.
	// The team is written directly, a team change would kill the pawn.
	this->persistentTeam = controller->m_iTeamNum();
	CTeam::MovePlayer(controller, this->persistentTeam, CS_TEAM_SPECTATOR);
	controller->m_iTeamNum(CS_TEAM_SPECTATOR);
	controller->m_iPendingTeamNum(CS_TEAM_SPECTATOR);
	RebuildScoreboards();
	this->player->recordingService->OnControlChange(RpEvent::RpEventData::ControlEvent::CONTROL_SPECTATE);
}

void KZAwayService::RestorePersistentPawn(bool leaving)
{
	if (!this->observingWithPersistentPawn)
	{
		return;
	}
	this->RevivePersistentPawn();
	this->observingWithPersistentPawn = false;
	CCSPlayerController *controller = this->player->GetController();
	CTeam::MovePlayer(controller, CS_TEAM_SPECTATOR, this->persistentTeam);
	controller->m_iTeamNum(this->persistentTeam);
	controller->m_iPendingTeamNum(this->persistentTeam);
	controller->m_DesiredObserverMode(OBS_MODE_NONE);
	controller->m_hDesiredObserverTarget(CHandle<CCSPlayerPawn>());
	// The controller would only switch back on its next tick, and whatever the caller does next (a teleport, a team change) has
	// to happen to the pawn the controller uses. This switches the same way the game does.
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	CCSPlayerPawnBase *observerPawn = controller->m_hObserverPawn().Get();
	if (pawn && controller->GetCurrentPawn() != pawn)
	{
		// SetPawn takes the new pawn away from the controller it points to first, which would be this one.
		pawn->m_hController(CHandle<CBasePlayerController>());
		g_pKZUtils->SetPawn(controller, pawn, true, false, false, true);
		if (observerPawn)
		{
			observerPawn->SetPlayerState(PLAYER_STATE_DORMANT);
			observerPawn->m_lifeState(LIFE_DEAD);
		}
	}
	// The client keeps its observer view angles until it uses the pawn, so the view is set once the player's commands drive it.
	this->snapAngles = true;
	this->snapIndex = -1;
	// About a second of commands, the client normally applies the snap within its latency.
	this->holdHandCommands = 64;
	// A player who leaves while spectating stays paused and keeps going without control.
	if (leaving)
	{
		return;
	}
	this->player->recordingService->OnControlChange(RpEvent::RpEventData::ControlEvent::CONTROL_RETURN);
	// The run picks up again with the player, unless they turned that off.
	if (this->player->timerService->GetPaused() && this->player->timerService->GetAutoResumeAfterSpectating())
	{
		this->player->timerService->Resume();
	}
}

bool KZAwayService::OnReconnect()
{
	if (!this->reconnecting)
	{
		return false;
	}
	this->reconnecting = false;
	// A player who left while spectating comes back in control of the pawn.
	this->RestorePersistentPawn();
	// The new client starts with its own view angles.
	this->player->SetAngles(this->persistentAngles);
	// The new client numbers its commands from 1 again.
	this->player->recordingService->lastCmdNumReceived = 0;
	this->player->recordingService->OnControlChange(RpEvent::RpEventData::ControlEvent::CONTROL_RETURN);
	// A returning player gets a moment to settle in, if the pawn did not come to rest while they were away.
	KZTimerService *timer = this->player->timerService;
	if (timer->GetTimerRunning() && !timer->GetPaused() && timer->CanPause())
	{
		timer->Pause(false);
	}
	this->player->languageService->PrintChat(true, false, timer->GetPaused() ? "Away Restored (Paused)" : "Away Restored");
	// The resumed run never started under the new connection, so this is the only warning that Steam has not confirmed it yet,
	// for example while Steam is down. Until it does, the run is not saved.
	if (!this->player->IsAuthenticated())
	{
		this->player->languageService->PrintChat(true, false, "No Steam Authentication Warning");
	}
	return true;
}

bool KZAwayService::OnControllerSimulatePre()
{
	// The persistent pawn of a spectator runs its own command, and the per-tick logic runs around that one instead.
	if (this->observingWithPersistentPawn)
	{
		return false;
	}
	this->simulatedFrame = g_pKZUtils->GetServerGlobals(true)->tickcount;
	return true;
}

void KZAwayService::OnControllerSimulatePost()
{
	CCSPlayerController *controller = this->player->GetController();
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!this->observingWithPersistentPawn || !controller || !pawn || controller->GetCurrentPawn() == pawn)
	{
		return;
	}
	// The game only simulates the pawn a connected controller uses. The pawn's own simulation runs an empty command when the
	// controller counts as not connected, the way it does for a player who is away, and puts the clock where the game would.
	this->RevivePersistentPawn();
	static_persist const i32 offset = g_pGameConfig->GetOffset("EntityPhysicsSimulate");
	u32 tickBase = controller->m_nTickBase();
	CHandle<CCSPlayerPawn> currentPawn = controller->m_hPawn();
	controller->m_nTickBase(g_pKZUtils->GetGlobals()->tickcount - 1);
	controller->m_iConnected(PlayerConnectedState::PlayerDisconnected);
	// Movement only runs for the pawn the controller uses.
	controller->m_hPawn(pawn);
	CALL_VIRTUAL(void, offset, pawn);
	controller->m_hPawn(currentPawn);
	controller->m_iConnected(PlayerConnectedState::PlayerConnected);
	controller->m_nTickBase(tickBase);
}

void KZAwayService::OnPlayerRunCommandPre(CUserCmd *cmd)
{
	CBaseUserCmdPB *base = cmd->mutable_base();
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	// The commands a client sends run inside the controller's simulation. A pawn that runs a command outside of it is one the
	// controller did not drive this tick: a player who is away, loading back in or spectating.
	if (g_KZPlugin.simulatingPhysics)
	{
		if (!this->CanControlPawn() || !pawn)
		{
			return;
		}
		if (this->snapAngles)
		{
			this->snapAngles = false;
			base->mutable_viewangles()->set_x(this->persistentAngles.x);
			base->mutable_viewangles()->set_y(this->persistentAngles.y);
			base->mutable_viewangles()->set_z(this->persistentAngles.z);
			g_pKZUtils->SnapViewAngles(pawn, this->persistentAngles);
			CSchemaCollection<ViewAngleServerChange_t> changes = pawn->m_ServerViewAngleChanges();
			if (changes.Count() > 0)
			{
				this->snapIndex = changes.Element(changes.Count() - 1)->nIndex();
			}
		}
		if (this->holdHandCommands > 0)
		{
			this->holdHandCommands--;
			cmd->set_left_hand_desired(this->leftHanded);
			if (this->snapIndex >= 0 && (i64)base->consumed_server_angle_changes() >= this->snapIndex)
			{
				this->holdHandCommands = 0;
			}
		}
		return;
	}

	// The command the pawn builds for itself carries no input, this keeps it that way and adds what the player last had.
	base->set_forwardmove(0.0f);
	base->set_leftmove(0.0f);
	base->set_upmove(0.0f);
	base->mutable_buttons_pb()->Clear();
	base->clear_subtick_moves();
	base->clear_weaponselect();
	// The persistent state also covers the tick between giving the pawn back and the controller driving it.
	if (!this->CanControlPawn() || this->snapAngles)
	{
		base->mutable_viewangles()->set_x(this->persistentAngles.x);
		base->mutable_viewangles()->set_y(this->persistentAngles.y);
		base->mutable_viewangles()->set_z(this->persistentAngles.z);
		CBasePlayerWeapon *weapon = this->heldWeapon.Get();
		if (weapon && pawn && pawn->m_pWeaponServices()->m_hActiveWeapon().Get() != weapon)
		{
			base->set_weaponselect(weapon->entindex());
		}
		cmd->set_left_hand_desired(this->leftHanded);
	}
	cmd->buttonstates.m_pButtonStates[0] = 0;
	cmd->buttonstates.m_pButtonStates[1] = 0;
	cmd->buttonstates.m_pButtonStates[2] = 0;

	// The controller hook carries the player's tick only while the controller drives the pawn, otherwise this command does.
	i32 frame = g_pKZUtils->GetServerGlobals(true)->tickcount;
	if (this->simulatedFrame != frame)
	{
		this->simulatedFrame = frame;
		g_KZPlugin.simulatingPhysics = true;
		KZAwayService::simulatingPersistentPawn = true;
		this->player->OnPhysicsSimulate();
	}
}

void KZAwayService::OnPlayerRunCommandPost()
{
	if (!KZAwayService::simulatingPersistentPawn)
	{
		return;
	}
	KZAwayService::simulatingPersistentPawn = false;
	this->player->OnPhysicsSimulatePost();
	g_KZPlugin.simulatingPhysics = false;
}

void KZAwayService::OnTeleport(const QAngle *angles)
{
	// When it comes from MovementPlayer::SetAngles, the teleport carries no pitch, since that would tilt the pawn's player model
	// instead of the camera.
	const QAngle *viewAngles = this->player->settingViewAngles ? this->player->settingViewAngles : angles;
	if (viewAngles && (!this->CanControlPawn() || this->snapAngles))
	{
		this->persistentAngles = *viewAngles;
	}
}
