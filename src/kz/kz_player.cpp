#include "kz.h"
#include "utils/utils.h"
#include "igameevents.h"
#include "tier0/memdbgon.h"

#include "checkpoint/kz_checkpoint.h"
#include "quiet/kz_quiet.h"
#include "jumpstats/kz_jumpstats.h"
#include "hud/kz_hud.h"
#include "mode/kz_mode.h"

void KZPlayer::Init()
{
	this->inNoclip = false;
	this->hideLegs = false;
	this->previousTurnState = TURN_NONE;

	// TODO: initialize every service.
	delete this->checkpointService;
	delete this->jumpstatsService;
	delete this->quietService;
	delete this->hudService;
	this->checkpointService = new KZCheckpointService(this);
	this->jumpstatsService = new KZJumpstatsService(this);
	this->quietService = new KZQuietService(this);
	this->hudService = new KZHUDService(this);
	KZ::mode::InitModeService(this);
}

void KZPlayer::Reset()
{
	MovementPlayer::Reset();

	// TODO: reset every service.
	this->checkpointService->Reset();
	this->quietService->Reset();
	this->jumpstatsService->Reset();
	// TODO: Make a cvar for default mode
	g_pKZModeManager->SwitchToMode(this, "VNL", true);
}

float KZPlayer::GetPlayerMaxSpeed()
{
	return this->modeService->GetPlayerMaxSpeed();
}

void KZPlayer::OnProcessUsercmds(void *cmds, int numcmds)
{
	this->modeService->OnProcessUsercmds(cmds, numcmds);
}

void KZPlayer::OnProcessUsercmdsPost(void *cmds, int numcmds)
{
	this->modeService->OnProcessUsercmdsPost(cmds, numcmds);
}

void KZPlayer::OnProcessMovement()
{
	MovementPlayer::OnProcessMovement();
	KZ::mode::ApplyModeCvarValues(this);
	this->modeService->OnProcessMovement();
	this->jumpstatsService->OnProcessMovement();
	this->checkpointService->TpHoldPlayerStill();
	this->EnableGodMode();
	this->HandleMoveCollision();
	this->UpdatePlayerModelAlpha();
}

void KZPlayer::OnProcessMovementPost()
{
	this->hudService->DrawSpeedPanel();
	this->jumpstatsService->UpdateJump();
	MovementPlayer::OnProcessMovementPost();
	this->modeService->OnProcessMovementPost();
	this->jumpstatsService->OnProcessMovementPost();
}

void KZPlayer::OnPlayerMove()
{
	this->modeService->OnPlayerMove();
}

void KZPlayer::OnPlayerMovePost()
{
	this->modeService->OnPlayerMovePost();
}

void KZPlayer::OnCheckParameters()
{
	this->modeService->OnCheckParameters();
}
void KZPlayer::OnCheckParametersPost()
{
	this->modeService->OnCheckParametersPost();
}
void KZPlayer::OnCanMove()
{
	this->modeService->OnCanMove();
}
void KZPlayer::OnCanMovePost()
{
	this->modeService->OnCanMovePost();
}
void KZPlayer::OnFullWalkMove(bool &ground)
{
	this->modeService->OnFullWalkMove(ground);
}
void KZPlayer::OnFullWalkMovePost(bool ground)
{
	this->modeService->OnFullWalkMovePost(ground);
}
void KZPlayer::OnMoveInit()
{
	this->modeService->OnMoveInit();
}
void KZPlayer::OnMoveInitPost()
{
	this->modeService->OnMoveInitPost();
}
void KZPlayer::OnCheckWater()
{
	this->modeService->OnCheckWater();
}
void KZPlayer::OnWaterMove()
{
	this->modeService->OnWaterMove();
}
void KZPlayer::OnWaterMovePost()
{
	this->modeService->OnWaterMovePost();
}
void KZPlayer::OnCheckWaterPost()
{
	this->modeService->OnCheckWaterPost();
}
void KZPlayer::OnCheckVelocity(const char *a3)
{
	this->modeService->OnCheckVelocity(a3);
}
void KZPlayer::OnCheckVelocityPost(const char *a3)
{
	this->modeService->OnCheckVelocityPost(a3);
}
void KZPlayer::OnDuck()
{
	this->modeService->OnDuck();
}
void KZPlayer::OnDuckPost()
{
	this->modeService->OnDuckPost();
}
int KZPlayer::OnCanUnduck()
{
	return this->modeService->OnCanUnduck();
}
void KZPlayer::OnCanUnduckPost()
{
	this->modeService->OnCanUnduckPost();
}
void KZPlayer::OnLadderMove()
{
	this->modeService->OnLadderMove();
}
void KZPlayer::OnLadderMovePost()
{
	this->modeService->OnLadderMovePost();
}
void KZPlayer::OnCheckJumpButton()
{
	this->modeService->OnCheckJumpButton();
}
void KZPlayer::OnCheckJumpButtonPost()
{
	this->modeService->OnCheckJumpButtonPost();
}
void KZPlayer::OnJump()
{
	this->modeService->OnJump();
}
void KZPlayer::OnJumpPost()
{
	this->modeService->OnJumpPost();
}
void KZPlayer::OnAirAccelerate(Vector &wishdir, f32 &wishspeed, f32 &accel)
{
	this->modeService->OnAirAccelerate(wishdir, wishspeed, accel);
	this->jumpstatsService->OnAirAccelerate();
}
void KZPlayer::OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel)
{
	this->modeService->OnAirAcceleratePost(wishdir, wishspeed, accel);
	this->jumpstatsService->OnAirAcceleratePost(wishdir, wishspeed, accel);
}
void KZPlayer::OnFriction()
{
	this->modeService->OnFriction();
}
void KZPlayer::OnFrictionPost()
{
	this->modeService->OnFrictionPost();
}
void KZPlayer::OnWalkMove()
{
	this->modeService->OnWalkMove();
}
void KZPlayer::OnWalkMovePost()
{
	this->modeService->OnWalkMovePost();
}
void KZPlayer::OnTryPlayerMove(Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	this->modeService->OnTryPlayerMove(pFirstDest, pFirstTrace);
	this->jumpstatsService->OnTryPlayerMove();
}
void KZPlayer::OnTryPlayerMovePost(Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	this->modeService->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
	this->jumpstatsService->OnTryPlayerMovePost();
}
void KZPlayer::OnCategorizePosition(bool bStayOnGround)
{
	this->modeService->OnCategorizePosition(bStayOnGround);
}
void KZPlayer::OnCategorizePositionPost(bool bStayOnGround)
{
	this->modeService->OnCategorizePositionPost(bStayOnGround);
}
void KZPlayer::OnFinishGravity()
{
	this->modeService->OnFinishGravity();
}
void KZPlayer::OnFinishGravityPost()
{
	this->modeService->OnFinishGravityPost();
}
void KZPlayer::OnCheckFalling()
{
	this->modeService->OnCheckFalling();
}
void KZPlayer::OnCheckFallingPost()
{
	this->modeService->OnCheckFallingPost();
}
void KZPlayer::OnPostPlayerMove()
{
	this->modeService->OnPostPlayerMove();
}
void KZPlayer::OnPostPlayerMovePost()
{
	this->modeService->OnPostPlayerMovePost();
}
void KZPlayer::OnPostThink()
{
	this->modeService->OnPostThink();
	MovementPlayer::OnPostThink();
}
void KZPlayer::OnPostThinkPost()
{
	this->modeService->OnPostThinkPost();
}

