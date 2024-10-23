#include "kz.h"
#include "utils/utils.h"
#include "utils/ctimer.h"
#include "checkpoint/kz_checkpoint.h"
#include "db/kz_db.h"
#include "hud/kz_hud.h"
#include "jumpstats/kz_jumpstats.h"
#include "language/kz_language.h"
#include "mode/kz_mode.h"
#include "noclip/kz_noclip.h"
#include "option/kz_option.h"
#include "quiet/kz_quiet.h"
#include "spec/kz_spec.h"
#include "goto/kz_goto.h"
#include "style/kz_style.h"
#include "telemetry/kz_telemetry.h"
#include "timer/kz_timer.h"
#include "tip/kz_tip.h"

#include "sdk/entity/cbasetrigger.h"

#include "steam/isteamgameserver.h"
#include "tier0/memdbgon.h"

extern CSteamGameServerAPIContext g_steamAPI;
static_global ConVar *sv_standable_normal;
static_global ConVar *sv_walkable_normal;

void KZPlayer::Init()
{
	MovementPlayer::Init();
	this->hideLegs = false;

	// TODO: initialize every service.
	delete this->checkpointService;
	delete this->jumpstatsService;
	delete this->languageService;
	delete this->databaseService;
	delete this->quietService;
	delete this->hudService;
	delete this->specService;
	delete this->timerService;
	delete this->optionService;
	delete this->noclipService;
	delete this->tipService;
	delete this->telemetryService;

	this->checkpointService = new KZCheckpointService(this);
	this->jumpstatsService = new KZJumpstatsService(this);
	this->databaseService = new KZDatabaseService(this);
	this->languageService = new KZLanguageService(this);
	this->noclipService = new KZNoclipService(this);
	this->quietService = new KZQuietService(this);
	this->hudService = new KZHUDService(this);
	this->specService = new KZSpecService(this);
	this->gotoService = new KZGotoService(this);
	this->timerService = new KZTimerService(this);
	this->optionService = new KZOptionService(this);
	this->tipService = new KZTipService(this);
	this->telemetryService = new KZTelemetryService(this);

	KZ::mode::InitModeService(this);
}

void KZPlayer::Reset()
{
	MovementPlayer::Reset();

	// Reset services that should not persist across player sessions.
	this->languageService->Reset();
	this->tipService->Reset();
	this->modeService->Reset();
	this->optionService->Reset();
	this->checkpointService->Reset();
	this->noclipService->Reset();
	this->quietService->Reset();
	this->jumpstatsService->Reset();
	this->hudService->Reset();
	this->timerService->Reset();
	this->specService->Reset();

	g_pKZModeManager->SwitchToMode(this, KZOptionService::GetOptionStr("defaultMode", KZ_DEFAULT_MODE), true, true);
	g_pKZStyleManager->ClearStyles(this, true);
	CSplitString styles(KZOptionService::GetOptionStr("defaultStyles"), ",");
	FOR_EACH_VEC(styles, i)
	{
		g_pKZStyleManager->AddStyle(this, styles[i]);
	}
}

void KZPlayer::OnPlayerActive()
{
	// Mode/Styles stuff must be here for convars to be properly replicated.
	g_pKZModeManager->SwitchToMode(this, this->modeService->GetModeName(), true, true);
	g_pKZStyleManager->RefreshStyles(this);

	this->optionService->OnPlayerActive();
}

void KZPlayer::OnAuthorized()
{
	MovementPlayer::OnAuthorized();
	this->databaseService->SetupClient();
}

META_RES KZPlayer::GetPlayerMaxSpeed(f32 &maxSpeed)
{
	return this->modeService->GetPlayerMaxSpeed(maxSpeed);
}

void KZPlayer::OnPhysicsSimulate()
{
	MovementPlayer::OnPhysicsSimulate();
	this->modeService->OnPhysicsSimulate();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPhysicsSimulate();
	}
}

