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
#include "timer/kz_timer.h"
#include "tip/kz_tip.h"

#include "sdk/entity/cbasetrigger.h"

#include "steam/isteamgameserver.h"
#include "tier0/memdbgon.h"

extern CSteamGameServerAPIContext g_steamAPI;

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
	KZ::mode::InitModeService(this);
	KZ::style::InitStyleService(this);
}

void KZPlayer::Reset()
{
	MovementPlayer::Reset();

	// Reset services that should not persist across player sessions.
	this->languageService->Reset();
	this->tipService->Reset();
	this->modeService->Reset();
	this->optionService->Reset();

	g_pKZModeManager->SwitchToMode(this, KZOptionService::GetOptionStr("defaultMode", KZ_DEFAULT_MODE), true, true);
	g_pKZStyleManager->ClearStyles(this, true);
}

void KZPlayer::OnPlayerActive()
{
	// Reset services that should not persist across map changes.

	this->hideLegs = this->optionService->GetPreferenceBool("hideLegs", false);
	this->checkpointService->Reset();
	this->noclipService->Reset();
	this->quietService->Reset();
	this->jumpstatsService->Reset();
	this->hudService->Reset();
	this->timerService->Reset();
	this->specService->Reset();

	// Refresh the convars because they couldn't receive the message when connecting.
	g_pKZModeManager->SwitchToMode(this, this->modeService->GetModeName(), true, true);
	g_pKZStyleManager->RefreshStyles(this);
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

void KZPlayer::OnSetupMove(CUserCmd *pb)
{
	this->modeService->OnSetupMove(pb);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnSetupMove(pb);
	}
}

void KZPlayer::OnSetupMovePost(CUserCmd *pb)
{
	this->modeService->OnSetupMovePost(pb);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnSetupMovePost(pb);
	}
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
	this->jumpstatsService->OnProcessMovement();
	this->checkpointService->TpHoldPlayerStill();
	this->noclipService->HandleMoveCollision();
	this->EnableGodMode();
	this->UpdatePlayerModelAlpha();
}

void KZPlayer::OnProcessMovementPost()
{
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
}

void KZPlayer::OnStopTouchGround()
{
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

void KZPlayer::StartZoneStartTouch()
{
	this->checkpointService->ResetCheckpoints();
	this->timerService->StartZoneStartTouch();
}

void KZPlayer::StartZoneEndTouch()
{
	if (!this->noclipService->IsNoclipping())
	{
		this->checkpointService->ResetCheckpoints();
		this->timerService->StartZoneEndTouch();
	}
}

void KZPlayer::EndZoneStartTouch()
{
	// TODO: get course name
	this->timerService->TimerEnd("Main");
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
