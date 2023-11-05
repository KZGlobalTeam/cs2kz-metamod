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
}

void KZPlayer::OnStartProcessMovement()
{
	MovementPlayer::OnStartProcessMovement();
	this->jumpstatsService->OnStartProcessMovement();
	this->checkpointService->TpHoldPlayerStill();
	this->EnableGodMode();
	this->HandleMoveCollision();
	this->UpdatePlayerModelAlpha();
}

void KZPlayer::OnStopProcessMovement()
{
	this->hudService->DrawSpeedPanel();
	this->jumpstatsService->UpdateJump();
	MovementPlayer::OnStopProcessMovement();
	this->jumpstatsService->TrackJumpstatsVariables();
}

void KZPlayer::OnStartTouchGround()
{
	this->jumpstatsService->EndJump();
}

void KZPlayer::OnStopTouchGround()
{
	this->jumpstatsService->AddJump();
}

void KZPlayer::OnChangeMoveType(MoveType_t oldMoveType)
{
	this->jumpstatsService->OnChangeMoveType(oldMoveType);
}

void KZPlayer::OnAirAcceleratePre(Vector &wishdir, f32 &wishspeed, f32 &accel)
{
	this->jumpstatsService->OnAirAcceleratePre();
}

void KZPlayer::OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel)
{
	this->jumpstatsService->OnAirAcceleratePost(wishdir, wishspeed, accel);
}

void KZPlayer::EnableGodMode()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	if (pawn->m_bTakesDamage())
	{
		pawn->m_bTakesDamage(false);
	}
}

void KZPlayer::HandleMoveCollision()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
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
		utils::PrintChatAll("%s %s finished the map with a %s run of %s!%s",
							KZ_CHAT_PREFIX,
							controller->m_pEntity->m_name,
							this->checkpointService->tpCount ? "TP" : "PRO",
							time,
							tpCount);
	}
	MovementPlayer::EndZoneStartTouch();
}

void KZPlayer::UpdatePlayerModelAlpha()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	
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