void KZPlayer::OnPhysicsSimulatePost()
{
	MovementPlayer::OnPhysicsSimulatePost();
	this->telemetryService->OnPhysicsSimulatePost();
	this->modeService->OnPhysicsSimulatePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPhysicsSimulatePost();
	}
	this->timerService->OnPhysicsSimulatePost();
}

void KZPlayer::OnProcessUsercmds(void *cmds, int numcmds)
{
	this->modeService->OnProcessUsercmds(cmds, numcmds);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnProcessUsercmds(cmds, numcmds);
	}
}

void KZPlayer::OnProcessUsercmdsPost(void *cmds, int numcmds)
{
	this->modeService->OnProcessUsercmdsPost(cmds, numcmds);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnProcessUsercmdsPost(cmds, numcmds);
	}
}

void KZPlayer::OnSetupMove(PlayerCommand *pc)
{
	this->modeService->OnSetupMove(pc);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnSetupMove(pc);
	}
}

void KZPlayer::OnSetupMovePost(PlayerCommand *pc)
{
	this->modeService->OnSetupMovePost(pc);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnSetupMovePost(pc);
	}
}

void KZPlayer::TouchAntibhopTrigger(KzTouchingTrigger touching)
{
	float timeOnGround = g_pKZUtils->GetServerGlobals()->curtime - this->landingTime;
	const KzTrigger *trigger = touching.trigger;
	if (trigger->antibhop.time == 0                              // No jump trigger
		|| timeOnGround <= trigger->antibhop.time                // Haven't touched the trigger for long enough
		|| (this->GetPlayerPawn()->m_fFlags & FL_ONGROUND) == 0) // Not on the ground (for prediction)
	{
		this->antiBhopActive = true;
	}
}

bool KZPlayer::TouchTeleportTrigger(KzTouchingTrigger touching)
{
	bool shouldTeleport = false;
	assert(touching.trigger);

	bool isBhopTrigger = g_pMappingApi->IsBhopTrigger(touching.trigger->type);

	if (isBhopTrigger && touching.groundTouchTime <= 0)
	{
		return shouldTeleport;
	}

	CEntityHandle destinationHandle = GameEntitySystem()->FindFirstEntityHandleByName(touching.trigger->teleport.destination);
	CBaseEntity *destination = dynamic_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(destinationHandle));
	if (!destinationHandle.IsValid() || !destination)
	{
		this->PrintConsole(true, "Invalid teleport destination \"%s\" on trigger with hammerID %i.", touching.trigger->teleport.destination,
						   touching.trigger->hammerId);
		return shouldTeleport;
	}

	Vector destOrigin = destination->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	QAngle destAngles = destination->m_CBodyComponent()->m_pSceneNode()->m_angRotation();
	CBaseEntity *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(touching.trigger->entity));
	Vector triggerOrigin = Vector(0, 0, 0);
	if (trigger)
	{
		triggerOrigin = trigger->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	}

	// NOTE: We only use the trigger's origin if we're using a relative destination, so if
	// we're not using a relative destination and don't have it, then it's fine.
	if (!trigger && touching.trigger->teleport.relative)
	{
		return shouldTeleport;
	}

	if (isBhopTrigger && (this->GetPlayerPawn()->m_fFlags & FL_ONGROUND))
	{
		float touchingTime = g_pKZUtils->GetServerGlobals()->curtime - touching.groundTouchTime;
		if (touchingTime > touching.trigger->teleport.delay)
		{
			shouldTeleport = true;
		}
		else if (touching.trigger->type == KZTRIGGER_SINGLE_BHOP)
		{
			shouldTeleport = this->lastTouchedSingleBhop == touching.trigger->entity;
		}
		else if (touching.trigger->type == KZTRIGGER_SEQUENTIAL_BHOP)
		{
			for (i32 i = 0; i < this->lastTouchedSequentialBhops.GetReadAvailable(); i++)
			{
				CEntityHandle handle = CEntityHandle();
				if (!this->lastTouchedSequentialBhops.Peek(&handle, i))
				{
					assert(0);
					break;
				}
				if (handle == touching.trigger->entity)
				{
					shouldTeleport = true;
					break;
				}
			}
		}
	}
	else if (touching.trigger->type == KZTRIGGER_TELEPORT)
	{
		float touchingTime = g_pKZUtils->GetServerGlobals()->curtime - touching.startTouchTime;
		shouldTeleport = touchingTime > touching.trigger->teleport.delay || touching.trigger->teleport.delay <= 0;
	}

	if (!shouldTeleport)
	{
		return shouldTeleport;
	}

	bool shouldReorientPlayer = touching.trigger->teleport.reorientPlayer && destAngles[YAW] != 0;
	Vector up = Vector(0, 0, 1);
	Vector finalOrigin = destOrigin;

	if (touching.trigger->teleport.relative)
	{
		Vector playerOrigin;
		this->GetOrigin(&playerOrigin);
		Vector playerOffsetFromTrigger = playerOrigin - triggerOrigin;

		if (shouldReorientPlayer)
		{
			VectorRotate(playerOffsetFromTrigger, QAngle(0, destAngles[YAW], 0), playerOffsetFromTrigger);
		}

		finalOrigin = destOrigin + playerOffsetFromTrigger;
	}
	QAngle finalPlayerAngles;
	this->GetAngles(&finalPlayerAngles);
	Vector finalVelocity;
	this->GetVelocity(&finalVelocity);
	if (shouldReorientPlayer)
	{
		// TODO: BUG: sometimes when getting reoriented and holding a movement key
		//  the player's speed will get reduced, almost like velocity rotation
		//  and angle rotation is out of sync leading to counterstrafing.
		VectorRotate(finalVelocity, QAngle(0, destAngles[YAW], 0), finalVelocity);
		finalPlayerAngles[YAW] -= destAngles[YAW];
		this->SetAngles(finalPlayerAngles);
	}

	if (touching.trigger->teleport.resetSpeed)
	{
		this->SetVelocity(vec3_origin);
	}
	else
	{
		this->SetVelocity(finalVelocity);
	}

	this->SetOrigin(finalOrigin);

	return shouldTeleport;
}

