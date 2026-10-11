#include "cs2kz.h"
#include "kz/kz.h"
#include "kz/hud/kz_hud.h"
#include "kz/mode/kz_mode.h"
#include "kz_replay.h"
#include "playback.h"
#include "data.h"
#include "bot.h"
#include "item.h"
#include "events.h"
#include "sdk/usercmd.h"
#include "utils/utils.h"

namespace KZ::replaysystem::playback
{
	struct HeldState
	{
		Vector origin;
		QAngle angles;
		Vector velocity;
	};

	static_global CHandle<CCSPlayerController> clockOwner;
	static_global bool botTimeApplied;
	static_global i32 serverTickCount;
	static_global f32 serverCurtime;

	static_function bool IsTeleportInTick(const data::ReplayPlayback *replay, const TickData *tickData)
	{
		// The tick has not been played yet, so its events start at currentEvent.
		for (u32 i = replay->currentEvent; i < replay->numEvents && replay->events[i].serverTick <= tickData->serverTick; i++)
		{
			if (replay->events[i].type == RPEVENT_TELEPORT && replay->events[i].serverTick == tickData->serverTick)
			{
				return true;
			}
		}
		return false;
	}

	// The bot's state part of the way through currentTick, shown on server ticks that do not simulate a replay tick.
	static_function HeldState GetHeldState(const data::ReplayPlayback *replay, const TickData *tickData)
	{
		// A teleport during the tick cannot be interpolated across, so the bot waits where the tick starts.
		f32 fraction = IsTeleportInTick(replay, tickData) ? 0.0f : replay->tickProgress;
		const TickData::MovementData &pre = tickData->pre;
		const TickData::MovementData &post = tickData->post;
		// A tick's pre angles are already the ones its command asks for, so the turn is from where the last tick ended.
		const QAngle &from = replay->currentTick > 0 ? replay->tickData[replay->currentTick - 1].post.angles : pre.angles;

		HeldState state;
		state.origin = Lerp(fraction, pre.origin, post.origin);
		state.angles.x = utils::LerpAngle(from.x, post.angles.x, fraction);
		state.angles.y = utils::LerpAngle(from.y, post.angles.y, fraction);
		state.angles.z = utils::LerpAngle(from.z, post.angles.z, fraction);
		state.velocity = Lerp(fraction, pre.velocity, post.velocity);
		return state;
	}

	// The bot's clock only moves when a replay tick is simulated. While it simulates, the plugin's copy of the server
	// globals holds that clock, the same way PlayerRunCommand puts it in the engine's globals.
	static_function void ApplyBotTime()
	{
		if (!botTimeApplied)
		{
			serverTickCount = g_KZPlugin.serverGlobals.tickcount;
			serverCurtime = g_KZPlugin.serverGlobals.curtime;
			botTimeApplied = true;
		}
		g_KZPlugin.serverGlobals.tickcount = data::GetBotTickcount();
		g_KZPlugin.serverGlobals.curtime = data::GetBotCurtime();
	}

	static_function void RestoreServerTime()
	{
		if (botTimeApplied)
		{
			g_KZPlugin.serverGlobals.tickcount = serverTickCount;
			g_KZPlugin.serverGlobals.curtime = serverCurtime;
			botTimeApplied = false;
		}
	}

	static_function bool IsPlayingOn(KZPlayer *player)
	{
		return player && bot::IsValidBot(player->GetController()) && data::GetCurrentReplay()->playingReplay;
	}

	bool BeginServerTick(KZPlayer *player)
	{
		if (!IsPlayingOn(player))
		{
			return true;
		}

		auto replay = data::GetCurrentReplay();
		if (clockOwner != player->GetController())
		{
			clockOwner = player->GetController();
			replay->hasLastMoveData = false;
		}

		replay->tickProgress += replay->replayPaused ? 0.0f : replay->timescale;
		u32 ticks = (u32)replay->tickProgress;
		replay->tickProgress -= (f32)ticks;
		if (!replay->hasLastMoveData && ticks == 0)
		{
			// Holding reuses the move data of a simulated tick, so the bot has to simulate one first.
			ticks = 1;
			replay->tickProgress = 0.0f;
		}
		// The replay must not end in the middle of a command, so its last tick is always the one the engine runs.
		replay->pendingTicks = MIN(ticks, replay->tickCount - replay->currentTick);
		replay->holdingTick = replay->pendingTicks == 0;
		if (replay->holdingTick)
		{
			return false;
		}
		BeginReplayTick(player);
		return true;
	}

