#include "kz_trigger.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"

void KZTriggerService::ResetBhopState()
{
	this->lastTouchedSingleBhop = CEntityHandle();
	// all hail fixed buffers
	this->lastTouchedSequentialBhops = CSequentialBhopBuffer();
}

void KZTriggerService::UpdateModifiersInternal()
{
	if (this->modifiers.enableSlideCount > 0)
	{
		this->ApplySlide();
	}
	else
	{
		this->CancelSlide();
	}

	if (this->antiBhopActive)
	{
		this->ApplyAntiBhop();
	}
	else
	{
		this->CancelAntiBhop();
	}

	if (this->modifiers.forcedDuckCount > 0)
	{
		this->ApplyForcedDuck();
	}
	else if (this->lastModifiers.forcedDuckCount > 0)
	{
		this->CancelForcedDuck();
	}

	if (this->modifiers.forcedUnduckCount > 0)
	{
		this->ApplyForcedUnduck();
	}
	else if (this->lastModifiers.forcedUnduckCount > 0)
	{
		this->CancelForcedUnduck();
	}
}

bool KZTriggerService::InAntiPauseArea()
{
	return this->modifiers.disablePausingCount > 0;
}

bool KZTriggerService::InBhopTriggers()
{
	FOR_EACH_VEC(this->triggerTrackers, i)
	{
		bool justTouched = g_pKZUtils->GetServerGlobals()->curtime - this->triggerTrackers[i].startTouchTime < 0.15f;
		if (justTouched && this->triggerTrackers[i].isPossibleLegacyBhopTrigger)
		{
			return true;
		}
	}
	return this->bhopTouchCount > 0;
}

bool KZTriggerService::InAntiCpArea()
{
	return this->modifiers.disableCheckpointsCount > 0;
}

bool KZTriggerService::CanTeleportToCheckpoints()
{
	return this->modifiers.disableTeleportsCount <= 0;
}

bool KZTriggerService::ShouldDisableJumpstats()
{
	return this->modifiers.disableJumpstatsCount > 0;
}

void KZTriggerService::TouchModifierTrigger(TriggerTouchTracker tracker)
{
	const KzTrigger *trigger = tracker.kzTrigger;

	if (trigger->modifier.gravity != 1)
	{
		// No gravity while paused.
		if (this->player->timerService->GetPaused())
		{
			this->player->GetPlayerPawn()->SetGravityScale(0);
			return;
		}
		this->player->GetPlayerPawn()->SetGravityScale(trigger->modifier.gravity);
	}
	this->modifiers.jumpFactor = trigger->modifier.jumpFactor;
}

void KZTriggerService::TouchAntibhopTrigger(TriggerTouchTracker tracker)
{
	f32 timeOnGround = g_pKZUtils->GetServerGlobals()->curtime - this->player->landingTimeServer;
	if (tracker.kzTrigger->antibhop.time == 0                            // No jump trigger
		|| timeOnGround <= tracker.kzTrigger->antibhop.time              // Haven't touched the trigger for long enough
		|| (this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND) == 0) // Not on the ground (for prediction)
	{
		this->antiBhopActive = true;
	}
}

