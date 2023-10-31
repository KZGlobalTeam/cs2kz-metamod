#include "kz.h"
#include "utils/utils.h"
#include "igameevents.h"
#include "tier0/memdbgon.h"

static const Vector NULL_VECTOR = Vector(0, 0, 0);

void KZPlayer::EnableGodMode()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	if (pawn->m_bTakesDamage())
	{
		pawn->m_bTakesDamage(false);
	}
}

void KZPlayer::OnStartTouchGround()
{
	if (this->jumps.Count() > 0)
	{
		Jump *jump = &this->jumps.Tail();
		jump->End();
		if ((jump->GetOffset() > -0.03125 && jump->IsValid()) || this->jsAlways)
		{
			utils::PrintChat(this->GetPawn(), " \x06 KZ\x08 | \x0A%.1f\x08 |\x05 %i\x08 Strafes |\x08 Sync\x05 %2.f%%\x08 |\x05 %.2f\x08 Pre |\x05 %.2f\x08 Max\xe2\x80\xa9\x08 BA\x05 %2.0f%% \x08| OL\x05 %2.0f%% \x08| DA\x05 %2.0f%% \x08|\x05 %.1f\x08 Deviation |\x05 %.1f\x08 Width |\x05 %.2f\x08 Height",
				jump->GetDistance(),
				jump->strafes.Count(),
				jump->GetSync() * 100.0f,
				this->takeoffVelocity.Length2D(),
				jump->GetMaxSpeed(),
				jump->GetBadAngles() * 100,
				jump->GetOverlap() * 100,
				jump->GetDeadAir() * 100,
				jump->GetDeviation(),
				jump->GetWidth(),
				jump->GetMaxHeight());
		}
	}
}

void KZPlayer::OnStopTouchGround()
{
	this->jumps.AddToTail(Jump(this));
}

void KZPlayer::OnAirAcceleratePre(Vector &wishdir, f32 &wishspeed, f32 &accel)
{
	AACall call;
	this->GetVelocity(&call.velocityPre);

	// moveDataPost is still the movedata from last tick.
	call.externalSpeedDiff = call.velocityPre.Length2D() - this->moveDataPost.m_vecVelocity.Length2D();

	call.curtime = utils::GetServerGlobals()->curtime;
	call.tickcount = utils::GetServerGlobals()->tickcount;
	Strafe *strafe = this->jumps.Tail().GetCurrentStrafe();
	strafe->aaCalls.AddToTail(call);
}

void KZPlayer::OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel)
{
	this->jumps.Tail().UpdateAACallPost(wishdir, wishspeed, accel);
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
		}
		if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
		{
			pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_STANDARD;
			utils::EntityCollisionRulesChanged(pawn);
		}
	}
	else
	{
		if (pawn->m_MoveType() == MOVETYPE_NOCLIP)
		{
			utils::SetEntityMoveType(pawn, MOVETYPE_WALK);
		}
		if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_NOTRIGGER)
		{
			pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_NOTRIGGER;
			utils::EntityCollisionRulesChanged(pawn);
		}
	}
}

void KZPlayer::UpdatePlayerModelAlpha()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	Color ogColor = pawn->m_clrRender();
	if (pawn->m_clrRender().a() != 254)
	{
		pawn->m_clrRender(Color(255, 255, 255, 254));
	}
	else
	{
		pawn->m_clrRender(Color(255, 255, 255, 255));
	}
}

void KZPlayer::ToggleNoclip()
{
	this->inNoclip = !this->inNoclip;
}

void KZPlayer::DisableNoclip()
{
	this->inNoclip = false;
}

void KZPlayer::SetCheckpoint()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	u32 flags = pawn->m_fFlags();
	if (!(flags & FL_ONGROUND) && !(pawn->m_MoveType() == MOVETYPE_LADDER))
	{
		utils::PrintChat(this->GetPawn(), "Checkpoint unavailable in the air.");
		return;
	}
	
	Checkpoint cp = {};
	this->GetOrigin(&cp.origin);
	this->GetAngles(&cp.angles);
	cp.slopeDropHeight = pawn->m_flSlopeDropHeight();
	cp.slopeDropOffset = pawn->m_flSlopeDropOffset();
	if (this->GetMoveServices())
	{
		cp.ladderNormal = this->GetMoveServices()->m_vecLadderNormal();
		cp.onLadder = pawn->m_MoveType() == MOVETYPE_LADDER;
	}
	cp.groundEnt = pawn->m_hGroundEntity();
	this->checkpoints.AddToTail(cp);
	// newest checkpoints aren't deleted after using prev cp.
	this->currentCpIndex = this->checkpoints.Count() - 1;
	utils::PrintChat(this->GetPawn(), "Checkpoint (#%i)", this->currentCpIndex);
}