	void BeginReplayTick(KZPlayer *player)
	{
		if (!IsPlayingOn(player))
		{
			return;
		}
		data::SetBotTickcount(data::GetCurrentReplay()->tickData[data::GetCurrentReplay()->currentTick].serverTick);
		ApplyBotTime();
	}

	bool HasQueuedReplayTicks(KZPlayer *player)
	{
		return IsPlayingOn(player) && data::GetCurrentReplay()->pendingTicks > 1;
	}

	bool IsHoldingTick(KZPlayer *player)
	{
		return IsPlayingOn(player) && data::GetCurrentReplay()->holdingTick;
	}

	void EndServerTick(KZPlayer *player)
	{
		RestoreServerTime();
		if (IsPlayingOn(player))
		{
			data::GetCurrentReplay()->pendingTicks = 0;
			data::GetCurrentReplay()->holdingTick = false;
		}
	}

	void OnPhysicsSimulate(KZPlayer *player)
	{
		if (!player || !bot::IsValidBot(player->GetController()))
		{
			return;
		}

		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		TickData *tickData = &replay->tickData[replay->currentTick];
		auto pawn = player->GetPlayerPawn();

		// Setting the origin via teleport will break client interp, set the values directly
		pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(tickData->pre.origin);
		pawn->m_angEyeAngles(tickData->pre.angles);
		pawn->m_vecAbsVelocity(tickData->pre.velocity);

		auto moveServices = player->GetMoveServices();
		// The bot's clock is the one the replay was recorded on, so recorded times can be used directly instead of being adjusted.
		moveServices->m_LegacyJump().m_flJumpPressedTime = tickData->pre.jumpPressedTime;

		moveServices->m_flDuckSpeed = tickData->pre.duckSpeed;
		moveServices->m_flDuckAmount = tickData->pre.duckAmount;
		moveServices->m_flLastDuckTime = tickData->pre.lastDuckTime;
		moveServices->m_bDucking = tickData->pre.replayFlags.ducking;
		moveServices->m_bDucked = tickData->pre.replayFlags.ducked;
		moveServices->m_bDesiresDuck = tickData->pre.replayFlags.desiresDuck;

		moveServices->m_ModernJump().m_nLastActualJumpPressTick = tickData->modernJump.lastActualJumpPressTick;
		moveServices->m_ModernJump().m_flLastActualJumpPressFrac = tickData->modernJump.lastActualJumpPressFrac;
		moveServices->m_ModernJump().m_nLastUsableJumpPressTick = tickData->modernJump.lastUsableJumpPressTick;
		moveServices->m_ModernJump().m_flLastUsableJumpPressFrac = tickData->modernJump.lastUsableJumpPressFrac;
		moveServices->m_ModernJump().m_nLastLandedTick = tickData->modernJump.lastLandedTick;
		moveServices->m_ModernJump().m_flLastLandedFrac = tickData->modernJump.lastLandedFrac;
		moveServices->m_ModernJump().m_flLastLandedVelocityX = tickData->modernJump.lastLandedVelocity.x;
		moveServices->m_ModernJump().m_flLastLandedVelocityY = tickData->modernJump.lastLandedVelocity.y;
		moveServices->m_ModernJump().m_flLastLandedVelocityZ = tickData->modernJump.lastLandedVelocity.z;

		u32 playerFlagBits = (-1) & ~((u32)(FL_CLIENT | FL_BOT));
		pawn->m_fFlags = (pawn->m_fFlags & ~playerFlagBits) | (tickData->pre.entityFlags & playerFlagBits);
		pawn->v_angle = tickData->pre.angles;
	}

	void OnProcessMovement(KZPlayer *player)
	{
		if (!player || !bot::IsValidBot(player->GetController()))
		{
			return;
		}

		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		auto pawn = player->GetPlayerPawn();
		u32 newFlags = pawn->m_fFlags() | FL_BOT;
		pawn->m_fFlags(newFlags);
	}

	void OnProcessMovementPost(KZPlayer *player)
	{
		if (!player || !bot::IsValidBot(player->GetController()))
		{
			return;
		}

		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		auto pawn = player->GetPlayerPawn();
		u32 newFlags = pawn->m_fFlags() | FL_BOT;
		pawn->m_fFlags(newFlags);
	}