void KZPlayer::OnProcessMovement()
{
	MovementPlayer::OnProcessMovement();
	KZ::mode::ApplyModeSettings(this);

	this->modeService->OnProcessMovement();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnProcessMovement();
	}

	KZ::mapapi::OnProcessMovement(this);
	this->jumpstatsService->OnProcessMovement();
	this->checkpointService->TpHoldPlayerStill();
	this->noclipService->HandleMoveCollision();
	this->EnableGodMode();
	this->UpdatePlayerModelAlpha();

	this->lastModifiers = this->modifiers;
	this->lastAntiBhopActive = this->antiBhopActive;
}

void KZPlayer::ResetBhopState()
{
	this->lastTouchedSingleBhop = CEntityHandle();
	// all hail fixed buffers
	this->lastTouchedSequentialBhops = CSequentialBhopBuffer();
}

void KZPlayer::OnProcessMovementPost()
{
	// if the player isn't touching any bhop triggers on ground/a ladder, then
	// reset the singlebhop and sequential bhop state.
	if ((this->GetPlayerPawn()->m_fFlags & FL_ONGROUND || this->GetMoveType() == MOVETYPE_LADDER) && this->bhopTouchCount == 0)
	{
		this->ResetBhopState();
	}

	this->antiBhopActive = false;
	// Check if we're touching any triggers and act accordingly.
	// NOTE: Read through the touch list in reverse order, so
	//  that we resolve most recently touched triggers first.
	// TODO: Move this to trigger service
	FOR_EACH_VEC_BACK(this->kzTriggerTouchList, i)
	{
		const KzTouchingTrigger touching = this->kzTriggerTouchList[i];
		switch (touching.trigger->type)
		{
			case KZTRIGGER_ANTI_BHOP:
			{
				TouchAntibhopTrigger(touching);
			}
			break;

			case KZTRIGGER_TELEPORT:
			case KZTRIGGER_MULTI_BHOP:
			case KZTRIGGER_SINGLE_BHOP:
			case KZTRIGGER_SEQUENTIAL_BHOP:
			{
				if (TouchTeleportTrigger(touching))
				{
					RemoveKzTriggerFromTouchList(touching.trigger);
				}
			}
			break;
		}
	}

	if (this->specService->GetSpectatedPlayer())
	{
		this->specService->GetSpectatedPlayer()->hudService->DrawPanels(this);
	}
	else
	{
		this->hudService->DrawPanels(this);
	}

	this->jumpstatsService->UpdateJump();
	this->modeService->OnProcessMovementPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnProcessMovementPost();
	}
	this->jumpstatsService->OnProcessMovementPost();
	MovementPlayer::OnProcessMovementPost();
}

