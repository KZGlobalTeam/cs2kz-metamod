#include "kz_trigger.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/noclip/kz_noclip.h"
#include "kz/timer/kz_timer.h"
#include "kz/language/kz_language.h"
#include "kz/replays/kz_replaysystem.h"

/*
	Note: Whether touching is allowed is set determined by the mode, while Mapping API effects will be applied after touching events.
*/

// Whether we allow interaction from happening.
bool KZTriggerService::OnTriggerStartTouchPre(CBaseTrigger *trigger)
{
	bool retValue = this->player->modeService->OnTriggerStartTouch(trigger);
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		retValue &= this->player->styleServices[i]->OnTriggerStartTouch(trigger);
	}
	retValue &= KZ::replaysystem::CanTouchTrigger(this->player, trigger);
	return retValue;
}

bool KZTriggerService::OnTriggerTouchPre(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	bool retValue = this->player->modeService->OnTriggerTouch(trigger);
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		retValue &= this->player->styleServices[i]->OnTriggerTouch(trigger);
	}
	retValue &= KZ::replaysystem::CanTouchTrigger(this->player, trigger);
	return retValue;
}

bool KZTriggerService::OnTriggerEndTouchPre(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	bool retValue = this->player->modeService->OnTriggerEndTouch(trigger);
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		retValue &= this->player->styleServices[i]->OnTriggerEndTouch(trigger);
	}
	retValue &= KZ::replaysystem::CanTouchTrigger(this->player, trigger);
	return retValue;
}

// Mapping API stuff.
void KZTriggerService::OnTriggerStartTouchPost(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	if (!tracker.kzTrigger || !trigger->PassesTriggerFilters(this->player->GetPlayerPawn()))
	{
		return;
	}
	this->OnMappingApiTriggerStartTouchPost(tracker);
}

void KZTriggerService::OnTriggerTouchPost(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	if (!tracker.kzTrigger || !trigger->PassesTriggerFilters(this->player->GetPlayerPawn()))
	{
		return;
	}
	this->OnMappingApiTriggerTouchPost(tracker);
}

void KZTriggerService::OnTriggerEndTouchPost(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	if (!tracker.kzTrigger || !trigger->PassesTriggerFilters(this->player->GetPlayerPawn()))
	{
		return;
	}
	this->OnMappingApiTriggerEndTouchPost(tracker);
}

void KZTriggerService::AddPushEvent(const KzTrigger *trigger)
{
	f32 curtime = g_pKZUtils->GetGlobals()->curtime;
	PushEvent event {trigger, curtime + trigger->push.delay};
	if (this->pushEvents.Find(event) == -1)
	{
		this->pushEvents.AddToTail(event);
	}
}

void KZTriggerService::CleanupPushEvents()
{
	f32 frametime = g_pKZUtils->GetGlobals()->frametime;
	// Don't remove push events since these push events are not fired yet.
	if (frametime == 0.0f)
	{
		return;
	}
	f32 curtime = g_pKZUtils->GetGlobals()->curtime;
	FOR_EACH_VEC_BACK(this->pushEvents, i)
	{
		if (!this->pushEvents[i].applied)
		{
			continue;
		}
		if (curtime - frametime >= this->pushEvents[i].pushTime + this->pushEvents[i].source->push.cooldown
			|| curtime < this->pushEvents[i].pushTime + this->pushEvents[i].source->push.cooldown)
		{
			this->pushEvents.Remove(i);
		}
	}
}

