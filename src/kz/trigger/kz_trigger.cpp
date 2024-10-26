#include "kz_trigger.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"

void KZTriggerService::Reset()
{
	triggerTrackers.RemoveAll();
}

void KZTriggerService::OnPhysicsSimulate()
{
	FOR_EACH_VEC(this->triggerTrackers, i)
	{
		CEntityHandle handle = this->triggerTrackers[i].triggerHandle;
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(handle));
		// The trigger mysteriously disappeared...
		if (!trigger)
		{
			const KzTrigger *kzTrigger = this->triggerTrackers[i].kzTrigger;
			if (kzTrigger)
			{
				this->OnMappingApiTriggerEndTouchPost(this->triggerTrackers[i]);
			}

			this->triggerTrackers.Remove(i);
			i--;
			continue;
		}
		this->triggerTrackers[i].touchedThisTick = false;
	}
}

void KZTriggerService::OnPhysicsSimulatePost()
{
	this->TouchAll();
	/*
		NOTE:
		1. To prevent multiplayer bugs, make sure that all of these cvars are part of the mode convars.
		2. The apply part is here mostly just to replicate the values to the client.
	*/

	if (this->modifiers.enableSlideCount > 0)
	{
		this->ApplySlide(!this->lastModifiers.enableSlideCount);
	}
	else if (this->lastModifiers.enableSlideCount > 0)
	{
		this->CancelSlide(true);
	}

	if (this->antiBhopActive)
	{
		this->ApplyAntiBhop(!this->lastAntiBhopActive);
	}
	else if (this->lastAntiBhopActive)
	{
		this->CancelAntiBhop(true);
	}

	this->lastModifiers = this->modifiers;
	this->lastAntiBhopActive = this->antiBhopActive;
}

void KZTriggerService::OnProcessMovement() {}

void KZTriggerService::OnProcessMovementPost()
{
	// if the player isn't touching any bhop triggers on ground/a ladder, then
	// reset the singlebhop and sequential bhop state.
	if ((this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND || this->player->GetMoveType() == MOVETYPE_LADDER) && this->bhopTouchCount == 0)
	{
		this->ResetBhopState();
	}

	this->antiBhopActive = false;
}

void KZTriggerService::OnStopTouchGround()
{
	FOR_EACH_VEC(this->triggerTrackers, i)
	{
		TriggerTouchTracker tracker = this->triggerTrackers[i];
		if (!tracker.kzTrigger)
		{
			continue;
		}
		if (KZ::mapapi::IsBhopTrigger(tracker.kzTrigger->type))
		{
			// set last touched triggers for single and sequential bhop.
			if (tracker.kzTrigger->type == KZTRIGGER_SEQUENTIAL_BHOP)
			{
				CEntityHandle handle = tracker.kzTrigger->entity;
				this->lastTouchedSequentialBhops.Write(handle);
			}

			// NOTE: For singlebhops, we don't care which type of bhop we last touched, because
			//  otherwise jumping back and forth between a multibhop and a singlebhop wouldn't work.
			// We only care about the most recently touched trigger!
			this->lastTouchedSingleBhop = tracker.kzTrigger->entity;
		}
	}
}

void KZTriggerService::TouchTriggersAlongPath(const Vector &start, const Vector &end, const bbox_t &bounds)
{
	if (!this->player->IsAlive() || this->player->GetCollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
	{
		return;
	}
	CTraceFilterHitAllTriggers filter;
	trace_t tr;
	g_pKZUtils->TracePlayerBBox(start, end, bounds, &filter, tr);
	FOR_EACH_VEC(filter.hitTriggerHandles, i)
	{
		CEntityHandle handle = filter.hitTriggerHandles[i];
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(handle));
		if (!trigger || !V_strstr(trigger->GetClassname(), "trigger_"))
		{
			continue;
		}
		if (!this->GetTriggerTracker(trigger))
		{
			this->StartTouch(trigger);
		}
	}
}