void KZPlayer::OnPlayerMove()
{
	this->modeService->OnPlayerMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPlayerMove();
	}
}

void KZPlayer::OnPlayerMovePost()
{
	this->modeService->OnPlayerMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPlayerMovePost();
	}
}

void KZPlayer::OnCheckParameters()
{
	this->modeService->OnCheckParameters();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckParameters();
	}
}

void KZPlayer::OnCheckParametersPost()
{
	this->modeService->OnCheckParametersPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckParametersPost();
	}
}

void KZPlayer::OnCanMove()
{
	this->modeService->OnCanMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCanMove();
	}
}

void KZPlayer::OnCanMovePost()
{
	this->modeService->OnCanMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCanMovePost();
	}
}

void KZPlayer::OnFullWalkMove(bool &ground)
{
	this->modeService->OnFullWalkMove(ground);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFullWalkMove(ground);
	}
}

void KZPlayer::OnFullWalkMovePost(bool ground)
{
	this->modeService->OnFullWalkMovePost(ground);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFullWalkMovePost(ground);
	}
}

void KZPlayer::OnMoveInit()
{
	this->modeService->OnMoveInit();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnMoveInit();
	}
}

void KZPlayer::OnMoveInitPost()
{
	this->modeService->OnMoveInitPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnMoveInitPost();
	}
}

void KZPlayer::OnCheckWater()
{
	this->modeService->OnCheckWater();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckWater();
	}
}

void KZPlayer::OnWaterMove()
{
	this->modeService->OnWaterMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnWaterMove();
	}
}

void KZPlayer::OnWaterMovePost()
{
	this->modeService->OnWaterMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnWaterMovePost();
	}
}

void KZPlayer::OnCheckWaterPost()
{
	this->modeService->OnCheckWaterPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckWaterPost();
	}
}

void KZPlayer::OnCheckVelocity(const char *a3)
{
	this->modeService->OnCheckVelocity(a3);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckVelocity(a3);
	}
}

void KZPlayer::OnCheckVelocityPost(const char *a3)
{
	this->modeService->OnCheckVelocityPost(a3);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckVelocityPost(a3);
	}
}

void KZPlayer::OnDuck()
{
	this->modeService->OnDuck();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnDuck();
	}
}

void KZPlayer::OnDuckPost()
{
	this->modeService->OnDuckPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnDuckPost();
	}
}

void KZPlayer::OnCanUnduck()
{
	this->modeService->OnCanUnduck();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCanUnduck();
	}
}

void KZPlayer::OnCanUnduckPost(bool &ret)
{
	this->modeService->OnCanUnduckPost(ret);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCanUnduckPost(ret);
	}
}

void KZPlayer::OnLadderMove()
{
	this->modeService->OnLadderMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnLadderMove();
	}
}

void KZPlayer::OnLadderMovePost()
{
	this->modeService->OnLadderMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnLadderMovePost();
	}
}

void KZPlayer::OnCheckJumpButton()
{
	this->modeService->OnCheckJumpButton();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckJumpButton();
	}
}