	void OnFinishMovePre(KZPlayer *player, CMoveData *mv)
	{
		if (!player || !bot::IsValidBot(player->GetController()))
		{
			return;
		}

		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		// Setting the origin via teleport will break client interp, set the values directly.
		// We have to do it here to be ahead of SetAbsOrigin calls in FinishMove.
		TickData *tickData = &replay->tickData[replay->currentTick];
		if (replay->holdingTick)
		{
			HeldState held = GetHeldState(replay, tickData);
			mv->m_vecAbsOrigin = held.origin;
			mv->m_vecVelocity = held.velocity;
			mv->m_vecViewAngles = held.angles;
		}
		else
		{
			mv->m_vecAbsOrigin = tickData->post.origin;
			mv->m_vecVelocity = tickData->post.velocity;
			mv->m_vecViewAngles = tickData->post.angles;
		}

		// Directly set the pawn's origin/angles/velocity to something else so that they will be resynchronized.
		auto pawn = player->GetPlayerPawn();
		pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(mv->m_vecAbsOrigin + Vector(0, 0, 1000)); // Move it up so it's obviously wrong
	}

	void OnPhysicsSimulatePost(KZPlayer *player)
	{
		if (!player || !bot::IsValidBot(player->GetController()))
		{
			return;
		}

		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		auto pawn = player->GetPlayerPawn();
		TickData *tickData = &replay->tickData[replay->currentTick];

		auto moveServices = player->GetMoveServices();
		moveServices->m_nButtons().m_pButtonStates[0] = tickData->post.buttons[0];
		moveServices->m_nButtons().m_pButtonStates[1] = tickData->post.buttons[1];
		moveServices->m_nButtons().m_pButtonStates[2] = tickData->post.buttons[2];
		moveServices->m_LegacyJump().m_flJumpPressedTime = tickData->post.jumpPressedTime;

		moveServices->m_flDuckSpeed = tickData->post.duckSpeed;
		moveServices->m_flDuckAmount = tickData->post.duckAmount;
		moveServices->m_flLastDuckTime = tickData->post.lastDuckTime;
		moveServices->m_bDucking = tickData->post.replayFlags.ducking;
		moveServices->m_bDucked = tickData->post.replayFlags.ducked;
		moveServices->m_bDesiresDuck = tickData->post.replayFlags.desiresDuck;
		player->SetMoveType(tickData->post.moveType);
		u32 playerFlagBits = (-1) & ~((u32)(FL_CLIENT | FL_FAKECLIENT | FL_BOT));
		pawn->m_fFlags = (pawn->m_fFlags & ~playerFlagBits) | (tickData->post.entityFlags & playerFlagBits);
		pawn->v_angle = tickData->post.angles;
		pawn->SetAnimGraphUpdateEnabled(true);

		// MoveHeldBot hands this back to FinishMove on server ticks that do not simulate.
		if (player->currentMoveData)
		{
			memcpy(replay->lastMoveData, player->currentMoveData, sizeof(replay->lastMoveData));
			replay->hasLastMoveData = true;
		}

		// Update checkpoint state from current tick data
		replay->currentCpIndex = tickData->checkpoint.index;
		replay->currentCheckpoint = tickData->checkpoint.checkpointCount;
		replay->currentTeleport = tickData->checkpoint.teleportCount;

		// Process jumps and events after physics simulation
		events::CheckJumps(*player);
		events::CheckEvents(*player);

		if (replay->paused && replay->startTime > 0.0f)
		{
			replay->startTime += ENGINE_FIXED_TICK_INTERVAL;
		}

		replay->currentTick++;
		if (replay->pendingTicks > 0)
		{
			replay->pendingTicks--;
		}
		if (replay->currentTick >= replay->tickCount)
		{
			bot::KickBot();
			replay->playingReplay = false;
		}
	}

