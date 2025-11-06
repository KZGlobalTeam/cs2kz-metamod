#include "cs2kz.h"
#include "kz/kz.h"
#include "kz_replay.h"
#include "kz_replayplayback.h"
#include "kz_replaydata.h"
#include "kz_replaybot.h"
#include "kz_replayitem.h"
#include "kz_replayevents.h"
#include "sdk/usercmd.h"

namespace KZ::replaysystem::playback
{

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
		moveServices->m_flJumpPressedTime = tickData->pre.jumpPressedTime;

		moveServices->m_flDuckSpeed = tickData->pre.duckSpeed;
		moveServices->m_flDuckAmount = tickData->pre.duckAmount;
		moveServices->m_flDuckOffset = tickData->pre.duckOffset;
		moveServices->m_flLastDuckTime = g_pKZUtils->GetServerGlobals()->curtime + tickData->pre.lastDuckTime - tickData->gameTime;
		moveServices->m_bDucking = tickData->pre.replayFlags.ducking;
		moveServices->m_bDucked = tickData->pre.replayFlags.ducked;
		moveServices->m_bDesiresDuck = tickData->pre.replayFlags.desiresDuck;

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
		mv->m_vecAbsOrigin = tickData->post.origin;
		mv->m_vecVelocity = tickData->post.velocity;
		mv->m_vecViewAngles = tickData->post.angles;

		// Directly set the pawn's origin/angles/velocity to something else so that they will be resynchronized.
		auto pawn = player->GetPlayerPawn();
		pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(tickData->post.origin + Vector(0, 0, 1000)); // Move it up so it's obviously wrong
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
		moveServices->m_nButtons()->m_pButtonStates[0] = tickData->post.buttons[0];
		moveServices->m_nButtons()->m_pButtonStates[1] = tickData->post.buttons[1];
		moveServices->m_nButtons()->m_pButtonStates[2] = tickData->post.buttons[2];
		moveServices->m_flJumpPressedTime = tickData->post.jumpPressedTime;

		moveServices->m_flDuckSpeed = tickData->post.duckSpeed;
		moveServices->m_flDuckAmount = tickData->post.duckAmount;
		moveServices->m_flDuckOffset = tickData->post.duckOffset;
		moveServices->m_flLastDuckTime = g_pKZUtils->GetServerGlobals()->curtime + tickData->post.lastDuckTime - tickData->gameTime;
		moveServices->m_bDucking = tickData->post.replayFlags.ducking;
		moveServices->m_bDucked = tickData->post.replayFlags.ducked;
		moveServices->m_bDesiresDuck = tickData->post.replayFlags.desiresDuck;
		player->SetMoveType(tickData->post.moveType);
		u32 playerFlagBits = (-1) & ~((u32)(FL_CLIENT | FL_FAKECLIENT | FL_BOT));
		pawn->m_fFlags = (pawn->m_fFlags & ~playerFlagBits) | (tickData->post.entityFlags & playerFlagBits);
		pawn->v_angle = tickData->post.angles;

		// If replay is paused, disable gravity and freeze the bot at current tick
		if (replay->replayPaused)
		{
			// Disable gravity to prevent physics interference
			pawn->SetGravityScale(0.0f);
			// Keep velocity at zero to prevent movement
			pawn->m_vecAbsVelocity(Vector(0, 0, 0));
			// Don't advance tick or process events/jumps
			if (replay->startTime > 0.0f)
			{
				replay->startTime += ENGINE_FIXED_TICK_INTERVAL;
			}
			return;
		}

		// Restore normal gravity when not paused
		pawn->SetGravityScale(1.0f);

		// Update checkpoint state from current tick data
		if (tickData->checkpoint.index != 0 || tickData->checkpoint.checkpointCount != 0 || tickData->checkpoint.teleportCount != 0)
		{
			replay->currentCpIndex = tickData->checkpoint.index;
			replay->currentCheckpoint = tickData->checkpoint.checkpointCount;
			replay->currentTeleport = tickData->checkpoint.teleportCount;
		}

		// Process jumps and events after physics simulation
		events::CheckJumps(*player);
		events::CheckEvents(*player);

		if (replay->paused && replay->startTime > 0.0f)
		{
			replay->startTime += ENGINE_FIXED_TICK_INTERVAL;
		}