void KZTriggerService::UpdateTriggerTouchList()
{
	if (!this->player->IsAlive() || this->player->GetCollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
	{
		this->EndTouchAll();
		return;
	}
	Vector origin;
	this->player->GetOrigin(&origin);
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);
	CTraceFilterHitAllTriggers filter;
	trace_t tr;
	g_pKZUtils->TracePlayerBBox(origin, origin, bounds, &filter, tr);

	FOR_EACH_VEC_BACK(this->triggerTrackers, i)
	{
		CEntityHandle handle = this->triggerTrackers[i].triggerHandle;
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(handle));
		// The trigger mysteriously disappeared...
		if (!trigger)
		{
			const KzTrigger *kzTrigger = this->triggerTrackers[i].kzTrigger;
			if (kzTrigger)
			{
				this->OnMappingApiTriggerEndTouchPost(this->triggerTrackers[i]);
			}

			this->triggerTrackers.Remove(i);
			continue;
		}
		if (!filter.hitTriggerHandles.HasElement(handle))
		{
			this->EndTouch(trigger);
		}
	}

	// Reset antibhop state, we will re-evaluate it again inside touch events.
	this->antiBhopActive = false;

	FOR_EACH_VEC(filter.hitTriggerHandles, i)
	{
		CEntityHandle handle = filter.hitTriggerHandles[i];
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(handle));
		if (!trigger || !V_strstr(trigger->GetClassname(), "trigger_"))
		{
			continue;
		}
		auto tracker = this->GetTriggerTracker(trigger);
		if (!tracker)
		{
			this->StartTouch(trigger);
		}
		else if (KZTriggerService::HighFrequencyTouchAllowed(*tracker))
		{
			this->Touch(trigger);
		}
	}

	UpdateModifiersInternal();
}

void KZTriggerService::EndTouchAll()
{
	FOR_EACH_VEC(this->triggerTrackers, i)
	{
		CEntityHandle handle = this->triggerTrackers[i].triggerHandle;
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(handle));
		// The trigger mysteriously disappeared...
		if (!trigger)
		{
			const KzTrigger *kzTrigger = this->triggerTrackers[i].kzTrigger;
			if (kzTrigger)
			{
				this->OnMappingApiTriggerEndTouchPost(this->triggerTrackers[i]);
			}

			this->triggerTrackers.Remove(i);
			i--;
			continue;
		}
		this->EndTouch(trigger);
	}
}

void KZTriggerService::TouchAll()
{
	FOR_EACH_VEC(this->triggerTrackers, i)
	{
		CEntityHandle handle = this->triggerTrackers[i].triggerHandle;
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(handle));
		// The trigger mysteriously disappeared...
		if (!trigger)
		{
			const KzTrigger *kzTrigger = this->triggerTrackers[i].kzTrigger;
			if (kzTrigger)
			{
				this->OnMappingApiTriggerEndTouchPost(this->triggerTrackers[i]);
			}

			this->triggerTrackers.Remove(i);
			i--;
			continue;
		}
		this->Touch(trigger);
	}
}

bool KZTriggerService::IsManagedByTriggerService(CBaseEntity *toucher, CBaseEntity *touched)
{
	KZPlayer *player = NULL;
	CBaseTrigger *trigger = NULL;
	if (!toucher || !touched)
	{
		return false;
	}
	if (V_stricmp(toucher->GetClassname(), "player") == 0 && V_strstr(touched->GetClassname(), "trigger_"))
	{
		player = g_pKZPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(toucher));
		trigger = static_cast<CBaseTrigger *>(touched);
	}
	if (V_stricmp(touched->GetClassname(), "player") == 0 && V_strstr(toucher->GetClassname(), "trigger_"))
	{
		player = g_pKZPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(touched));
		trigger = static_cast<CBaseTrigger *>(toucher);
	}
	if (player && player->IsAlive() && player->GetMoveType() != MOVETYPE_NOCLIP)
	{
		return true;
	}
	return false;
}

bool KZTriggerService::HighFrequencyTouchAllowed(TriggerTouchTracker tracker)
{
	if (!tracker.kzTrigger)
	{
		return false;
	}
	switch (tracker.kzTrigger->type)
	{
		case KZTRIGGER_ANTI_BHOP:
		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
			return true;
	}
	return false;
}

KZTriggerService::TriggerTouchTracker *KZTriggerService::GetTriggerTracker(CBaseTrigger *trigger)
{
	if (!trigger)
	{
		return nullptr;
	}
	CEntityHandle handle = trigger->GetRefEHandle();
	FOR_EACH_VEC(triggerTrackers, i)
	{
		TriggerTouchTracker &tracker = triggerTrackers[i];
		if (tracker.triggerHandle == handle)
		{
			return &tracker;
		}
	}
	return nullptr;
}

