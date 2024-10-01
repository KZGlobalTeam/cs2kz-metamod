#include "kz_trigger.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/noclip/kz_noclip.h"
#include "kz/timer/kz_timer.h"

bool KZTriggerService::OnTriggerStartTouchPre(CBaseTrigger *trigger)
{
	bool retValue = this->player->modeService->OnTriggerStartTouch(trigger);
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		retValue &= this->player->styleServices[i]->OnTriggerStartTouch(trigger);
	}
	return retValue;
}

void KZTriggerService::OnTriggerStartTouchPost(CBaseTrigger *trigger)
{
	if (!V_stricmp(trigger->GetClassname(), "trigger_multiple"))
	{
		if (trigger->IsEndZone())
		{
			this->player->timerService->TimerEnd("Main");
		}
		else if (trigger->IsStartZone())
		{
			this->player->checkpointService->ResetCheckpoints();
			this->player->timerService->StartZoneStartTouch();
		}
	}
}

bool KZTriggerService::OnTriggerTouchPre(CBaseTrigger *trigger)
{
	bool retValue = this->player->modeService->OnTriggerTouch(trigger);
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		retValue &= this->player->styleServices[i]->OnTriggerTouch(trigger);
	}
	return retValue;
}

void KZTriggerService::OnTriggerTouchPost(CBaseTrigger *trigger) {}

bool KZTriggerService::OnTriggerEndTouchPre(CBaseTrigger *trigger)
{
	bool retValue = this->player->modeService->OnTriggerEndTouch(trigger);
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		retValue &= this->player->styleServices[i]->OnTriggerEndTouch(trigger);
	}
	return retValue;
}

void KZTriggerService::OnTriggerEndTouchPost(CBaseTrigger *trigger)
{
	if (!V_stricmp(trigger->GetClassname(), "trigger_multiple") && trigger->IsStartZone())
	{
		if (!this->player->noclipService->IsNoclipping())
		{
			this->player->checkpointService->ResetCheckpoints();
			this->player->timerService->StartZoneEndTouch();
		}
	}
}