	// Moves the bot between two replay ticks without simulating, so that nothing but where it is changes.
	static_function void MoveHeldBot(KZPlayer *player, PlayerCommand *command)
	{
		auto replay = data::GetCurrentReplay();
		auto pawn = player->GetPlayerPawn();
		auto moveServices = player->GetMoveServices();
		if (!replay->hasLastMoveData || !pawn || !moveServices)
		{
			return;
		}

		CMoveData *mv = reinterpret_cast<CMoveData *>(replay->lastMoveData);
		mv->m_nPlayerHandle = pawn->GetRefEHandle();
		f32 fallVelocity = moveServices->m_flFallVelocity;
		// OnFinishMovePre fills in the held state.
		static_persist const i32 offset = g_pGameConfig->GetOffset("FinishMove");
		CALL_VIRTUAL(void, offset, moveServices, command, mv);
		moveServices->m_flFallVelocity = fallVelocity;
		// The engine resets a bot's tick base every server tick, and GetCurtime reads it.
		player->GetController()->m_nTickBase(data::GetBotTickcount());

		pawn->m_angEyeAngles(mv->m_vecViewAngles);
		// Freezes the pose on clients until the next replay tick, so slow playback also slows the animations.
		pawn->SetAnimGraphUpdateEnabled(false);
	}