void KZTriggerService::StartTouch(CBaseTrigger *trigger)
{
	// Can't touch nothing.
	if (!trigger)
	{
		return;
	}
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	// Can't touch anything if there is no pawn.
	if (!pawn)
	{
		return;
	}

	TriggerTouchTracker *tracker = this->GetTriggerTracker(trigger);
	bool shouldStartTouch = (!tracker || tracker->CanStartTouch()) && this->OnTriggerStartTouchPre(trigger);

	if (!shouldStartTouch)
	{
		return;
	}

	// New interaction!
	if (!tracker)
	{
		tracker = triggerTrackers.AddToTailGetPtr();
		tracker->triggerHandle = trigger->GetRefEHandle();
		tracker->startTouchTime = g_pKZUtils->GetServerGlobals()->curtime;
		tracker->kzTrigger = KZ::mapapi::GetKzTrigger(trigger);
	}

	// Handle changes in origin and velocity due to this event.
	this->UpdatePreTouchData();
	trigger->StartTouch(pawn);
	pawn->StartTouch(pawn);
	tracker->startedTouch = true;
	this->OnTriggerStartTouchPost(trigger, *tracker);
	// Call UpdatePlayerPostTouch here because UpdatePlayerStartTouch will be run inside Touch later anyway.
	this->UpdatePlayerPostTouch();

	if (KZTriggerService::ShouldTouchOnStartTouch(*tracker))
	{
		this->Touch(trigger, true);
	}
}

void KZTriggerService::Touch(CBaseTrigger *trigger, bool silent)
{
	// Can't touch nothing.
	if (!trigger)
	{
		return;
	}
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	// Can't touch anything if there is no pawn.
	if (!pawn)
	{
		return;
	}

	TriggerTouchTracker *tracker = this->GetTriggerTracker(trigger);
	if (!tracker)
	{
		return;
	}
	bool shouldTouch = tracker->CanTouch() && this->OnTriggerTouchPre(trigger, *tracker);

	if (shouldTouch)
	{
		this->UpdatePreTouchData();
		trigger->Touch(pawn);
		pawn->Touch(trigger);
		if (!silent)
		{
			tracker->touchedThisTick = true;
		}

		this->OnTriggerTouchPost(trigger, *tracker);
		this->UpdatePlayerPostTouch();
	}
}

void KZTriggerService::EndTouch(CBaseTrigger *trigger)
{
	// Can't touch nothing.
	if (!trigger)
	{
		return;
	}
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	// Can't touch anything if there is no pawn.
	if (!pawn)
	{
		return;
	}

	TriggerTouchTracker *tracker = this->GetTriggerTracker(trigger);
	if (!tracker)
	{
		return;
	}

	bool shouldEndTouch = tracker->CanEndTouch() && this->OnTriggerEndTouchPre(trigger, *tracker);
	if (shouldEndTouch)
	{
		if (KZTriggerService::ShouldTouchBeforeEndTouch(*tracker))
		{
			this->Touch(trigger);
		}
		this->UpdatePreTouchData();
		trigger->EndTouch(pawn);
		pawn->EndTouch(trigger);
		this->UpdatePlayerPostTouch();
		this->OnTriggerEndTouchPost(trigger, *tracker);
		this->triggerTrackers.FindAndRemove(*tracker);
	}
}

void KZTriggerService::UpdatePreTouchData()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	this->preTouchVelocity = pawn->m_vecAbsVelocity();
	this->preTouchOrigin = pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
}

void KZTriggerService::UpdatePlayerPostTouch()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	// Player has a modified velocity through trigger touching, take this into account.
	bool modifiedVelocity = this->preTouchVelocity != pawn->m_vecAbsVelocity();
	if (player->processingMovement && modifiedVelocity)
	{
		player->SetVelocity(player->currentMoveData->m_vecVelocity - player->moveDataPre.m_vecVelocity + pawn->m_vecAbsVelocity());
		this->player->jumpstatsService->InvalidateJumpstats("Externally modified");
	}

	bool modifiedOrigin = this->preTouchOrigin != pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	if (player->processingMovement && modifiedOrigin)
	{
		player->SetOrigin(pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin());
		this->player->jumpstatsService->InvalidateJumpstats("Externally modified");
	}
}
