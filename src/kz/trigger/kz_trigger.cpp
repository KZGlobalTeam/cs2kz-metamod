#include "kz_trigger.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"
#include "kz/noclip/kz_noclip.h"

void KZTriggerService::Reset()
{
	this->triggerTrackers.RemoveAll();
	this->modifiers = {};
	this->lastModifiers = {};
	this->antiBhopActive = {};
	this->lastAntiBhopActive = {};
	this->lastTouchedSingleBhop = {};
	this->bhopTouchCount = {};
	this->lastTouchedSequentialBhops = {};
	this->pushEvents.RemoveAll();
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
	this->player->UpdateTriggerTouchList();
	this->TouchAll();
	/*
		NOTE:
		1. To prevent multiplayer bugs, make sure that all of these cvars are part of the mode convars.
		2. The apply part is here mostly just to replicate the values to the client, with the exception of push triggers.
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
		this->modifiers.jumpFactor = 0.0f;
		this->ApplyAntiBhop(!this->lastAntiBhopActive);
	}
	else if (this->lastAntiBhopActive)
	{
		this->CancelAntiBhop(true);
	}

	this->ApplyJumpFactor(this->modifiers.jumpFactor != this->lastModifiers.jumpFactor);
	// Try to apply pushes one last time on this tick, to catch all the buttons that were not set during movement processing (attack+attack2).
	this->ApplyPushes();
	this->CleanupPushEvents();

	this->lastModifiers = this->modifiers;
	this->lastAntiBhopActive = this->antiBhopActive;
}

void KZTriggerService::OnCheckJumpButton()
{
	this->ApplyJumpFactor(false);
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
	this->modifiers.jumpFactor = 1.0f;
	this->ApplyPushes();
	this->CleanupPushEvents();
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
		if (this->player->jumped && KZ::mapapi::IsPushTrigger(tracker.kzTrigger->type)
			&& tracker.kzTrigger->push.pushConditions & KzMapPush::KZ_PUSH_JUMP_EVENT)
		{
			this->AddPushEvent(tracker.kzTrigger);
		}
	}
}

void KZTriggerService::OnTeleport()
{
	FOR_EACH_VEC_BACK(this->pushEvents, i)
	{
		if (this->pushEvents[i].source->push.cancelOnTeleport)
		{
			this->pushEvents.Remove(i);
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
		if (!trigger || !KZTriggerService::IsValidTrigger(trigger))
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
	// reset gravity before all the Touch() calls
	if (this->player->timerService->GetPaused())
	{
		// No gravity while paused.
		this->player->GetPlayerPawn()->SetGravityScale(0);
	}
	else
	{
		this->player->GetPlayerPawn()->SetGravityScale(1);
	}

	if (!this->player->IsAlive() || this->player->noclipService->IsNoclipping())
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
		if (!trigger || !KZTriggerService::IsValidTrigger(trigger))
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

	this->UpdateModifiersInternal();
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

bool KZTriggerService::IsValidTrigger(CBaseEntity *entity)
{
	// All trigger_ entities are valid except trigger_push.
	if (entity && V_strstr(entity->GetClassname(), "trigger_") && !KZ_STREQI(entity->GetClassname(), "trigger_push"))
	{
		return true;
	}
	return false;
}

bool KZTriggerService::IsPossibleLegacyBhopTrigger(CTriggerMultiple *trigger)
{
	if (!trigger)
	{
		return false;
	}
	if (trigger->m_iFilterName().String()[0] == 0 && !trigger->m_hFilter().IsValid())
	{
		return false;
	}
	EntityIOConnection_t *connection = trigger->m_OnTrigger().m_pConnections;
	while (connection)
	{
		if (!V_stricmp(connection->m_targetInput.ToCStr(), "TeleportEntity"))
		{
			return true;
		}
		connection = connection->m_pNext;
	}
	return false;
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
	if (V_stricmp(toucher->GetClassname(), "player") == 0 && KZTriggerService::IsValidTrigger(touched))
	{
		player = g_pKZPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(toucher));
		trigger = static_cast<CBaseTrigger *>(touched);
	}
	if (V_stricmp(touched->GetClassname(), "player") == 0 && KZTriggerService::IsValidTrigger(toucher))
	{
		player = g_pKZPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(touched));
		trigger = static_cast<CBaseTrigger *>(toucher);
	}
	if (player && player->IsAlive())
	{
		return true;
	}
	return false;
}

bool KZTriggerService::HighFrequencyTouchAllowed(TriggerTouchTracker tracker)
{
	return tracker.kzTrigger;
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
		tracker->isPossibleLegacyBhopTrigger = V_stricmp(trigger->GetClassname(), "trigger_multiple") == 0
												   ? KZTriggerService::IsPossibleLegacyBhopTrigger((CTriggerMultiple *)trigger)
												   : false;
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
	Vector pawnAbsVel = pawn->m_vecAbsVelocity();
	bool modifiedVelocity = this->preTouchVelocity != pawnAbsVel;
	if (player->processingMovement && modifiedVelocity)
	{
		this->player->jumpstatsService->InvalidateJumpstats("Externally modified");
	}

	bool modifiedOrigin = this->preTouchOrigin != pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	if (player->processingMovement && modifiedOrigin)
	{
		player->SetOrigin(pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin());
		this->player->jumpstatsService->InvalidateJumpstats("Externally modified");
	}
}