		replay->currentTick++;
		if (replay->currentTick >= replay->tickCount)
		{
			bot::KickBot();
			replay->playingReplay = false;
		}
	}

	void OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command)
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
		if (replay->currentTick >= replay->tickCount)
		{
			return;
		}

		TickData *tickData = &replay->tickData[replay->currentTick];
		if (tickData->forward)
		{
			command->mutable_base()->set_forwardmove(tickData->forward);
		}
		if (tickData->left)
		{
			command->mutable_base()->set_leftmove(tickData->left);
		}
		if (tickData->up)
		{
			command->mutable_base()->set_upmove(tickData->up);
		}

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

		if (tickData->pre.buttons[0])
		{
			command->mutable_base()->mutable_buttons_pb()->set_buttonstate1(tickData->pre.buttons[0]);
			command->buttonstates.buttons[0] = tickData->pre.buttons[0];
		}
		if (tickData->pre.buttons[1])
		{
			command->mutable_base()->mutable_buttons_pb()->set_buttonstate2(tickData->pre.buttons[1]);
			command->buttonstates.buttons[1] = tickData->pre.buttons[1];
		}
		if (tickData->pre.buttons[2])
		{
			command->mutable_base()->mutable_buttons_pb()->set_buttonstate3(tickData->pre.buttons[2]);
			command->buttonstates.buttons[2] = tickData->pre.buttons[2];
		}

		if (tickData->leftHanded)
		{
			command->set_left_hand_desired(true);
		}
		player->SetMoveType(tickData->pre.moveType);

		SubtickData *subtickData = &replay->subtickData[replay->currentTick];

		// Bots should never have any subtick move, but who knows?
		command->mutable_base()->clear_subtick_moves();
		for (u32 i = 0; i < subtickData->numSubtickMoves && i < 64; i++)
		{
			auto move = command->mutable_base()->add_subtick_moves();
			move->set_when(subtickData->subtickMoves[i].when);
			move->set_button(subtickData->subtickMoves[i].button);
			if (!subtickData->subtickMoves[i].IsAnalogInput())
			{
				move->set_pressed(subtickData->subtickMoves[i].pressed);
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
				if (subtickData->subtickMoves[i].analogMove.pitch_delta != 0)
				{
					move->set_pitch_delta(subtickData->subtickMoves[i].analogMove.pitch_delta);
				}
				if (subtickData->subtickMoves[i].analogMove.yaw_delta != 0)
				{
					move->set_yaw_delta(subtickData->subtickMoves[i].analogMove.yaw_delta);
				}
			}
		}
		CheckWeapon(*player, *command);
	}

	void CheckWeapon(KZPlayer &player, PlayerCommand &cmd)
	{
		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		TickData *tickData = &replay->tickData[replay->currentTick];
		TickData *finalTickData = &replay->tickData[replay->tickCount - 1];

		// Check what the current weapon should be.
		EconInfo desiredWeapon;
		if (replay->currentWeapon < replay->numWeapons && replay->weapons[replay->currentWeapon + 1].serverTick <= tickData->serverTick)
		{
			replay->currentWeapon++;
		}
		desiredWeapon = replay->weapons[replay->currentWeapon].econInfo;

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

			// Check if we can get away with not stripping all weapons here.
			gear_slot_t desiredGearSlot = KZ::replaysystem::item::GetWeaponGearSlot(desiredWeapon.mainInfo.itemDef);
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

			std::string weaponName = KZ::replaysystem::item::GetWeaponName(desiredWeapon.mainInfo.itemDef);
			CBasePlayerWeapon *newWeapon = player.GetPlayerPawn()->m_pItemServices()->GiveNamedItem(weaponName.c_str());
			if (newWeapon)
			{
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
		u16 itemDef = replay->weapons[0].econInfo.mainInfo.itemDef;
		if (itemDef == 0)
		{
			return;
		}

		std::string weaponName = KZ::replaysystem::item::GetWeaponName(itemDef);
		CBasePlayerWeapon *weapon = pawn->m_pItemServices()->GiveNamedItem(weaponName.c_str());
		if (!weapon)
		{
			assert(false);
		}

		KZ::replaysystem::item::ApplyItemAttributesToWeapon(*weapon, replay->weapons[0].econInfo);
	}

	void StartReplay()
	{
		auto replay = data::GetCurrentReplay();
		replay->playingReplay = true;
		replay->replayPaused = false;
		replay->currentTick = 0;
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
		moveServices->m_flDuckOffset = tickData->post.duckOffset;
		moveServices->m_flLastDuckTime = g_pKZUtils->GetServerGlobals()->curtime + tickData->post.lastDuckTime - tickData->gameTime;
		moveServices->m_bDucking = tickData->post.replayFlags.ducking;
		moveServices->m_bDucked = tickData->post.replayFlags.ducked;
		moveServices->m_bDesiresDuck = tickData->post.replayFlags.desiresDuck;
		player->SetMoveType(tickData->post.moveType);

		// Set entity flags
		u32 playerFlagBits = (-1) & ~((u32)(FL_CLIENT | FL_FAKECLIENT | FL_BOT));
		pawn->m_fFlags = (pawn->m_fFlags & ~playerFlagBits) | (tickData->post.entityFlags & playerFlagBits);
	}

} // namespace KZ::replaysystem::playback