void KZTriggerService::ApplyPushes()
{
	f32 frametime = g_pKZUtils->GetGlobals()->frametime;
	// There's no point applying any push if player isn't going to move anyway.
	if (frametime == 0.0f)
	{
		return;
	}
	f32 curtime = g_pKZUtils->GetGlobals()->curtime;
	bool setSpeed[3] {};

	if (this->pushEvents.Count() == 0)
	{
		return;
	}
	bool useBaseVelocity = this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND && this->player->processingMovement;
	FOR_EACH_VEC(this->pushEvents, i)
	{
		if (curtime - frametime >= this->pushEvents[i].pushTime || curtime < this->pushEvents[i].pushTime || this->pushEvents[i].applied)
		{
			continue;
		}
		this->pushEvents[i].applied = true;
		auto &push = this->pushEvents[i].source->push;
		for (u32 i = 0; i < 3; i++)
		{
			Vector vel;
			if (useBaseVelocity && i != 2)
			{
				this->player->GetBaseVelocity(&vel);
			}
			else
			{
				this->player->GetVelocity(&vel);
			}
			// Set speed overrides add speed.
			if (push.setSpeed[i])
			{
				vel[i] = push.impulse[i];
				setSpeed[i] = true;
			}
			else if (!setSpeed[i])
			{
				vel[i] += push.impulse[i];
			}
			// If we are pushing the player up, make sure they cannot re-ground themselves.
			if (i == 2 && vel[i] > 0 && useBaseVelocity)
			{
				this->player->GetPlayerPawn()->m_hGroundEntity().FromIndex(INVALID_EHANDLE_INDEX);
				this->player->GetPlayerPawn()->m_fFlags() &= ~FL_ONGROUND;
				this->player->currentMoveData->m_groundNormal = vec3_origin;
			}
			if (useBaseVelocity && i != 2)
			{
				this->player->SetBaseVelocity(vel);
				this->player->GetPlayerPawn()->m_fFlags() |= FL_BASEVELOCITY;
			}
			else
			{
				this->player->SetVelocity(vel);
			}
			this->player->jumpstatsService->InvalidateJumpstats("Disabled By Map");
		}
	}
	// Try to nullify velocity if needed.
	if (useBaseVelocity)
	{
		Vector velocity, newVelocity;
		this->player->GetVelocity(&velocity);
		newVelocity = velocity;
		for (u32 i = 0; i < 2; i++)
		{
			if (setSpeed[i])
			{
				newVelocity[i] = 0;
			}
		}
		if (velocity != newVelocity)
		{
			this->player->SetVelocity(newVelocity);
		}
	}
}