void KZPlayer::OnCheckJumpButtonPost()
{
	this->modeService->OnCheckJumpButtonPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckJumpButtonPost();
	}
}

void KZPlayer::OnJump()
{
	this->modeService->OnJump();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnJump();
	}
}

void KZPlayer::OnJumpPost()
{
	this->modeService->OnJumpPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnJumpPost();
	}
}

void KZPlayer::OnAirMove()
{
	this->modeService->OnAirMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnAirMove();
	}
}

void KZPlayer::OnAirMovePost()
{
	this->modeService->OnAirMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnAirMovePost();
	}
}

void KZPlayer::OnAirAccelerate(Vector &wishdir, f32 &wishspeed, f32 &accel)
{
	this->modeService->OnAirAccelerate(wishdir, wishspeed, accel);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnAirAccelerate(wishdir, wishspeed, accel);
	}
	this->jumpstatsService->OnAirAccelerate();
}

void KZPlayer::OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel)
{
	this->modeService->OnAirAcceleratePost(wishdir, wishspeed, accel);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnAirAcceleratePost(wishdir, wishspeed, accel);
	}
	this->jumpstatsService->OnAirAcceleratePost(wishdir, wishspeed, accel);
}

void KZPlayer::OnFriction()
{
	this->modeService->OnFriction();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFriction();
	}
}

void KZPlayer::OnFrictionPost()
{
	this->modeService->OnFrictionPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFrictionPost();
	}
}

void KZPlayer::OnWalkMove()
{
	this->modeService->OnWalkMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnWalkMove();
	}
}

void KZPlayer::OnWalkMovePost()
{
	this->modeService->OnWalkMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnWalkMovePost();
	}
}

void KZPlayer::OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace)
{
	this->modeService->OnTryPlayerMove(pFirstDest, pFirstTrace);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnTryPlayerMove(pFirstDest, pFirstTrace);
	}
	this->jumpstatsService->OnTryPlayerMove();
}

void KZPlayer::OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace)
{
	this->modeService->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
	}
	this->jumpstatsService->OnTryPlayerMovePost();
}

void KZPlayer::OnCategorizePosition(bool bStayOnGround)
{
	this->modeService->OnCategorizePosition(bStayOnGround);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCategorizePosition(bStayOnGround);
	}
}

void KZPlayer::OnCategorizePositionPost(bool bStayOnGround)
{
	this->modeService->OnCategorizePositionPost(bStayOnGround);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCategorizePositionPost(bStayOnGround);
	}
}

void KZPlayer::OnFinishGravity()
{
	this->modeService->OnFinishGravity();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFinishGravity();
	}
}

void KZPlayer::OnFinishGravityPost()
{
	this->modeService->OnFinishGravityPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFinishGravityPost();
	}
}

void KZPlayer::OnCheckFalling()
{
	this->modeService->OnCheckFalling();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckFalling();
	}
}

void KZPlayer::OnCheckFallingPost()
{
	this->modeService->OnCheckFallingPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckFallingPost();
	}
}

void KZPlayer::OnPostPlayerMove()
{
	this->modeService->OnPostPlayerMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPostPlayerMove();
	}
}

void KZPlayer::OnPostPlayerMovePost()
{
	this->modeService->OnPostPlayerMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPostPlayerMovePost();
	}
}

void KZPlayer::OnPostThink()
{
	this->modeService->OnPostThink();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPostThink();
	}
	MovementPlayer::OnPostThink();
}

void KZPlayer::OnPostThinkPost()
{
	this->modeService->OnPostThinkPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPostThinkPost();
	}
}

void KZPlayer::OnStartTouchGround()
{
	this->jumpstatsService->EndJump();
	this->timerService->OnStartTouchGround();
	this->modeService->OnStartTouchGround();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnStartTouchGround();
	}
	FOR_EACH_VEC(this->kzTriggerTouchList, i)
	{
		this->kzTriggerTouchList[i].groundTouchTime = g_pKZUtils->GetServerGlobals()->curtime;
	}
}

