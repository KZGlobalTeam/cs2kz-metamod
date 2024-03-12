#include "kz.h"
#include "utils/utils.h"

#include "checkpoint/kz_checkpoint.h"
#include "quiet/kz_quiet.h"
#include "jumpstats/kz_jumpstats.h"
#include "hud/kz_hud.h"
#include "mode/kz_mode.h"
#include "noclip/kz_noclip.h"
#include "tip/kz_tip.h"
#include "noclip/kz_noclip.h"
#include "style/kz_style.h"
#include "spec/kz_spec.h"
#include "timer/kz_timer.h"

#include "tier0/memdbgon.h"

void KZPlayer::Init()
{
	this->hideLegs 	= false;
	this->previousTurnState = TURN_NONE;

	// TODO: initialize every service.
	delete this->checkpointService;
	delete this->jumpstatsService;
	delete this->quietService;
	delete this->hudService;
	delete this->specService;
	delete this->timerService;
	delete this->noclipService;
	delete this->tipService;

	this->checkpointService = new KZCheckpointService(this);
	this->jumpstatsService = new KZJumpstatsService(this);
	this->noclipService = new KZNoclipService(this);
	this->quietService = new KZQuietService(this);
	this->hudService = new KZHUDService(this);
	this->specService = new KZSpecService(this);
	this->timerService = new KZTimerService(this);
	this->tipService = new KZTipService(this);
	KZ::mode::InitModeService(this);
	KZ::style::InitStyleService(this);
}

void KZPlayer::Reset()
{
	MovementPlayer::Reset();
	this->hideLegs 	= false;
	this->previousTurnState = TURN_NONE;

	// TODO: reset every service.
	this->checkpointService->Reset();
	this->noclipService->Reset();
	this->quietService->Reset();
	this->jumpstatsService->Reset();
	this->hudService->Reset();
	this->timerService->Reset();
	this->tipService->Reset();

	// TODO: Make a config for default mode and style
	g_pKZModeManager->SwitchToMode(this, "VNL", true);
	g_pKZStyleManager->SwitchToStyle(this, "NRM", true);
}

float KZPlayer::GetPlayerMaxSpeed()
{
	return this->modeService->GetPlayerMaxSpeed();
}

void KZPlayer::OnPhysicsSimulate()
{
	MovementPlayer::OnPhysicsSimulate();
	this->modeService->OnPhysicsSimulate();
	this->styleService->OnPhysicsSimulate();
}

void KZPlayer::OnPhysicsSimulatePost()
{
	MovementPlayer::OnPhysicsSimulatePost();
	this->modeService->OnPhysicsSimulatePost();
	this->styleService->OnPhysicsSimulatePost();
	this->timerService->OnPhysicsSimulatePost();
}

void KZPlayer::OnProcessUsercmds(void *cmds, int numcmds)
{
	this->modeService->OnProcessUsercmds(cmds, numcmds);
	this->styleService->OnProcessUsercmds(cmds, numcmds);
}

void KZPlayer::OnProcessUsercmdsPost(void *cmds, int numcmds)
{
	this->modeService->OnProcessUsercmdsPost(cmds, numcmds);
	this->styleService->OnProcessUsercmdsPost(cmds, numcmds);
}

void KZPlayer::OnProcessMovement()
{
	MovementPlayer::OnProcessMovement();
	KZ::mode::ApplyModeSettings(this);
	this->modeService->OnProcessMovement();
	this->styleService->OnProcessMovement();
	this->jumpstatsService->OnProcessMovement();
	this->checkpointService->TpHoldPlayerStill();
	this->noclipService->HandleMoveCollision();
	this->EnableGodMode();
	this->UpdatePlayerModelAlpha();
}

void KZPlayer::OnProcessMovementPost()
{
	this->hudService->DrawSpeedPanel();
	this->jumpstatsService->UpdateJump();
	this->modeService->OnProcessMovementPost();
	this->styleService->OnProcessMovementPost();
	this->jumpstatsService->OnProcessMovementPost();
	MovementPlayer::OnProcessMovementPost();
}

void KZPlayer::OnPlayerMove()
{
	this->modeService->OnPlayerMove();
	this->styleService->OnPlayerMove();
}

