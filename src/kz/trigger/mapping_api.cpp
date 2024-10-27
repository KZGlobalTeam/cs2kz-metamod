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

	if (tracker.kzTrigger->teleport.resetSpeed)
	{
		this->player->SetVelocity(vec3_origin);
	}
	else
	{
		this->player->SetVelocity(finalVelocity);
	}

	this->player->SetOrigin(finalOrigin);

	return shouldTeleport;
}

void KZTriggerService::ApplySlide(bool replicate)
{
	CUtlString aaValue = player->ComputeCvarValueFromModeStyles("sv_airaccelerate");
	aaValue.Format("%f", atof(aaValue.Get()) * 4.0);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_standable_normal", "2", replicate);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_walkable_normal", "2", replicate);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_airaccelerate", aaValue.Get(), replicate);
}

void KZTriggerService::CancelSlide(bool replicate)
{
	CUtlString standableValue = player->ComputeCvarValueFromModeStyles("sv_standable_normal");
	CUtlString walkableValue = player->ComputeCvarValueFromModeStyles("sv_walkable_normal");
	CUtlString aaValue = player->ComputeCvarValueFromModeStyles("sv_airaccelerate");
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_airaccelerate", aaValue.Get(), replicate);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_standable_normal", standableValue.Get(), replicate);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_walkable_normal", walkableValue.Get(), replicate);
}

void KZTriggerService::ApplyAntiBhop(bool replicate)
{
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_jump_impulse", "0.0", replicate);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_jump_spam_penalty_time", "999999.9", replicate);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_autobunnyhopping", "false", replicate);
	player->GetMoveServices()->m_bOldJumpPressed() = true;
}

void KZTriggerService::CancelAntiBhop(bool replicate)
{
	CUtlString impulseModeValue = player->ComputeCvarValueFromModeStyles("sv_jump_impulse");
	CUtlString spamModeValue = player->ComputeCvarValueFromModeStyles("sv_jump_spam_penalty_time");
	CUtlString autoBhopValue = player->ComputeCvarValueFromModeStyles("sv_autobunnyhopping");
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_jump_impulse", impulseModeValue.Get(), replicate);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_jump_spam_penalty_time", spamModeValue.Get(), replicate);
	utils::SetConvarValue(player->GetPlayerSlot(), "sv_autobunnyhopping", autoBhopValue.Get(), replicate);
}