bool KZTriggerService::TouchTeleportTrigger(TriggerTouchTracker tracker)
{
	bool shouldTeleport = false;

	bool isBhopTrigger = KZ::mapapi::IsBhopTrigger(tracker.kzTrigger->type);
	// Do not teleport the player if it's a bhop trigger and they are not on the ground.
	if (isBhopTrigger && (this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND) == 0)
	{
		return false;
	}

	CEntityHandle destinationHandle = GameEntitySystem()->FindFirstEntityHandleByName(tracker.kzTrigger->teleport.destination);
	CBaseEntity *destination = dynamic_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(destinationHandle));
	if (!destinationHandle.IsValid() || !destination)
	{
		META_CONPRINTF("Invalid teleport destination \"%s\" on trigger with hammerID %i.\n", tracker.kzTrigger->teleport.destination,
					   tracker.kzTrigger->hammerId);
		return false;
	}

	Vector destOrigin = destination->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	QAngle destAngles = destination->m_CBodyComponent()->m_pSceneNode()->m_angRotation();
	CBaseEntity *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(tracker.kzTrigger->entity));
	Vector triggerOrigin = Vector(0, 0, 0);
	if (trigger)
	{
		triggerOrigin = trigger->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	}

	// NOTE: We only use the trigger's origin if we're using a relative destination, so if
	// we're not using a relative destination and don't have it, then it's fine.
	// TODO: Can this actually happen? If the trigger is touched then the entity must be valid.
	if (!trigger && tracker.kzTrigger->teleport.relative)
	{
		return false;
	}

	if (isBhopTrigger && (this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND))
	{
		f32 effectiveStartTouchTime = MAX(this->player->landingTimeServer, tracker.startTouchTime);
		f32 touchingTime = g_pKZUtils->GetServerGlobals()->curtime - effectiveStartTouchTime;
		if (touchingTime > tracker.kzTrigger->teleport.delay)
		{
			shouldTeleport = true;
		}
		else if (tracker.kzTrigger->type == KZTRIGGER_SINGLE_BHOP)
		{
			shouldTeleport = this->lastTouchedSingleBhop == tracker.kzTrigger->entity;
		}
		else if (tracker.kzTrigger->type == KZTRIGGER_SEQUENTIAL_BHOP)
		{
			for (i32 i = 0; i < this->lastTouchedSequentialBhops.GetReadAvailable(); i++)
			{
				CEntityHandle handle = CEntityHandle();
				if (!this->lastTouchedSequentialBhops.Peek(&handle, i))
				{
					assert(0);
					break;
				}
				if (handle == tracker.kzTrigger->entity)
				{
					shouldTeleport = true;
					break;
				}
			}
		}
	}
	else if (tracker.kzTrigger->type == KZTRIGGER_TELEPORT)
	{
		f32 touchingTime = g_pKZUtils->GetServerGlobals()->curtime - tracker.startTouchTime;
		shouldTeleport = touchingTime > tracker.kzTrigger->teleport.delay || tracker.kzTrigger->teleport.delay <= 0;
	}

	if (!shouldTeleport)
	{
		return false;
	}

	bool shouldReorientPlayer = tracker.kzTrigger->teleport.reorientPlayer && destAngles[YAW] != 0;
	Vector up = Vector(0, 0, 1);
	Vector finalOrigin = destOrigin;

	if (tracker.kzTrigger->teleport.relative)
	{
		Vector playerOrigin;
		this->player->GetOrigin(&playerOrigin);
		Vector playerOffsetFromTrigger = playerOrigin - triggerOrigin;

		if (shouldReorientPlayer)
		{
			VectorRotate(playerOffsetFromTrigger, QAngle(0, destAngles[YAW], 0), playerOffsetFromTrigger);
		}

		finalOrigin = destOrigin + playerOffsetFromTrigger;
	}
	QAngle finalPlayerAngles;
	this->player->GetAngles(&finalPlayerAngles);
	Vector finalVelocity;
	this->player->GetVelocity(&finalVelocity);
	if (shouldReorientPlayer)
	{
		// TODO: BUG: sometimes when getting reoriented and holding a movement key
		//  the player's speed will get reduced, almost like velocity rotation
		//  and angle rotation is out of sync leading to counterstrafing.
		// Maybe we should check m_nHighestGeneratedServerViewAngleChangeIndex for angles overridding...
		VectorRotate(finalVelocity, QAngle(0, destAngles[YAW], 0), finalVelocity);
		finalPlayerAngles[YAW] -= destAngles[YAW];
		this->player->SetAngles(finalPlayerAngles);
	}
	else if (!tracker.kzTrigger->teleport.reorientPlayer && tracker.kzTrigger->teleport.useDestinationAngles)
	{
		this->player->SetAngles(destAngles);
	}

	if (tracker.kzTrigger->teleport.resetSpeed)
	{
		this->player->SetVelocity(vec3_origin);
	}
	else
	{
		this->player->SetVelocity(finalVelocity);
	}

	// We need to call teleport hook because we don't use teleport function directly.
	if (this->player->processingMovement && this->player->currentMoveData)
	{
		this->player->OnTeleport(&finalOrigin, nullptr, nullptr);
	}
	this->player->SetOrigin(finalOrigin);

	return true;
}