	bool OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command)
	{
		if (!player || !bot::IsValidBot(player->GetController()))
		{
			return true;
		}

		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return true;
		}
		if (replay->currentTick >= replay->tickCount)
		{
			return true;
		}
		if (replay->holdingTick)
		{
			MoveHeldBot(player, command);
			return false;
		}

		// PlayerRunCommand advances the tick base by one and runs the command at that tick.
		player->GetController()->m_nTickBase(data::GetBotTickcount() - 1);

		TickData *tickData = &replay->tickData[replay->currentTick];
		command->mutable_base()->set_forwardmove(tickData->forward);
		command->mutable_base()->set_leftmove(tickData->left);
		command->mutable_base()->set_upmove(tickData->up);

		command->mutable_base()->mutable_viewangles()->Clear();
		if (tickData->pre.angles.x != 0)
		{
			command->mutable_base()->mutable_viewangles()->set_x(tickData->pre.angles.x);
		}
		if (tickData->pre.angles.y != 0)
		{
			command->mutable_base()->mutable_viewangles()->set_y(tickData->pre.angles.y);
		}
		if (tickData->pre.angles.z != 0)
		{
			command->mutable_base()->mutable_viewangles()->set_z(tickData->pre.angles.z);
		}

		// Fast playback runs the same command object for several replay ticks, so nothing may be left over from the last one.
		command->mutable_base()->mutable_buttons_pb()->Clear();
		command->mutable_base()->clear_weaponselect();
		command->clear_left_hand_desired();
		if (tickData->pre.buttons[0])
		{
			command->mutable_base()->mutable_buttons_pb()->set_buttonstate1(tickData->pre.buttons[0]);
		}
		if (tickData->pre.buttons[1])
		{
			command->mutable_base()->mutable_buttons_pb()->set_buttonstate2(tickData->pre.buttons[1]);
		}
		if (tickData->pre.buttons[2])
		{
			command->mutable_base()->mutable_buttons_pb()->set_buttonstate3(tickData->pre.buttons[2]);
		}
		command->buttonstates.m_pButtonStates[0] = tickData->pre.buttons[0];
		command->buttonstates.m_pButtonStates[1] = tickData->pre.buttons[1];
		command->buttonstates.m_pButtonStates[2] = tickData->pre.buttons[2];

		if (tickData->leftHanded)
		{
			command->set_left_hand_desired(true);
		}
		player->SetMoveType(tickData->pre.moveType);

		SubtickData *subtickData = &replay->subtickData[replay->currentTick];

		// Bots should never have any subtick move, but who knows?
		command->mutable_base()->clear_subtick_moves();
		for (u32 i = 0; i < subtickData->numSubtickMoves && i < MAX_SUBTICK_MOVES; i++)
		{
			auto move = command->mutable_base()->add_subtick_moves();
			move->set_when(subtickData->subtickMoves[i].when);
			move->set_button(subtickData->subtickMoves[i].button);
			if (!subtickData->subtickMoves[i].IsAnalogInput())
			{
				move->set_pressed(subtickData->subtickMoves[i].pressed);
				if (subtickData->subtickMoves[i].analogMove.pitch_delta != 0)
				{
					move->set_pitch_delta(subtickData->subtickMoves[i].analogMove.pitch_delta);
				}
				if (subtickData->subtickMoves[i].analogMove.yaw_delta != 0)
				{
					move->set_yaw_delta(subtickData->subtickMoves[i].analogMove.yaw_delta);
				}
			}
			else
			{
				if (subtickData->subtickMoves[i].analogMove.analog_forward_delta != 0)
				{
					move->set_analog_forward_delta(subtickData->subtickMoves[i].analogMove.analog_forward_delta);
				}
				if (subtickData->subtickMoves[i].analogMove.analog_left_delta != 0)
				{
					move->set_analog_left_delta(subtickData->subtickMoves[i].analogMove.analog_left_delta);
				}
			}
		}
		KZ_LOG_DEBUG(LogChannel::Replays, "%s\n", command->DebugString().c_str());
		CheckWeapon(*player, *command);
		return true;
	}

	void CheckWeapon(KZPlayer &player, PlayerCommand &cmd)
	{
		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		TickData *tickData = &replay->tickData[replay->currentTick];

		// Check what the current weapon should be.
		i32 weaponIndex = tickData->weapon;
		i32 found = -1;
		for (i32 i = 0; i < replay->weaponTableSize; i++)
		{
			if (replay->weaponIndices[i] == weaponIndex)
			{
				found = i;
				break;
			}
		}
		if (found == -1)
		{
			player.GetPlayerPawn()->m_pWeaponServices()->m_hActiveWeapon(nullptr);
			return;
		}
		EconInfo desiredWeapon = replay->weapons[found];
		gear_slot_t desiredGearSlot = KZ::replaysystem::item::GetWeaponGearSlot(desiredWeapon.mainInfo.itemDef);
		if (desiredGearSlot == gear_slot_t::GEAR_SLOT_C4)
		{
			// The bot can't keep hold of the C4: the game takes it straight back off it and the dropped
			// entity is never destroyed, so replaying it verbatim spawns one world entity per tick.
			// Play it back as a knife instead, preferring one the bot already carries so that nothing
			// has to be given at all.
			desiredGearSlot = gear_slot_t::GEAR_SLOT_KNIFE;
			desiredWeapon = EconInfo();
			desiredWeapon.mainInfo.itemDef = KZ::replaysystem::item::GetDefaultKnifeItemDef();

			CUtlVector<CHandle<CBasePlayerWeapon>> *inventory = player.GetPlayerPawn()->m_pWeaponServices()->m_hMyWeapons();
			for (i32 i = 0; i < inventory->Count(); i++)
			{
				CBasePlayerWeapon *invWeapon = inventory->Element(i).Get();
				if (invWeapon
					&& KZ::replaysystem::item::GetWeaponGearSlot(invWeapon->m_AttributeManager().m_Item().m_iItemDefinitionIndex())
						   == gear_slot_t::GEAR_SLOT_KNIFE)
				{
					desiredWeapon = EconInfo(invWeapon);
					break;
				}
			}
		}

		EconInfo activeWeapon = player.GetPlayerPawn()->m_pWeaponServices()->m_hActiveWeapon().Get();

		if (desiredWeapon != activeWeapon)
		{
			if (desiredWeapon.mainInfo.itemDef == 0)
			{
				// If weapon is 0, then remove the active weapon from the player.
				player.GetPlayerPawn()->m_pWeaponServices()->m_hActiveWeapon(nullptr);
				return;
			}

			// Find the weapon in the player's inventory.
			CUtlVector<CHandle<CBasePlayerWeapon>> *weapons = player.GetPlayerPawn()->m_pWeaponServices()->m_hMyWeapons();
			for (i32 i = 0; i < weapons->Count(); i++)
			{
				CBasePlayerWeapon *invWeapon = weapons->Element(i).Get();
				if (desiredWeapon == EconInfo(invWeapon))
				{
					cmd.mutable_base()->set_weaponselect(invWeapon->entindex());
					return;
				}
			}

			std::string weaponName = KZ::replaysystem::item::GetWeaponName(desiredWeapon.mainInfo.itemDef);
			if (weaponName.empty())
			{
				return;
			}

			// Check if we can get away with not stripping all weapons here.
			for (i32 i = 0; i < weapons->Count(); i++)
			{
				CBasePlayerWeapon *invWeapon = weapons->Element(i).Get();
				gear_slot_t invGearSlot =
					KZ::replaysystem::item::GetWeaponGearSlot(invWeapon->m_AttributeManager().m_Item().m_iItemDefinitionIndex());
				if (invGearSlot == desiredGearSlot)
				{
					// We have a weapon in the same gear slot, so we need to strip all weapons to avoid conflicts.
					player.GetPlayerPawn()->m_pItemServices()->RemoveAllItems(false);
					break;
				}
			}

			CBasePlayerWeapon *newWeapon = player.GetPlayerPawn()->m_pItemServices()->GiveNamedItem(weaponName.c_str());
			if (newWeapon)
			{
				// The weapon stamps its attack timers with server time when it is given, but they run on the bot's clock.
				i32 clockOffset = data::GetBotTickcount() - g_pKZUtils->GetGlobals()->tickcount;
				newWeapon->m_nNextPrimaryAttackTick(newWeapon->m_nNextPrimaryAttackTick() + clockOffset);
				newWeapon->m_nNextSecondaryAttackTick(newWeapon->m_nNextSecondaryAttackTick() + clockOffset);
				KZ::replaysystem::item::ApplyItemAttributesToWeapon(*newWeapon, desiredWeapon);
				cmd.mutable_base()->set_weaponselect(newWeapon->entindex());
			}
		}
	}

	void InitializeWeapons()
	{
		auto replay = data::GetCurrentReplay();
		CCSPlayerController *bot = bot::GetBot();
		if (!bot)
		{
			return;
		}

		CCSPlayerPawn *pawn = bot->GetPlayerPawn();
		pawn->m_pItemServices()->RemoveAllItems(false);
	}

	void StartReplay()
	{
		auto replay = data::GetCurrentReplay();
		if (replay->tickCount == 0)
		{
			return;
		}
		replay->playingReplay = true;
		replay->replayPaused = false;
		replay->timescale = 1.0f;
		replay->tickProgress = 0.0f;
		replay->currentTick = 0;
		ResetBotClock(bot::GetBotPlayer(), replay->tickData[0].serverTick);
	}

	void ResetBotClock(KZPlayer *player, u32 tick)
	{
		data::SetBotTickcount(tick);
		if (!player || !player->GetPlayerPawn())
		{
			return;
		}

		// Anything stamped on the clock before the jump can now be in the future.
		player->takeoffTime = 0.0f;
		player->landingTime = 0.0f;
		player->landingTimeServer = 0.0f;
		player->landingTimeActual = 0.0f;
		player->hudService->Reset();
		player->modeService->Reset();
		CUtlVector<CHandle<CBasePlayerWeapon>> *weapons = player->GetPlayerPawn()->m_pWeaponServices()->m_hMyWeapons();
		for (i32 i = 0; i < weapons->Count(); i++)
		{
			CBasePlayerWeapon *weapon = weapons->Element(i).Get();
			if (!weapon)
			{
				continue;
			}
			weapon->m_nNextPrimaryAttackTick(MIN(weapon->m_nNextPrimaryAttackTick(), (i32)tick));
			weapon->m_nNextSecondaryAttackTick(MIN(weapon->m_nNextSecondaryAttackTick(), (i32)tick));
		}
	}

	void ApplyTickState(KZPlayer *player, const TickData *tickData)
	{
		if (!player || !tickData)
		{
			return;
		}

		auto pawn = player->GetPlayerPawn();

		// Set position and physics state
		pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(tickData->post.origin);
		pawn->m_angEyeAngles(tickData->post.angles);
		pawn->m_vecAbsVelocity(tickData->post.velocity);
		pawn->v_angle = tickData->post.angles;

		// Set movement state
		auto moveServices = player->GetMoveServices();
		moveServices->m_flDuckSpeed = tickData->post.duckSpeed;
		moveServices->m_flDuckAmount = tickData->post.duckAmount;
		moveServices->m_flLastDuckTime = tickData->post.lastDuckTime;
		moveServices->m_bDucking = tickData->post.replayFlags.ducking;
		moveServices->m_bDucked = tickData->post.replayFlags.ducked;
		moveServices->m_bDesiresDuck = tickData->post.replayFlags.desiresDuck;
		player->SetMoveType(tickData->post.moveType);

		// Set entity flags
		u32 playerFlagBits = (-1) & ~((u32)(FL_CLIENT | FL_FAKECLIENT | FL_BOT));
		pawn->m_fFlags = (pawn->m_fFlags & ~playerFlagBits) | (tickData->post.entityFlags & playerFlagBits);
	}

} // namespace KZ::replaysystem::playback