void KZPlayer::OnPlayerMovePost()
{
	this->modeService->OnPlayerMovePost();
	this->styleService->OnPlayerMovePost();
}

void KZPlayer::OnCheckParameters()
{
	this->modeService->OnCheckParameters();
	this->styleService->OnCheckParameters();
}

void KZPlayer::OnCheckParametersPost()
{
	this->modeService->OnCheckParametersPost();
	this->styleService->OnCheckParametersPost();
}

void KZPlayer::OnCanMove()
{
	this->modeService->OnCanMove();
	this->styleService->OnCanMove();
}

void KZPlayer::OnCanMovePost()
{
	this->modeService->OnCanMovePost();
	this->styleService->OnCanMovePost();
}

void KZPlayer::OnFullWalkMove(bool &ground)
{
	this->modeService->OnFullWalkMove(ground);
	this->styleService->OnFullWalkMove(ground);
}

void KZPlayer::OnFullWalkMovePost(bool ground)
{
	this->modeService->OnFullWalkMovePost(ground);
	this->styleService->OnFullWalkMovePost(ground);
}

void KZPlayer::OnMoveInit()
{
	this->modeService->OnMoveInit();
	this->styleService->OnMoveInit();
}

void KZPlayer::OnMoveInitPost()
{
	this->modeService->OnMoveInitPost();
	this->styleService->OnMoveInitPost();
}

void KZPlayer::OnCheckWater()
{
	this->modeService->OnCheckWater();
	this->styleService->OnCheckWater();
}

void KZPlayer::OnWaterMove()
{
	this->modeService->OnWaterMove();
	this->styleService->OnWaterMove();
}

void KZPlayer::OnWaterMovePost()
{
	this->modeService->OnWaterMovePost();
	this->styleService->OnWaterMovePost();
}

void KZPlayer::OnCheckWaterPost()
{
	this->modeService->OnCheckWaterPost();
	this->styleService->OnCheckWaterPost();
}

void KZPlayer::OnCheckVelocity(const char *a3)
{
	this->modeService->OnCheckVelocity(a3);
	this->styleService->OnCheckVelocity(a3);
}

void KZPlayer::OnCheckVelocityPost(const char *a3)
{
	this->modeService->OnCheckVelocityPost(a3);
	this->styleService->OnCheckVelocityPost(a3);
}

void KZPlayer::OnDuck()
{
	this->modeService->OnDuck();
	this->styleService->OnDuck();
}

void KZPlayer::OnDuckPost()
{
	this->modeService->OnDuckPost();
	this->styleService->OnDuckPost();
}

void KZPlayer::OnCanUnduck()
{
	this->modeService->OnCanUnduck();
	this->styleService->OnCanUnduck();
}

void KZPlayer::OnCanUnduckPost(bool &ret)
{
	this->modeService->OnCanUnduckPost(ret);
	this->styleService->OnCanUnduckPost(ret);
}

void KZPlayer::OnLadderMove()
{
	this->modeService->OnLadderMove();
	this->styleService->OnLadderMove();
}

void KZPlayer::OnLadderMovePost()
{
	this->modeService->OnLadderMovePost();
	this->styleService->OnLadderMovePost();
}

void KZPlayer::OnCheckJumpButton()
{
	this->modeService->OnCheckJumpButton();
	this->styleService->OnCheckJumpButton();
}

void KZPlayer::OnCheckJumpButtonPost()
{
	this->modeService->OnCheckJumpButtonPost();
	this->styleService->OnCheckJumpButtonPost();
}

void KZPlayer::OnJump()
{
	this->modeService->OnJump();
	this->styleService->OnJump();
}

void KZPlayer::OnJumpPost()
{
	this->modeService->OnJumpPost();
	this->styleService->OnJumpPost();
}

void KZPlayer::OnAirMove()
{
	this->modeService->OnAirMove();
	this->styleService->OnAirMove();
}

void KZPlayer::OnAirMovePost()
{
	this->modeService->OnAirMovePost();
	this->styleService->OnAirMovePost();
}

void KZPlayer::OnAirAccelerate(Vector &wishdir, f32 &wishspeed, f32 &accel)
{
	this->modeService->OnAirAccelerate(wishdir, wishspeed, accel);
	this->styleService->OnAirAccelerate(wishdir, wishspeed, accel);
	this->jumpstatsService->OnAirAccelerate();
}