void KZPlayer::OnStopTouchGround()
{
	FOR_EACH_VEC(this->kzTriggerTouchList, i)
	{
		KzTouchingTrigger touching = this->kzTriggerTouchList[i];
		if (g_pMappingApi->IsBhopTrigger(touching.trigger->type))
		{
			// set last touched triggers for single and sequential bhop.
			if (touching.trigger->type == KZTRIGGER_SEQUENTIAL_BHOP)
			{
				CEntityHandle handle = touching.trigger->entity;
				this->lastTouchedSequentialBhops.Write(handle);
			}

			// NOTE: For singlebhops, we don't care which type of bhop we last touched, because
			//  otherwise jumping back and forth between a multibhop and a singlebhop wouldn't work.
			// We only care about the most recently touched trigger!
			if (i == 0 && g_pMappingApi->IsBhopTrigger(touching.trigger->type))
			{
				this->lastTouchedSingleBhop = touching.trigger->entity;
			}
		}
	}
	this->jumpstatsService->AddJump();
	this->timerService->OnStopTouchGround();
	this->modeService->OnStopTouchGround();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnStopTouchGround();
	}
}

void KZPlayer::OnChangeMoveType(MoveType_t oldMoveType)
{
	this->jumpstatsService->OnChangeMoveType(oldMoveType);
	this->timerService->OnChangeMoveType(oldMoveType);
	this->modeService->OnChangeMoveType(oldMoveType);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnChangeMoveType(oldMoveType);
	}
}

void KZPlayer::OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	this->lastTeleportTime = g_pKZUtils->GetServerGlobals()->curtime;
	this->jumpstatsService->InvalidateJumpstats("Teleported");
	this->modeService->OnTeleport(origin, angles, velocity);
	this->timerService->OnTeleport(origin, angles, velocity);
}

void KZPlayer::EnableGodMode()
{
	CCSPlayerPawn *pawn = this->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	if (pawn->m_bTakesDamage())
	{
		pawn->m_bTakesDamage(false);
	}
}

void KZPlayer::AddKzTriggerToTouchList(const KzTrigger *trigger)
{
	KzTouchingTrigger touchingTrigger = {};
	touchingTrigger.trigger = trigger;
	touchingTrigger.startTouchTime = g_pKZUtils->GetServerGlobals()->curtime;

	this->kzTriggerTouchList.AddToTail(touchingTrigger);
}

void KZPlayer::RemoveKzTriggerFromTouchList(const KzTrigger *trigger)
{
	FOR_EACH_VEC(this->kzTriggerTouchList, i)
	{
		if (this->kzTriggerTouchList[i].trigger == trigger)
		{
			this->kzTriggerTouchList.Remove(i);
			break;
		}
	}
}