void KZPlayer::DoTeleport(i32 index)
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	if (checkpoints.Count() <= 0)
	{
		utils::PrintChat(this->GetPawn(), "No checkpoints available.");
		return;
	}
	const Checkpoint cp = this->checkpoints[this->currentCpIndex];

	// If we teleport the player to the same origin, the player ends just a slightly bit off from where they are supposed to be...
	Vector currentOrigin;
	this->GetOrigin(&currentOrigin);

	// If we teleport the player to this origin every tick, they will end up NOT on this origin in the end somehow.
	// So we only set the player origin if it doesn't match.
	if (currentOrigin != checkpoints[currentCpIndex].origin)
	{
		this->Teleport(&cp.origin, &cp.angles, &NULL_VECTOR);
	}
	else
	{
		this->Teleport(NULL, &cp.angles, &NULL_VECTOR);
	}
	pawn->m_flSlopeDropHeight(cp.slopeDropHeight);
	pawn->m_flSlopeDropOffset(cp.slopeDropOffset);

	pawn->m_hGroundEntity(cp.groundEnt);
	if (cp.onLadder)
	{
		CCSPlayer_MovementServices *ms = this->GetMoveServices();
		ms->m_vecLadderNormal(cp.ladderNormal);
		this->GetPawn()->m_MoveType(MOVETYPE_LADDER);
	}
	this->teleportTime = utils::GetServerGlobals()->curtime;
}

void KZPlayer::TpToCheckpoint()
{
	DoTeleport(this->currentCpIndex);
}

void KZPlayer::TpToPrevCp()
{
	this->currentCpIndex = MAX(0, this->currentCpIndex - 1);
	DoTeleport(this->currentCpIndex);
}

void KZPlayer::TpToNextCp()
{
	this->currentCpIndex = MIN(this->currentCpIndex + 1, this->checkpoints.Count() - 1);
	DoTeleport(this->currentCpIndex);
}

void KZPlayer::TpHoldPlayerStill()
{
	if (!checkpoints.IsValidIndex(currentCpIndex)) return;
	if (this->GetPawn()->m_lifeState() != LIFE_ALIVE) return;
	if (utils::GetServerGlobals()->curtime - this->teleportTime > 0.04) return;
	Vector currentOrigin;
	this->GetOrigin(&currentOrigin);

	// If we teleport the player to this origin every tick, they will end up NOT on this origin in the end somehow.
	if (currentOrigin != checkpoints[currentCpIndex].origin)
	{
		this->SetOrigin(checkpoints[currentCpIndex].origin);
	}
	this->SetVelocity(Vector(0,0,0));
	if (checkpoints[currentCpIndex].onLadder && this->GetPawn()->m_MoveType() != MOVETYPE_NONE)
	{
		this->GetPawn()->m_MoveType(MOVETYPE_LADDER);
	}
	if (checkpoints[currentCpIndex].groundEnt)
	{
		this->GetPawn()->m_fFlags |= FL_ONGROUND;
	}
	this->GetPawn()->m_hGroundEntity(checkpoints[currentCpIndex].groundEnt);
}

void KZPlayer::OnStartProcessMovement()
{
	MovementPlayer::OnStartProcessMovement();
	// Always ensure that the player has at least an ongoing jump.
	// This is mostly to prevent crash, it's not a valid jump.
	if (this->jumps.Count() == 0)
	{
		this->jumps.AddToTail(Jump(this));
		this->jumps.Tail().Invalidate();
	}
	this->TpHoldPlayerStill();
	this->EnableGodMode();
	this->HandleMoveCollision();
}

void KZPlayer::OnStopProcessMovement()
{
	KZ::HUD::DrawSpeedPanel(this);
	this->jumps.Tail().Update();
	MovementPlayer::OnStopProcessMovement();
}

void KZPlayer::ToggleHide()
{
	this->hideOtherPlayers = !this->hideOtherPlayers;
}

void KZPlayer::Reset()
{
	MovementPlayer::Reset();

	this->currentCpIndex = 0;
	this->hideOtherPlayers = false;
	this->holdingStill = false;
	this->teleportTime = 0.0f;
	this->checkpoints.Purge();
}