void KZPlayer::OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel)
{
	this->modeService->OnAirAcceleratePost(wishdir, wishspeed, accel);
	this->styleService->OnAirAcceleratePost(wishdir, wishspeed, accel);
	this->jumpstatsService->OnAirAcceleratePost(wishdir, wishspeed, accel);
}

void KZPlayer::OnFriction()
{
	this->modeService->OnFriction();
	this->styleService->OnFriction();
}

void KZPlayer::OnFrictionPost()
{
	this->modeService->OnFrictionPost();
	this->styleService->OnFrictionPost();
}

void KZPlayer::OnWalkMove()
{
	this->modeService->OnWalkMove();
	this->styleService->OnWalkMove();
}

void KZPlayer::OnWalkMovePost()
{
	this->modeService->OnWalkMovePost();
	this->styleService->OnWalkMovePost();
}

void KZPlayer::OnTryPlayerMove(Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	this->modeService->OnTryPlayerMove(pFirstDest, pFirstTrace);
	this->styleService->OnTryPlayerMove(pFirstDest, pFirstTrace);
	this->jumpstatsService->OnTryPlayerMove();
}

void KZPlayer::OnTryPlayerMovePost(Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	this->modeService->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
	this->styleService->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
	this->jumpstatsService->OnTryPlayerMovePost();
}

void KZPlayer::OnCategorizePosition(bool bStayOnGround)
{
	this->modeService->OnCategorizePosition(bStayOnGround);
	this->styleService->OnCategorizePosition(bStayOnGround);
}

void KZPlayer::OnCategorizePositionPost(bool bStayOnGround)
{
	this->modeService->OnCategorizePositionPost(bStayOnGround);
	this->styleService->OnCategorizePositionPost(bStayOnGround);
}

void KZPlayer::OnFinishGravity()
{
	this->modeService->OnFinishGravity();
	this->styleService->OnFinishGravity();
}

void KZPlayer::OnFinishGravityPost()
{
	this->modeService->OnFinishGravityPost();
	this->styleService->OnFinishGravityPost();
}

void KZPlayer::OnCheckFalling()
{
	this->modeService->OnCheckFalling();
	this->styleService->OnCheckFalling();
}

void KZPlayer::OnCheckFallingPost()
{
	this->modeService->OnCheckFallingPost();
	this->styleService->OnCheckFallingPost();
}

void KZPlayer::OnPostPlayerMove()
{
	this->modeService->OnPostPlayerMove();
	this->styleService->OnPostPlayerMove();
}

void KZPlayer::OnPostPlayerMovePost()
{
	this->modeService->OnPostPlayerMovePost();
	this->styleService->OnPostPlayerMovePost();
}

void KZPlayer::OnPostThink()
{
	this->modeService->OnPostThink();
	this->styleService->OnPostThink();
	MovementPlayer::OnPostThink();
}

void KZPlayer::OnPostThinkPost()
{
	this->modeService->OnPostThinkPost();
	this->styleService->OnPostThinkPost();
}

void KZPlayer::OnStartTouchGround()
{
	this->jumpstatsService->EndJump();
	this->modeService->OnStartTouchGround();
	this->styleService->OnStartTouchGround();
}

void KZPlayer::OnStopTouchGround()
{
	this->jumpstatsService->AddJump();
	this->modeService->OnStopTouchGround();
	this->styleService->OnStopTouchGround();
}

void KZPlayer::OnChangeMoveType(MoveType_t oldMoveType)
{
	this->jumpstatsService->OnChangeMoveType(oldMoveType);
	this->timerService->OnChangeMoveType(oldMoveType);
	this->modeService->OnChangeMoveType(oldMoveType);
	this->styleService->OnChangeMoveType(oldMoveType);								
}

void KZPlayer::OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	this->jumpstatsService->InvalidateJumpstats("Teleported");
	this->modeService->OnTeleport(origin, angles, velocity);
	this->timerService->OnTeleport(origin, angles, velocity);
}

void KZPlayer::EnableGodMode()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn)
	{
		return;	
	}
	if (pawn->m_bTakesDamage())
	{
		pawn->m_bTakesDamage(false);
	}
}