void KZPlayer::MappingApiTriggerStartTouch(const KzTrigger *touched, const KZCourseDescriptor *course)
{
	switch (touched->type)
	{
		case KZTRIGGER_MODIFIER:
		{
			KzMapModifier modifier = touched->modifier;
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
			if (this->timerService->GetTimerRunning())
			{
				if (this->checkpointService->GetCheckpointCount())
				{
					this->languageService->PrintChat(true, false, "Checkpoints Cleared By Map");
				}
				this->checkpointService->ResetCheckpoints(true, false);
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
			this->checkpointService->ResetCheckpoints();
			this->timerService->StartZoneStartTouch(course);
		}
		break;

		case KZTRIGGER_ZONE_END:
		{
			this->timerService->TimerEnd(course);
		}
		break;

		case KZTRIGGER_ZONE_SPLIT:
		{
			this->timerService->SplitZoneStartTouch(course, touched->zone.number);
		}
		break;

		case KZTRIGGER_ZONE_CHECKPOINT:
		{
			this->timerService->CheckpointZoneStartTouch(course, touched->zone.number);
		}
		break;

		case KZTRIGGER_ZONE_STAGE:
		{
			this->timerService->StageZoneStartTouch(course, touched->zone.number);
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_ANTI_BHOP:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
		{
			AddKzTriggerToTouchList(touched);
			if (g_pMappingApi->IsBhopTrigger(touched->type))
			{
				this->bhopTouchCount++;
			}
		}
		break;

		default:
			break;
	}
}

void KZPlayer::MappingApiTriggerEndTouch(const KzTrigger *touched, const KZCourseDescriptor *course)
{
	switch (touched->type)
	{
		case KZTRIGGER_MODIFIER:
		{
			KzMapModifier modifier = touched->modifier;
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
			this->checkpointService->ResetCheckpoints();
			this->timerService->StartZoneEndTouch(course);
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_ANTI_BHOP:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
		{
			RemoveKzTriggerFromTouchList(touched);
			if (g_pMappingApi->IsBhopTrigger(touched->type))
			{
				this->bhopTouchCount--;
			}
		}
		break;

		default:
			break;
	}
}

void KZPlayer::UpdatePlayerModelAlpha()
{
	CCSPlayerPawn *pawn = this->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	Color ogColor = pawn->m_clrRender();
	if (this->hideLegs && pawn->m_clrRender().a() == 255)
	{
		pawn->m_clrRender(Color(255, 255, 255, 254));
	}
	else if (!this->hideLegs && pawn->m_clrRender().a() != 255)
	{
		pawn->m_clrRender(Color(255, 255, 255, 255));
	}
}

bool KZPlayer::JustTeleported()
{
	return g_pKZUtils->GetServerGlobals()->curtime - this->lastTeleportTime < KZ_RECENT_TELEPORT_THRESHOLD;
}

void KZPlayer::ToggleHideLegs()
{
	this->hideLegs = !this->hideLegs;
	this->optionService->SetPreferenceBool("hideLegs", this->hideLegs);
}

void KZPlayer::PlayErrorSound()
{
	utils::PlaySoundToClient(this->GetPlayerSlot(), MV_SND_ERROR);
}

void KZPlayer::TouchTriggersAlongPath(const Vector &start, const Vector &end, const bbox_t &bounds)
{
	if (!this->IsAlive() || this->GetCollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
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
		if (!this->touchedTriggers.HasElement(handle))
		{
			this->GetPlayerPawn()->StartTouch(trigger);
			trigger->StartTouch(this->GetPlayerPawn());
			this->GetPlayerPawn()->Touch(trigger);
			trigger->Touch(this->GetPlayerPawn());
		}
	}
}

void KZPlayer::UpdateTriggerTouchList()
{
	if (!this->IsAlive() || this->GetCollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
	{
		FOR_EACH_VEC(this->touchedTriggers, i)
		{
			CBaseTrigger *trigger = static_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(this->touchedTriggers[i]));
			trigger->EndTouch(this->GetPlayerPawn());
			this->GetPlayerPawn()->EndTouch(trigger);
		}
		return;
	}
	Vector origin;
	this->GetOrigin(&origin);
	bbox_t bounds;
	this->GetBBoxBounds(&bounds);
	CTraceFilterHitAllTriggers filter;
	trace_t tr;
	g_pKZUtils->TracePlayerBBox(origin, origin, bounds, &filter, tr);

	FOR_EACH_VEC(this->touchedTriggers, i)
	{
		CEntityHandle handle = this->touchedTriggers[i];
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(handle));
		if (!trigger)
		{
			this->touchedTriggers.Remove(i);
			continue;
		}
		if (!filter.hitTriggerHandles.HasElement(handle))
		{
			this->GetPlayerPawn()->EndTouch(trigger);
			trigger->EndTouch(this->GetPlayerPawn());
		}
	}

	FOR_EACH_VEC(filter.hitTriggerHandles, i)
	{
		CEntityHandle handle = filter.hitTriggerHandles[i];
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(handle));
		if (!trigger || !V_strstr(trigger->GetClassname(), "trigger_"))
		{
			continue;
		}
		if (!this->touchedTriggers.HasElement(handle))
		{
			trigger->StartTouch(this->GetPlayerPawn());
			trigger->Touch(this->GetPlayerPawn());
			this->GetPlayerPawn()->StartTouch(trigger);
			this->GetPlayerPawn()->Touch(trigger);
		}
	}
}

bool KZPlayer::OnTriggerStartTouch(CBaseTrigger *trigger)
{
	bool retValue = this->modeService->OnTriggerStartTouch(trigger);
	FOR_EACH_VEC(this->styleServices, i)
	{
		retValue &= this->styleServices[i]->OnTriggerStartTouch(trigger);
	}
	return retValue;
}

bool KZPlayer::OnTriggerTouch(CBaseTrigger *trigger)
{
	bool retValue = this->modeService->OnTriggerTouch(trigger);
	FOR_EACH_VEC(this->styleServices, i)
	{
		retValue &= this->styleServices[i]->OnTriggerTouch(trigger);
	}
	return retValue;
}

bool KZPlayer::OnTriggerEndTouch(CBaseTrigger *trigger)
{
	bool retValue = this->modeService->OnTriggerEndTouch(trigger);
	FOR_EACH_VEC(this->styleServices, i)
	{
		retValue &= this->styleServices[i]->OnTriggerEndTouch(trigger);
	}
	return retValue;
}

void KZPlayer::OnChangeTeamPost(i32 team)
{
	this->timerService->OnPlayerJoinTeam(team);
}

CUtlString KZPlayer::ComputeCvarValueFromModeStyles(const char *name)
{
	ConVarHandle cvarHandle = g_pCVar->FindConVar(name);
	if (!cvarHandle.IsValid())
	{
		assert(0);
		META_CONPRINTF("Failed to find %s!\n", name);
		return "";
	}

	if (!name)
	{
		assert(0);
		return "";
	}

	ConVar *cvar = g_pCVar->GetConVar(cvarHandle);
	assert(cvar);
	CVValue_t *cvarValue = cvar->m_cvvDefaultValue;
	CUtlString valueStr;

	switch (cvar->m_eVarType)
	{
		case EConVarType_Bool:
		{
			valueStr.Format("%s", cvarValue->m_bValue ? "true" : "false");
			break;
		}
		case EConVarType_Int16:
		{
			valueStr.Format("%i", cvarValue->m_i16Value);
			break;
		}
		case EConVarType_Int32:
		{
			valueStr.Format("%i", cvarValue->m_i32Value);
			break;
		}
		case EConVarType_Int64:
		{
			valueStr.Format("%lli", cvarValue->m_i64Value);
			break;
		}
		case EConVarType_UInt16:
		{
			valueStr.Format("%u", cvarValue->m_u16Value);
			break;
		}
		case EConVarType_UInt32:
		{
			valueStr.Format("%u", cvarValue->m_u32Value);
			break;
		}
		case EConVarType_UInt64:
		{
			valueStr.Format("%llu", cvarValue->m_u64Value);
			break;
		}

		case EConVarType_Float32:
		{
			valueStr.Format("%f", cvarValue->m_flValue);
			break;
		}

		case EConVarType_Float64:
		{
			valueStr.Format("%lf", cvarValue->m_dbValue);
			break;
		}

		case EConVarType_String:
		{
			valueStr.Format("%s", cvarValue->m_szValue);
			break;
		}

		default:
			assert(0);
			break;
	}

	for (int i = 0; i < MODECVAR_COUNT; i++)
	{
		if (!V_stricmp(KZ::mode::modeCvarNames[i], name))
		{
			valueStr.Format("%s", this->modeService->GetModeConVarValues()[i]);
			break;
		}
	}

	FOR_EACH_VEC(this->styleServices, i)
	{
		if (this->styleServices[i]->GetTweakedConvarValue(name))
		{
			valueStr.Format("%s", this->styleServices[i]->GetTweakedConvarValue(name));
		}
	}
	return valueStr;
}