void KZPlayer::OnStartTouchGround()
{
	this->jumpstatsService->EndJump();
	this->modeService->OnStartTouchGround();
}

void KZPlayer::OnStopTouchGround()
{
	this->jumpstatsService->AddJump();
	this->modeService->OnStopTouchGround();
}

void KZPlayer::OnChangeMoveType(MoveType_t oldMoveType)
{
	this->jumpstatsService->OnChangeMoveType(oldMoveType);
	this->modeService->OnChangeMoveType(oldMoveType);
}

void KZPlayer::OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	this->jumpstatsService->InvalidateJumpstats("Teleported");
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

void KZPlayer::HandleMoveCollision()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn)
	{
		return;
	}
	if (pawn->m_lifeState() != LIFE_ALIVE)
	{
		DisableNoclip();
		return;
	}
	if (this->inNoclip)
	{
		if (pawn->m_MoveType() != MOVETYPE_NOCLIP)
		{
			utils::SetEntityMoveType(pawn, MOVETYPE_NOCLIP);
			this->InvalidateTimer();
		}
		if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
		{
			pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_STANDARD;
			utils::EntityCollisionRulesChanged(pawn);
		}
		this->InvalidateTimer();
	}
	else
	{
		if (pawn->m_MoveType() == MOVETYPE_NOCLIP)
		{
			utils::SetEntityMoveType(pawn, MOVETYPE_WALK);
			this->InvalidateTimer();
		}
		if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_NOTRIGGER)
		{
			pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_NOTRIGGER;
			utils::EntityCollisionRulesChanged(pawn);
		}
	}
}

void KZPlayer::StartZoneStartTouch()
{
	MovementPlayer::StartZoneStartTouch();
	this->checkpointService->Reset();
}

void KZPlayer::StartZoneEndTouch()
{
	if (!this->inNoclip)
	{
		this->checkpointService->Reset();
		MovementPlayer::StartZoneEndTouch();
	}
}

void KZPlayer::EndZoneStartTouch()
{
	if (this->timerIsRunning)
	{
		CCSPlayerController *controller = this->GetController();
		char time[32];
		i32 ticks = this->tickCount - this->timerStartTick;
		i32 chars = utils::FormatTimerText(ticks, time, sizeof(time));
		char tpCount[32] = "";
		if (this->checkpointService->tpCount)
		{
			snprintf(tpCount, sizeof(tpCount), " (%i teleports)", this->checkpointService->tpCount);
		}
		utils::CPrintChatAll("%s %s finished the map with a %s run of %s!%s",
			KZ_CHAT_PREFIX,
			controller->m_iszPlayerName(),
			this->checkpointService->tpCount ? "TP" : "PRO",
			time,
			tpCount);
	}
	MovementPlayer::EndZoneStartTouch();
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

void KZPlayer::ToggleNoclip()
{
	this->inNoclip = !this->inNoclip;
}

void KZPlayer::DisableNoclip()
{
	this->inNoclip = false;
}

void KZPlayer::PlayCheckpointSound()
{
	utils::PlaySoundToClient(this->GetPlayerSlot(), KZ_SND_SET_CP);
}

void KZPlayer::PlayTeleportSound()
{
	utils::PlaySoundToClient(this->GetPlayerSlot(), KZ_SND_DO_TP);
}