void KZPlayer::StartZoneStartTouch()
{
	this->checkpointService->ResetCheckpoints();
	this->timerService->TimerStop(false);
}

void KZPlayer::StartZoneEndTouch()
{
	if (!this->noclipService->IsNoclipping())
	{
		this->checkpointService->ResetCheckpoints();
		this->timerService->TimerStart("");
	}
}

void KZPlayer::EndZoneStartTouch()
{
	// TODO: get course name
	this->timerService->TimerEnd("");
}

void KZPlayer::UpdatePlayerModelAlpha()
{
	CCSPlayerPawn *pawn = this->GetPawn();
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

void KZPlayer::ToggleHideLegs()
{
	this->hideLegs = !this->hideLegs;
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
	trace_t_s2 tr;
	g_pKZUtils->TracePlayerBBox(start, end, bounds, &filter, tr);
	FOR_EACH_VEC(filter.hitTriggerHandles, i)
	{
		CEntityHandle handle = filter.hitTriggerHandles[i];
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetBaseEntity(handle));
		if (!trigger || !V_strstr(trigger->GetClassname(), "trigger_"))
		{
			continue;
		}
		if (!this->touchedTriggers.HasElement(handle))
		{
			this->GetPawn()->StartTouch(trigger);
			trigger->StartTouch(this->GetPawn());
			this->GetPawn()->Touch(trigger);
			trigger->Touch(this->GetPawn());
		}
	}
}

void KZPlayer::UpdateTriggerTouchList()
{
	if (!this->IsAlive() || this->GetCollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
	{
		FOR_EACH_VEC(this->touchedTriggers, i)
		{
			CBaseTrigger *trigger = static_cast<CBaseTrigger *>(GameEntitySystem()->GetBaseEntity(this->touchedTriggers[i]));
			trigger->EndTouch(this->GetPawn());
			this->GetPawn()->EndTouch(trigger);
		}
		return;
	}
	Vector origin;
	this->GetOrigin(&origin);
	bbox_t bounds;
	this->GetBBoxBounds(&bounds);
	CTraceFilterHitAllTriggers filter;
	trace_t_s2 tr;
	g_pKZUtils->TracePlayerBBox(origin, origin, bounds, &filter, tr);

	FOR_EACH_VEC(this->touchedTriggers, i)
	{
		CEntityHandle handle = this->touchedTriggers[i];
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetBaseEntity(handle));
		if (!trigger)
		{
			this->touchedTriggers.Remove(i);
			continue;
		}
		if (!filter.hitTriggerHandles.HasElement(handle))
		{
			this->GetPawn()->EndTouch(trigger);
			trigger->EndTouch(this->GetPawn());
		}
	}

	FOR_EACH_VEC(filter.hitTriggerHandles, i)
	{
		CEntityHandle handle = filter.hitTriggerHandles[i];
		CBaseTrigger *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetBaseEntity(handle));
		if (!trigger || !V_strstr(trigger->GetClassname(), "trigger_"))
		{
			continue;
		}
		if (!this->touchedTriggers.HasElement(handle))
		{
			trigger->StartTouch(this->GetPawn());
			this->GetPawn()->StartTouch(trigger);
			trigger->Touch(this->GetPawn());
			this->GetPawn()->Touch(trigger);
		}
	}
}

bool KZPlayer::OnTriggerStartTouch(CBaseTrigger *trigger)
{
	bool retValue = this->modeService->OnTriggerStartTouch(trigger);
	retValue &= this->styleService->OnTriggerStartTouch(trigger);
	return retValue;
}

bool KZPlayer::OnTriggerTouch(CBaseTrigger *trigger)
{
	bool retValue = this->modeService->OnTriggerTouch(trigger);
	retValue &= this->styleService->OnTriggerTouch(trigger);
	return retValue;
}

bool KZPlayer::OnTriggerEndTouch(CBaseTrigger *trigger)
{
	bool retValue = this->modeService->OnTriggerEndTouch(trigger);
	retValue &= this->styleService->OnTriggerEndTouch(trigger);
	return retValue;
}

void KZPlayer::OnChangeTeamPost(i32 team)
{
	this->timerService->OnPlayerJoinTeam(team);
}