void KZTriggerService::TouchPushTrigger(TriggerTouchTracker tracker)
{
	u32 pushConditions = tracker.kzTrigger->push.pushConditions;
	// clang-format off
	if (pushConditions & KzMapPush::KZ_PUSH_TOUCH
		|| (this->player->IsButtonNewlyPressed(IN_ATTACK) && pushConditions & KzMapPush::KZ_PUSH_ATTACK)
		|| (this->player->IsButtonNewlyPressed(IN_ATTACK2) && pushConditions & KzMapPush::KZ_PUSH_ATTACK2)
		|| (this->player->IsButtonNewlyPressed(IN_JUMP) && pushConditions & KzMapPush::KZ_PUSH_JUMP_BUTTON)
		|| (this->player->IsButtonNewlyPressed(IN_USE) && pushConditions & KzMapPush::KZ_PUSH_USE))
	// clang-format on
	{
		this->AddPushEvent(tracker.kzTrigger);
	}
}

void KZTriggerService::ApplySlide(bool replicate)
{
	const CVValue_t *aaValue = player->GetCvarValueFromModeStyles("sv_airaccelerate");
	const CVValue_t newAA = aaValue->m_fl32Value * 4.0f;
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_standable_normal", "2", replicate);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_walkable_normal", "2", replicate);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_airaccelerate", &newAA, replicate);
}

void KZTriggerService::CancelSlide(bool replicate)
{
	const CVValue_t *standableValue = player->GetCvarValueFromModeStyles("sv_standable_normal");
	const CVValue_t *walkableValue = player->GetCvarValueFromModeStyles("sv_walkable_normal");
	const CVValue_t *aaValue = player->GetCvarValueFromModeStyles("sv_airaccelerate");
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_airaccelerate", aaValue, replicate);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_standable_normal", standableValue, replicate);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_walkable_normal", walkableValue, replicate);
}

void KZTriggerService::ApplyAntiBhop(bool replicate)
{
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_jump_spam_penalty_time", "999999.9", replicate);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_autobunnyhopping", "false", replicate);
	player->GetMoveServices()->m_bOldJumpPressed() = true;
}

void KZTriggerService::CancelAntiBhop(bool replicate)
{
	const CVValue_t *spamModeValue = player->GetCvarValueFromModeStyles("sv_jump_spam_penalty_time");
	const CVValue_t *autoBhopValue = player->GetCvarValueFromModeStyles("sv_autobunnyhopping");
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_jump_spam_penalty_time", spamModeValue, replicate);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_autobunnyhopping", autoBhopValue, replicate);
}

void KZTriggerService::ApplyForcedDuck()
{
	this->player->GetMoveServices()->m_bDuckOverride.Set(true);
}

void KZTriggerService::CancelForcedDuck()
{
	this->player->GetMoveServices()->m_bDuckOverride.Set(false);
}

void KZTriggerService::ApplyForcedUnduck()
{
	// Set crouch time to a very large value so that the player cannot reduck.
	this->player->GetMoveServices()->m_flLastDuckTime(100000.0f);
	// This needs to be done every tick just since the player can be in spots that are not unduckable (eg. crouch tunnels)
	this->player->GetPlayerPawn()->m_fFlags.Set(this->player->GetPlayerPawn()->m_fFlags() & ~FL_DUCKING);
}

void KZTriggerService::CancelForcedUnduck()
{
	this->player->GetMoveServices()->m_flLastDuckTime(0.0f);
}

void KZTriggerService::ApplyJumpFactor(bool replicate)
{
	const CVValue_t *impulseModeValue = player->GetCvarValueFromModeStyles("sv_jump_impulse");
	const CVValue_t newImpulseValue = (impulseModeValue->m_fl32Value * this->modifiers.jumpFactor);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_jump_impulse", &newImpulseValue, replicate);

	const CVValue_t *jumpCostValue = player->GetCvarValueFromModeStyles("sv_staminajumpcost");
	const CVValue_t newJumpCostValue = (jumpCostValue->m_fl32Value / this->modifiers.jumpFactor);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_staminajumpcost", &newJumpCostValue, replicate);
}
