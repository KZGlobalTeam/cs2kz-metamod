#include "kz_trigger.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/noclip/kz_noclip.h"
#include "kz/timer/kz_timer.h"
#include "kz/language/kz_language.h"

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
	return retValue;
}

bool KZTriggerService::OnTriggerTouchPre(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	bool retValue = this->player->modeService->OnTriggerTouch(trigger);
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		retValue &= this->player->styleServices[i]->OnTriggerTouch(trigger);
	}
	return retValue;
}

bool KZTriggerService::OnTriggerEndTouchPre(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	bool retValue = this->player->modeService->OnTriggerEndTouch(trigger);
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		retValue &= this->player->styleServices[i]->OnTriggerEndTouch(trigger);
	}
	return retValue;
}

// Mapping API stuff.
void KZTriggerService::OnTriggerStartTouchPost(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	if (!tracker.kzTrigger)
	{
		return;
	}
	this->OnMappingApiTriggerStartTouchPost(tracker);
}

void KZTriggerService::OnTriggerTouchPost(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	if (!tracker.kzTrigger)
	{
		return;
	}
	this->OnMappingApiTriggerTouchPost(tracker);
}

void KZTriggerService::OnTriggerEndTouchPost(CBaseTrigger *trigger, TriggerTouchTracker tracker)
{
	if (!tracker.kzTrigger)
	{
		return;
	}
	this->OnMappingApiTriggerEndTouchPost(tracker);
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

		case KZTRIGGER_ANTI_BHOP:
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

		default:
			break;
	}
}

void KZTriggerService::OnMappingApiTriggerTouchPost(TriggerTouchTracker tracker)
{
	bool shouldRecheckTriggers = false;
	switch (tracker.kzTrigger->type)
	{
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
			assert(this->modifiers.disablePausingCount >= 0);
			assert(this->modifiers.disableCheckpointsCount >= 0);
			assert(this->modifiers.disableTeleportsCount >= 0);
			assert(this->modifiers.disableJumpstatsCount >= 0);
			assert(this->modifiers.enableSlideCount >= 0);
		}
		break;

		case KZTRIGGER_ZONE_START:
		{
			this->player->checkpointService->ResetCheckpoints();
			this->player->timerService->StartZoneEndTouch(course);
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_ANTI_BHOP:
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

		default:
			break;
	}
}