void KZTriggerService::OnMappingApiTriggerStartTouchPost(TriggerTouchTracker tracker)
{
	const KzTrigger *trigger = tracker.kzTrigger;
	const KZCourseDescriptor *course = KZ::mapapi::GetCourseDescriptorFromTrigger(trigger);
	if (KZ::mapapi::IsTimerTrigger(trigger->type) && !course)
	{
		return;
	}

	switch (trigger->type)
	{
		case KZTRIGGER_MODIFIER:
		{
			KzMapModifier modifier = trigger->modifier;
			this->modifiers.disablePausingCount += modifier.disablePausing ? 1 : 0;
			this->modifiers.disableCheckpointsCount += modifier.disableCheckpoints ? 1 : 0;
			this->modifiers.disableTeleportsCount += modifier.disableTeleports ? 1 : 0;
			this->modifiers.disableJumpstatsCount += modifier.disableJumpstats ? 1 : 0;
			// Enabling slide will also disable jumpstats.
			this->modifiers.disableJumpstatsCount += modifier.enableSlide ? 1 : 0;
			this->modifiers.enableSlideCount += modifier.enableSlide ? 1 : 0;
			// Modifying jump velocity will also disable jumpstats.
			this->modifiers.disableJumpstatsCount += modifier.jumpFactor != 1.0f ? 1 : 0;
			this->modifiers.jumpFactor = modifier.jumpFactor;
			// Modifying force (un)duck will also disable jumpstats.
			this->modifiers.disableJumpstatsCount += modifier.forceDuck || modifier.forceUnduck ? 1 : 0;
			this->modifiers.forcedDuckCount += modifier.forceDuck ? 1 : 0;
			this->modifiers.forcedUnduckCount += modifier.forceUnduck ? 1 : 0;
		}
		break;

		case KZTRIGGER_RESET_CHECKPOINTS:
		{
			if (this->player->timerService->GetTimerRunning())
			{
				if (this->player->checkpointService->GetCheckpointCount())
				{
					this->player->languageService->PrintChat(true, false, "Checkpoints Cleared By Map");
				}
				this->player->checkpointService->ResetCheckpoints(true, false);
			}
		};
		break;

		case KZTRIGGER_SINGLE_BHOP_RESET:
		{
			this->ResetBhopState();
		}
		break;

		case KZTRIGGER_ZONE_START:
		{
			this->player->checkpointService->ResetCheckpoints();
			this->player->timerService->StartZoneStartTouch(course);
		}
		break;

		case KZTRIGGER_ZONE_END:
		{
			this->player->timerService->TimerEnd(course);
		}
		break;

		case KZTRIGGER_ZONE_SPLIT:
		{
			this->player->timerService->SplitZoneStartTouch(course, trigger->zone.number);
		}
		break;

		case KZTRIGGER_ZONE_CHECKPOINT:
		{
			this->player->timerService->CheckpointZoneStartTouch(course, trigger->zone.number);
		}
		break;

		case KZTRIGGER_ZONE_STAGE:
		{
			this->player->timerService->StageZoneStartTouch(course, trigger->zone.number);
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
		{
			if (KZ::mapapi::IsBhopTrigger(trigger->type))
			{
				this->bhopTouchCount++;
			}
		}
		break;
		case KZTRIGGER_PUSH:
		{
			if (tracker.kzTrigger->push.pushConditions & KzMapPush::KZ_PUSH_START_TOUCH)
			{
				this->AddPushEvent(trigger);
			}
		}
		break;
		default:
			break;
	}
}

void KZTriggerService::OnMappingApiTriggerTouchPost(TriggerTouchTracker tracker)
{
	bool shouldRecheckTriggers = false;
	switch (tracker.kzTrigger->type)
	{
		case KZTRIGGER_MODIFIER:
		{
			this->TouchModifierTrigger(tracker);
		}
		break;

		case KZTRIGGER_ANTI_BHOP:
		{
			this->TouchAntibhopTrigger(tracker);
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
		{
			this->TouchTeleportTrigger(tracker);
		}
		break;
		case KZTRIGGER_PUSH:
		{
			this->TouchPushTrigger(tracker);
		}
		break;
	}
}

void KZTriggerService::OnMappingApiTriggerEndTouchPost(TriggerTouchTracker tracker)
{
	const KZCourseDescriptor *course = KZ::mapapi::GetCourseDescriptorFromTrigger(tracker.kzTrigger);
	if (KZ::mapapi::IsTimerTrigger(tracker.kzTrigger->type) && !course)
	{
		return;
	}

	switch (tracker.kzTrigger->type)
	{
		case KZTRIGGER_MODIFIER:
		{
			KzMapModifier modifier = tracker.kzTrigger->modifier;
			this->modifiers.disablePausingCount -= modifier.disablePausing ? 1 : 0;
			this->modifiers.disableCheckpointsCount -= modifier.disableCheckpoints ? 1 : 0;
			this->modifiers.disableTeleportsCount -= modifier.disableTeleports ? 1 : 0;
			this->modifiers.disableJumpstatsCount -= modifier.disableJumpstats ? 1 : 0;
			// Enabling slide will also disable jumpstats.
			this->modifiers.disableJumpstatsCount -= modifier.enableSlide ? 1 : 0;
			this->modifiers.enableSlideCount -= modifier.enableSlide ? 1 : 0;
			// Modifying jump factor will also disable jumpstats.
			this->modifiers.disableJumpstatsCount -= modifier.jumpFactor != 1.0f ? 1 : 0;

			// Modifying force (un)duck will also disable jumpstats.
			this->modifiers.disableJumpstatsCount -= modifier.forceDuck || modifier.forceUnduck ? 1 : 0;
			this->modifiers.forcedDuckCount -= modifier.forceDuck ? 1 : 0;
			this->modifiers.forcedUnduckCount -= modifier.forceUnduck ? 1 : 0;
			assert(this->modifiers.disablePausingCount >= 0);
			assert(this->modifiers.disableCheckpointsCount >= 0);
			assert(this->modifiers.disableTeleportsCount >= 0);
			assert(this->modifiers.disableJumpstatsCount >= 0);
			assert(this->modifiers.enableSlideCount >= 0);
			assert(this->modifiers.forcedDuckCount >= 0);
			assert(this->modifiers.forcedUnduckCount >= 0);
		}
		break;

		case KZTRIGGER_ZONE_START:
		{
			this->player->checkpointService->ResetCheckpoints();
			this->player->timerService->StartZoneEndTouch(course);
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
		{
			if (KZ::mapapi::IsBhopTrigger(tracker.kzTrigger->type))
			{
				this->bhopTouchCount--;
			}
		}
		break;
		case KZTRIGGER_PUSH:
		{
			if (tracker.kzTrigger->push.pushConditions & KzMapPush::KZ_PUSH_END_TOUCH)
			{
				this->AddPushEvent(tracker.kzTrigger);
			}
		}
		break;
		default:
			break;
	}
}
