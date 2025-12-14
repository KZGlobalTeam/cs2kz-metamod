#include "kz_jumpstats.h"

void KZJumpstatsService::CheckValidMoveType()
{
	// Invalidate jumpstats if movetype is invalid.
	if (this->player->GetPlayerPawn()->m_MoveType() != MOVETYPE_WALK && this->player->GetPlayerPawn()->m_MoveType() != MOVETYPE_LADDER)
	{
		this->InvalidateJumpstats("Invalid movetype");
	}
}

void KZJumpstatsService::DetectNoclip()
{
	if (this->lastNoclipTime + JS_MAX_NOCLIP_RESET_TIME > g_pKZUtils->GetGlobals()->curtime)
	{
		this->InvalidateJumpstats("Just noclipped");
	}
}

void KZJumpstatsService::DetectEdgebug()
{
	if (this->jumps.Count() == 0 || !this->jumps.Tail().IsValid())
	{
		return;
	}
	// If the player suddenly gain speed from negative speed, they probably edgebugged.
	this->possibleEdgebug = false;
	if (this->tpmVelocity.z < 0.0f && this->player->currentMoveData->m_vecVelocity.z > this->tpmVelocity.z
		&& this->player->currentMoveData->m_vecVelocity.z > -JS_EPSILON)
	{
		this->possibleEdgebug = true;
	}
}

void KZJumpstatsService::DetectInvalidCollisions()
{
	if (this->jumps.Count() == 0 || !this->jumps.Tail().IsValid())
	{
		return;
	}
	if (this->player->IsCollidingWithWorld())
	{
		this->jumps.Tail().touchDuration += g_pKZUtils->GetGlobals()->frametime;
		// Headhit invadidates following bhops but not the current jump,
		// while other collisions do after a certain duration.
		if (this->jumps.Tail().touchDuration > JS_TOUCH_GRACE_PERIOD)
		{
			this->InvalidateJumpstats("Invalid collisions");
		}
		if (this->player->moveDataPre.m_vecVelocity.z > 0.0f)
		{
			this->jumps.Tail().MarkHitHead();
		}
	}
}

void KZJumpstatsService::DetectInvalidGains()
{
	/*
	 * Ported from GOKZ: Fix certain props that don't give you base velocity
	 * We check for speed reduction for abuse; while prop abuses increase speed,
	 * wall collision will very likely (if not always) result in a speed reduction.
	 */

	// clang-format off

	f32 speed = this->player->currentMoveData->m_vecVelocity.Length2D();
	f32 actualSpeed = (this->player->currentMoveData->m_vecAbsOrigin - this->player->moveDataPre.m_vecAbsOrigin).Length2D();

	if (this->player->GetPlayerPawn()->m_vecBaseVelocity().Length() > 0.0f || this->player->GetPlayerPawn()->m_fFlags() & FL_BASEVELOCITY)
	{
		this->InvalidateJumpstats("Base velocity detected");
	}

	// clang-format on

	if (actualSpeed - speed > JS_SPEED_MODIFICATION_TOLERANCE && actualSpeed > JS_EPSILON)
	{
		this->InvalidateJumpstats("Invalid gains");
	}
}

void KZJumpstatsService::DetectExternalModifications()
{
	if ((this->player->currentMoveData->m_vecAbsOrigin - this->player->moveDataPost.m_vecAbsOrigin).LengthSqr() > JS_TELEPORT_DISTANCE_SQUARED)
	{
		this->InvalidateJumpstats("Externally modified");
	}
	if (this->player->GetPlayerPawn()->m_vecBaseVelocity().Length() > 0.0f || this->player->GetPlayerPawn()->m_fFlags() & FL_BASEVELOCITY)
	{
		this->InvalidateJumpstats("Base velocity detected");
	}
	if (this->player->GetPlayerPawn()->m_flGravityScale() != 1 || this->player->GetPlayerPawn()->m_flActualGravityScale() != 1)
	{
		this->InvalidateJumpstats("Player gravity scale changed");
	}
}

void KZJumpstatsService::DetectWater()
{
	if (this->jumps.Count() == 0 || !this->jumps.Tail().IsValid())
	{
		return;
	}
	if (player->GetPlayerPawn()->m_flWaterLevel() > 0.0f)
	{
		this->InvalidateJumpstats("Touched water");
	}
}
