#include "../kz.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"

#include "kz_jumpstats.h"
#include "../mode/kz_mode.h"
#include "../style/kz_style.h"
#include "../option/kz_option.h"
#include "../language/kz_language.h"

#include "tier0/memdbgon.h"

// clang-format off

const char *jumpTypeStr[JUMPTYPE_COUNT] = {
	"Long Jump",
	"Bunnyhop",
	"Multi Bunnyhop",
	"Weird Jump",
	"Ladder Jump",
	"Ladderhop",
	"Jumpbug",
	"Fall",
	"Unknown Jump",
	"Invalid Jump"
};

const char *jumpTypeShortStr[JUMPTYPE_COUNT] = {
	"LJ",
	"BH",
	"MBH",
	"WJ",
	"LAJ",
	"LAH",
	"JB",
	"FL",
	"UNK",
	"INV"
};

const char *distanceTierColors[DISTANCETIER_COUNT] = {
	"{grey}",
	"{grey}",
	"{blue}",
	"{green}",
	"{darkred}",
	"{gold}",
	"{orchid}"
};

const char *distanceTierSounds[DISTANCETIER_COUNT] = {
	"",
	"",
	"kz.impressive",
	"kz.perfect",
	"kz.godlike",
	"kz.ownage",
	"kz.wrecker"
};

// clang-format on

/*
 * AACall stuff
 */

f32 AACall::CalcIdealYaw(bool useRadians)
{
	f64 accelspeed;
	if (this->wishspeed != 0)
	{
		accelspeed = this->accel * this->wishspeed * this->surfaceFriction * this->duration;
	}
	else
	{
		accelspeed = this->accel * this->maxspeed * this->surfaceFriction * this->duration;
	}
	if (accelspeed <= 0.0)
	{
		return useRadians ? M_PI : RAD2DEG(M_PI);
	}

	if (this->velocityPre.Length2D() == 0.0)
	{
		return 0.0;
	}

	const f64 wishspeedcapped = 30; // Hardcoding for now.
	f64 tmp = wishspeedcapped - accelspeed;
	if (tmp <= 0.0)
	{
		return useRadians ? M_PI / 2 : RAD2DEG(M_PI / 2);
	}

	f64 speed = this->velocityPre.Length2D();
	if (tmp < speed)
	{
		return useRadians ? acos(tmp / speed) : RAD2DEG(acos(tmp / speed));
	}

	return 0.0;
}

f32 AACall::CalcMinYaw(bool useRadians)
{
	// Hardcoding max wishspeed. If your velocity is lower than 30, any direction will get you gain.
	const f64 wishspeedcapped = 30;
	if (this->velocityPre.Length2D() <= wishspeedcapped)
	{
		return 0.0;
	}
	return useRadians ? acos(30.0 / this->velocityPre.Length2D()) : RAD2DEG(acos(30.0 / this->velocityPre.Length2D()));
}

f32 AACall::CalcMaxYaw(bool useRadians)
{
	f32 gamma1, numer, denom;
	gamma1 = AACall::CalcAccelSpeed(true);
	f32 speed = this->velocityPre.Length2D();
	if (gamma1 <= 60)
	{
		numer = -gamma1;
		denom = 2 * speed;
	}
	else
	{
		numer = -30;
		denom = speed;
	}
	if (denom < fabs(numer))
	{
		return this->CalcIdealYaw();
	}

	return useRadians ? acos(numer / denom) : RAD2DEG(acos(numer / denom));
}

f32 AACall::CalcAccelSpeed(bool tryMaxSpeed)
{
	if (tryMaxSpeed && this->wishspeed == 0)
	{
		return this->accel * this->maxspeed * this->surfaceFriction * this->duration;
	}
	return this->accel * this->wishspeed * this->surfaceFriction * this->duration;
}

f32 AACall::CalcIdealGain()
{
	// sqrt(v^2+a^2+2*v*a*cos(yaw)
	// clang-format off
	f32 idealSpeed = sqrt(this->velocityPre.Length2DSqr()
		+ MIN(this->CalcAccelSpeed(true), 30)
		* MIN(this->CalcAccelSpeed(true), 30)
		+ 2
		* MIN(this->CalcAccelSpeed(true), 30)
		* this->velocityPre.Length2D()
		* cos(this->CalcIdealYaw(true))
	);
	// clang-format on

	return idealSpeed - this->velocityPre.Length2D();
}

/*
 * Strafe stuff
 */

void Strafe::UpdateCollisionVelocityChange(f32 delta)
{
	if (delta < 0.0f)
	{
		this->externalLoss -= delta;
	}
	else
	{
		this->externalGain += delta;
	}
}

void Strafe::End()
{
	FOR_EACH_VEC(this->aaCalls, i)
	{
		this->duration += this->aaCalls[i].duration;
		// Calculate BA/DA/OL
		if (this->aaCalls[i].wishspeed == 0)
		{
			u64 buttonBits = IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT;
			if (CInButtonState::IsButtonPressed(this->aaCalls[i].buttons, buttonBits))
			{
				this->overlap += this->aaCalls[i].duration;
			}
			else
			{
				this->deadAir += this->aaCalls[i].duration;
			}
		}
		else if ((this->aaCalls[i].velocityPost - this->aaCalls[i].velocityPre).Length2D() <= JS_EPSILON)
		{
			// This gain could just be from quantized float stuff.
			this->badAngles += this->aaCalls[i].duration;
		}
		// Calculate sync.
		else if (this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D() > JS_EPSILON)
		{
			this->syncDuration += this->aaCalls[i].duration;
		}

		// Gain/loss.
		this->maxGain += this->aaCalls[i].CalcIdealGain();
		f32 speedDiff = this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D();
		if (speedDiff > 0)
		{
			this->airGain += speedDiff;
		}
		else
		{
			this->airLoss += speedDiff;
		}
		f32 externalSpeedDiff = this->aaCalls[i].externalSpeedDiff;
		if (externalSpeedDiff > 0)
		{
			this->externalGain += externalSpeedDiff;
		}
		else
		{
			this->externalLoss += externalSpeedDiff;
		}
		this->width += fabs(utils::GetAngleDifference(this->aaCalls[i].currentYaw, this->aaCalls[i].prevYaw, 180.0f));
	}
	this->CalcAngleRatioStats();
}

bool Strafe::CalcAngleRatioStats()
{
	this->arStats.available = false;
	f32 totalDuration = 0.0f;
	f32 totalRatios = 0.0f;
	CUtlVector<f32> ratios;

	QAngle angles, velAngles;
	FOR_EACH_VEC(this->aaCalls, i)
	{
		if (this->aaCalls[i].velocityPre.Length2D() == 0)
		{
			// Any angle should be a good angle here.
			// ratio += 0;
			continue;
		}
		VectorAngles(this->aaCalls[i].velocityPre, velAngles);

		// If no attempt to gain speed was made, use the angle of the last call as a reference,
		// and add yaw relative to last tick's yaw.
		// If the velocity is 0 as well, then every angle is a perfect angle.
		if (this->aaCalls[i].wishspeed != 0)
		{
			VectorAngles(this->aaCalls[i].wishdir, angles);
		}
		else
		{
			angles.y = this->aaCalls[i].prevYaw + utils::GetAngleDifference(this->aaCalls[i].currentYaw, this->aaCalls[i].prevYaw, 180.0f);
		}

		angles -= velAngles;
		// Get the minimum, ideal, and max yaw for gain.
		f32 minYaw = utils::NormalizeDeg(this->aaCalls[i].CalcMinYaw());
		f32 idealYaw = utils::NormalizeDeg(this->aaCalls[i].CalcIdealYaw());
		f32 maxYaw = utils::NormalizeDeg(this->aaCalls[i].CalcMaxYaw());

		angles.y = utils::NormalizeDeg(angles.y);

		if (this->turnstate == TURN_RIGHT || /* The ideal angle is calculated for left turns, we need to flip it for right turns. */
			(this->turnstate == TURN_NONE
			 && fabs(utils::GetAngleDifference(angles.y, idealYaw, 180.0f)) > fabs(utils::GetAngleDifference(-angles.y, idealYaw, 180.0f))))
		// If we aren't turning at all, take the one closer to the ideal yaw.
		{
			angles.y = -angles.y;
		}

		// It is possible for the player to gain speed here, by pressing the opposite keys
		// while still turning in the same direction, which results in actual gain...
		// Usually this happens at the end of a strafe.
		if (angles.y < 0 && this->aaCalls[i].velocityPost.Length2D() > this->aaCalls[i].velocityPre.Length2D())
		{
			angles.y = -angles.y;
		}

		// If the player yaw is way too off, they are probably pressing the wrong key and probably not turning too fast.
		// So we shouldn't count them into the average calc.

		//	utils::PrintConsoleAll("%f %f %f %f | %f / %f / %f | %f -> %f | %f %f | ws %f wd %f %f %f accel %f fraction %f",
		//	minYaw, angles.y, idealYaw, maxYaw,
		//	utils::GetAngleDifference(angles.y, minYaw, 180.0),
		//	utils::GetAngleDifference(idealYaw, minYaw, 180.0),
		//	utils::GetAngleDifference(maxYaw, minYaw, 180.0),
		//	this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].velocityPost.Length2D(),
		//	this->aaCalls[i].velocityPre.x, this->aaCalls[i].velocityPre.y,
		//	this->aaCalls[i].wishspeed,
		//	this->aaCalls[i].wishdir.x,
		//	this->aaCalls[i].wishdir.y,
		//	this->aaCalls[i].wishdir.z,
		//	this->aaCalls[i].accel,
		//	this->aaCalls[i].duration * ENGINE_FIXED_TICK_RATE);
		if (angles.y > maxYaw + 20.0f || angles.y < minYaw - 20.0f)
		{
		}
		f32 gainRatio = (this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D()) / this->aaCalls[i].CalcIdealGain();
		f32 fraction = this->aaCalls[i].duration * ENGINE_FIXED_TICK_RATE;
		if (angles.y < minYaw)
		{
			totalRatios += -1 * fraction;
			totalDuration += fraction;
			ratios.AddToTail(-1 * fraction);
			// utils::PrintConsoleAll("No Gain: GR = %f (%f / %f)", gainRatio, this->aaCalls[i].velocityPost.Length2D()
			// - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain());
			continue;
		}
		else if (angles.y < idealYaw)
		{
			totalRatios += (gainRatio - 1) * fraction;
			totalDuration += fraction;
			ratios.AddToTail((gainRatio - 1) * fraction);
			// utils::PrintConsoleAll("Slow Gain: GR = %f (%f / %f)", gainRatio,
			// this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(),
			// this->aaCalls[i].CalcIdealGain());
		}
		else if (angles.y < maxYaw)
		{
			totalRatios += (1 - gainRatio) * fraction;
			totalDuration += fraction;
			ratios.AddToTail((1 - gainRatio) * fraction);
			// utils::PrintConsoleAll("Fast Gain: GR = %f (%f / %f)", gainRatio,
			// this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(),
			// this->aaCalls[i].CalcIdealGain());
		}
		else
		{
			totalRatios += 1.0f;
			totalDuration += fraction;
			ratios.AddToTail(1.0f);
			// utils::PrintConsoleAll("TooFast Gain: GR = %f (%f / %f)", gainRatio,
			// this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(),
			// this->aaCalls[i].CalcIdealGain());
		}
	}

	// This can return nan if the duration is 0, this is intended...
	if (totalDuration == 0.0f)
	{
		return false;
	}
	ratios.Sort(this->SortFloat);
	this->arStats.available = true;
	this->arStats.average = totalRatios / totalDuration;
	this->arStats.median = ratios[ratios.Count() / 2];
	this->arStats.max = ratios[ratios.Count() - 1];
	return true;
}

/*
 * Jump stuff
 */

void Jump::Init()
{
	this->takeoffOrigin = this->player->takeoffOrigin;
	this->adjustedTakeoffOrigin = this->player->takeoffGroundOrigin;
	this->takeoffVelocity = this->player->takeoffVelocity;
	this->jumpType = this->player->jumpstatsService->DetermineJumpType();
}

void Jump::UpdateAACallPost(Vector wishdir, f32 wishspeed, f32 accel)
{
	// Use the latest parameters, just in case they changed.
	Strafe *strafe = this->GetCurrentStrafe();
	AACall *call = &strafe->aaCalls.Tail();
	QAngle currentAngle;
	this->player->GetAngles(&currentAngle);
	call->maxspeed = this->player->currentMoveData->m_flMaxSpeed;
	call->currentYaw = currentAngle.y;
	this->player->GetMoveServices()->m_nButtons()->GetButtons(call->buttons);
	call->wishdir = wishdir;
	call->wishspeed = wishspeed;
	call->accel = accel;
	call->surfaceFriction = this->player->GetMoveServices()->m_flSurfaceFriction();
	call->duration = g_pKZUtils->GetGlobals()->frametime;
	call->ducking = this->player->GetMoveServices()->m_bDucked;
	this->player->GetVelocity(&call->velocityPost);
	strafe->UpdateStrafeMaxSpeed(call->velocityPost.Length2D());
}

void Jump::Update()
{
	if (this->AlreadyEnded())
	{
		return;
	}
	this->totalDistance += (this->player->currentMoveData->m_vecAbsOrigin - this->player->moveDataPre.m_vecAbsOrigin).Length2D();
	this->currentMaxSpeed = MAX(this->player->currentMoveData->m_vecVelocity.Length2D(), this->currentMaxSpeed);
	this->currentMaxHeight = MAX(this->player->currentMoveData->m_vecAbsOrigin.z, this->currentMaxHeight);
}

void Jump::End()
{
	this->Update();
	if (this->strafes.Count() > 0)
	{
		this->strafes.Tail().End();
	}
	this->landingOrigin = this->player->landingOrigin;
	this->adjustedLandingOrigin = this->player->landingOriginActual;
	this->currentMaxHeight -= this->adjustedTakeoffOrigin.z;
	// This is not the real jump duration, it's just here to calculate sync.
	f32 jumpDuration = 0.0f;

	f32 gain = 0.0f;
	f32 maxGain = 0.0f;
	FOR_EACH_VEC(this->strafes, i)
	{
		FOR_EACH_VEC(this->strafes[i].aaCalls, j)
		{
			if (this->strafes[i].aaCalls[j].ducking)
			{
				this->duckDuration += this->strafes[i].aaCalls[j].duration;
				this->duckEndDuration += this->strafes[i].aaCalls[j].duration;
			}
			else
			{
				this->duckEndDuration = 0.0f;
			}
		}
		this->width += this->strafes[i].GetWidth();
		this->overlap += this->strafes[i].GetOverlapDuration();
		this->deadAir += this->strafes[i].GetDeadAirDuration();
		this->badAngles += this->strafes[i].GetBadAngleDuration();
		this->sync += this->strafes[i].GetSyncDuration();
		jumpDuration += this->strafes[i].GetStrafeDuration();
		gain += this->strafes[i].GetGain();
		maxGain += this->strafes[i].GetMaxGain();
	}
	this->width /= this->strafes.Count();
	this->overlap /= jumpDuration;
	this->deadAir /= jumpDuration;
	this->badAngles /= jumpDuration;
	this->sync /= jumpDuration;
	this->ended = true;
	this->gainEff = gain / maxGain;
	// If there's no air time at all then that was definitely not a jump.
	// Happens when player touch the ground from a ladder.
	if (jumpDuration == 0.0f)
	{
		this->jumpType = JumpType_FullInvalid;
	}
	else
	{
		// Make sure the airtime is valid.
		switch (this->jumpType)
		{
			case JumpType_LadderJump:
			{
				if (jumpDuration > 1.04)
				{
					this->jumpType = JumpType_Invalid;
				}
				break;
			}
			case JumpType_LongJump:
			case JumpType_Bhop:
			case JumpType_MultiBhop:
			case JumpType_WeirdJump:
			case JumpType_Ladderhop:
			case JumpType_Jumpbug:
			{
				if (jumpDuration > 0.8)
				{
					this->jumpType = JumpType_Invalid;
				}
				break;
			}
		}
	}
}

Strafe *Jump::GetCurrentStrafe()
{
	// Always start with 1 strafe.
	if (this->strafes.Count() == 0)
	{
		Strafe strafe = Strafe();
		strafe.turnstate = this->player->GetTurning();
		this->strafes.AddToTail(strafe);
	}
	// If the player isn't turning, update the turn state until it changes.
	else if (!this->strafes.Tail().turnstate)
	{
		this->strafes.Tail().turnstate = this->player->GetTurning();
	}
	// Otherwise, if the strafe is in opposite direction, we add a new strafe.
	else if (this->strafes.Tail().turnstate == -this->player->GetTurning())
	{
		this->strafes.Tail().End();
		// Finish the previous strafe before adding a new strafe.
		Strafe strafe = Strafe();
		strafe.turnstate = this->player->GetTurning();
		this->strafes.AddToTail(strafe);
	}
	// Turn state didn't change, it's the same strafe. No need to do anything.

	return &this->strafes.Tail();
}

f32 Jump::GetDistance(bool useDistbugFix, bool disableAddDist, i32 floorLevel)
{
	f32 dist = 32.0f;
	if (this->jumpType == JumpType_LadderJump || disableAddDist)
	{
		dist = 0.0f;
	}
	if (useDistbugFix)
	{
		dist += (this->adjustedLandingOrigin - this->adjustedTakeoffOrigin).Length2D();
	}
	else
	{
		dist += (this->landingOrigin - this->takeoffOrigin).Length2D();
	}
	return floorLevel < 0 ? dist : floor(dist * pow(10, floorLevel)) / pow(10, floorLevel);
}

// TODO
f32 Jump::GetEdge(bool landing)
{
	return 0.0f;
}

f32 Jump::GetAirPath()
{
	if (this->totalDistance <= 0.0f)
	{
		return 0.0;
	}
	return this->totalDistance / this->GetDistance(false, true);
}

f32 Jump::GetDeviation()
{
	f32 distanceX = fabs(adjustedLandingOrigin.x - adjustedTakeoffOrigin.x);
	f32 distanceY = fabs(adjustedLandingOrigin.y - adjustedTakeoffOrigin.y);
	if (distanceX > distanceY)
	{
		return distanceY;
	}
	return distanceX;
}

JumpType KZJumpstatsService::DetermineJumpType()
{
	if (this->jumps.Count() <= 0 || this->player->JustTeleported())
	{
		return JumpType_Invalid;
	}
	if (this->player->takeoffFromLadder)
	{
		f32 ignoreLadderJumpTime = this->player->GetPlayerPawn()->m_ignoreLadderJumpTime();

		bool ignoringLadder = ignoreLadderJumpTime > g_pKZUtils->GetGlobals()->curtime - ENGINE_FIXED_TICK_INTERVAL;
		bool holdingJumpDuringIgnoreLadderPeriod =
			this->player->jumpstatsService->lastJumpButtonTime > ignoreLadderJumpTime - IGNORE_JUMP_TIME
			&& this->player->jumpstatsService->lastJumpButtonTime < ignoreLadderJumpTime + ENGINE_FIXED_TICK_INTERVAL;

		if (ignoringLadder && holdingJumpDuringIgnoreLadderPeriod)
		{
			return JumpType_Invalid;
		}
		if (this->player->jumped)
		{
			return JumpType_Ladderhop;
		}
		else
		{
			return JumpType_LadderJump;
		}
	}
	if (!this->player->jumped)
	{
		return JumpType_Fall;
	}
	if (this->player->duckBugged)
	{
		if (this->jumps.Tail().GetOffset() < JS_EPSILON && this->jumps.Tail().GetJumpType() == JumpType_LongJump)
		{
			return JumpType_Jumpbug;
		}
		else
		{
			return JumpType_Invalid;
		}
	}
	if (this->HitBhop() && !this->HitDuckbugRecently())
	{
		// Check for no offset
		if (this->jumps.Tail().DidHitHead() || !this->jumps.Tail().IsValid())
		{
			return JumpType_Invalid;
		}
		if (fabs(this->jumps.Tail().GetOffset()) < JS_EPSILON)
		{
			switch (this->jumps.Tail().GetJumpType())
			{
				case JumpType_LongJump:
					return JumpType_Bhop;
				case JumpType_Bhop:
					return JumpType_MultiBhop;
				case JumpType_MultiBhop:
					return JumpType_MultiBhop;
				default:
					return JumpType_Other;
			}
		}
		// Check for weird jump
		if (this->jumps.Tail().GetJumpType() == JumpType_Fall && this->ValidWeirdJumpDropDistance())
		{
			return JumpType_WeirdJump;
		}

		return JumpType_Other;
	}
	if (this->HitDuckbugRecently() || !this->GroundSpeedCappedRecently())
	{
		return JumpType_Invalid;
	}
	return JumpType_LongJump;
}

void KZJumpstatsService::Reset()
{
	this->broadcastMinTier = static_cast<DistanceTier>(KZOptionService::GetOptionInt("defaultJSBroadcastMinTier", DistanceTier_Godlike));
	this->soundMinTier = static_cast<DistanceTier>(KZOptionService::GetOptionInt("defaultJSSoundMinTier", DistanceTier_Godlike));
	this->showJumpstats = true;
	this->jumps.Purge();
	this->jsAlways = {};
	this->lastJumpButtonTime = {};
	this->lastNoclipTime = {};
	this->lastDuckbugTime = {};
	this->lastGroundSpeedCappedTime = {};
	this->lastMovementProcessedTime = {};
	this->tpmVelocity = Vector(0, 0, 0);
	this->possibleEdgebug = {};
}

void KZJumpstatsService::OnProcessMovement()
{
	// Always ensure that the player has at least an ongoing jump.
	// This is mostly to prevent crash, it's not a valid jump.
	if (this->jumps.Count() == 0)
	{
		this->AddJump();
		this->InvalidateJumpstats("First jump");
		return;
	}
	this->CheckValidMoveType();
	this->DetectExternalModifications();
}

void KZJumpstatsService::OnChangeMoveType(MoveType_t oldMoveType)
{
	if (oldMoveType == MOVETYPE_LADDER && this->player->GetPlayerPawn()->m_MoveType() == MOVETYPE_WALK)
	{
		this->AddJump();
	}
	else if (oldMoveType == MOVETYPE_WALK && this->player->GetPlayerPawn()->m_MoveType() == MOVETYPE_LADDER)
	{
		// Not really a valid jump for jumpstats purposes.
		this->InvalidateJumpstats("Invalid movetype change");
		this->EndJump();
	}
}

bool KZJumpstatsService::HitBhop()
{
	return this->player->takeoffTime - this->player->landingTime < JS_MAX_BHOP_GROUND_TIME;
}

bool KZJumpstatsService::HitDuckbugRecently()
{
	return g_pKZUtils->GetGlobals()->curtime - this->lastDuckbugTime <= JS_MAX_DUCKBUG_RESET_TIME;
}

bool KZJumpstatsService::ValidWeirdJumpDropDistance()
{
	return this->jumps.Tail().GetOffset() > -1 * JS_MAX_WEIRDJUMP_FALL_OFFSET;
}

bool KZJumpstatsService::GroundSpeedCappedRecently()
{
	return this->lastGroundSpeedCappedTime == this->lastMovementProcessedTime;
}

void KZJumpstatsService::OnAirAccelerate()
{
	if (g_pKZUtils->GetGlobals()->frametime == 0.0f)
	{
		return;
	}
	AACall call;
	this->player->GetVelocity(&call.velocityPre);

	// moveDataPost is still the movedata from last tick.
	call.externalSpeedDiff = call.velocityPre.Length2D() - this->player->moveDataPost.m_vecVelocity.Length2D();
	call.prevYaw = this->player->oldAngles.y;
	call.curtime = g_pKZUtils->GetGlobals()->curtime;
	call.tickcount = g_pKZUtils->GetGlobals()->tickcount;
	Strafe *strafe = this->jumps.Tail().GetCurrentStrafe();
	strafe->aaCalls.AddToTail(call);
}

void KZJumpstatsService::OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel)
{
	if (g_pKZUtils->GetGlobals()->frametime == 0.0f)
	{
		return;
	}
	this->jumps.Tail().UpdateAACallPost(wishdir, wishspeed, accel);
}

void KZJumpstatsService::AddJump()
{
	this->jumps.AddToTail(Jump(this->player));
}

void KZJumpstatsService::UpdateJump()
{
	if (this->jumps.Count() > 0)
	{
		this->jumps.Tail().Update();
	}
	this->DetectInvalidCollisions();
	this->DetectInvalidGains();
	this->DetectNoclip();
}

void KZJumpstatsService::EndJump()
{
	if (this->jumps.Count() > 0)
	{
		Jump *jump = &this->jumps.Tail();

		// Prevent stats being calculated twice.
		if (jump->AlreadyEnded())
		{
			return;
		}
		jump->End();
		if (jump->GetJumpType() == JumpType_FullInvalid)
		{
			return;
		}
		if ((jump->GetOffset() > -JS_EPSILON && jump->IsValid()) || this->jsAlways)
		{
			if (this->ShouldDisplayJumpstats())
			{
				KZJumpstatsService::PrintJumpToChat(this->player, jump);
			}
			KZJumpstatsService::BroadcastJumpToChat(jump);
			KZJumpstatsService::PlayJumpstatSound(this->player, jump);
			KZJumpstatsService::PrintJumpToConsole(this->player, jump);
		}
	}
}

void KZJumpstatsService::InvalidateJumpstats(const char *reason)
{
	if (this->jumps.Count() > 0 && !this->jumps.Tail().AlreadyEnded())
	{
		this->jumps.Tail().Invalidate(reason);
	}
}

void KZJumpstatsService::TrackJumpstatsVariables()
{
	if (this->player->IsButtonPressed(IN_JUMP))
	{
		this->lastJumpButtonTime = g_pKZUtils->GetGlobals()->curtime;
	}
	if (this->player->GetPlayerPawn()->m_MoveType == MOVETYPE_NOCLIP || this->player->GetPlayerPawn()->m_nActualMoveType == MOVETYPE_NOCLIP)
	{
		this->lastNoclipTime = g_pKZUtils->GetGlobals()->curtime;
	}
	if (this->player->duckBugged)
	{
		this->lastDuckbugTime = g_pKZUtils->GetGlobals()->curtime;
	}
	if (this->player->walkMoved)
	{
		this->lastGroundSpeedCappedTime = g_pKZUtils->GetGlobals()->curtime;
	}
	this->lastMovementProcessedTime = g_pKZUtils->GetGlobals()->curtime;
}

void KZJumpstatsService::ToggleJSAlways()
{
	this->jsAlways = !this->jsAlways;
	if (this->jsAlways)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Always - Enable");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Always - Disable");
	}
}

void KZJumpstatsService::ToggleJumpstatsReporting()
{
	this->showJumpstats = !this->showJumpstats;
	if (this->showJumpstats)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Master Switch - Enable");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Master Switch - Disable");
	}
}

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
}

void KZJumpstatsService::OnTryPlayerMove()
{
	this->tpmVelocity = this->player->currentMoveData->m_vecVelocity;
}

void KZJumpstatsService::OnTryPlayerMovePost()
{
	if (this->jumps.Count() == 0 || this->jumps.Tail().strafes.Count() == 0)
	{
		return;
	}
	f32 velocity = this->player->currentMoveData->m_vecVelocity.Length2D() - this->tpmVelocity.Length2D();
	this->jumps.Tail().strafes.Tail().UpdateCollisionVelocityChange(velocity);
	this->DetectEdgebug();
}

void KZJumpstatsService::OnProcessMovementPost()
{
	if (this->possibleEdgebug && !(this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND))
	{
		this->InvalidateJumpstats("Edgebugged");
	}
	this->possibleEdgebug = false;
	this->TrackJumpstatsVariables();
}

DistanceTier KZJumpstatsService::GetDistTierFromString(const char *tierString)
{
	if (V_stricmp("Meh", tierString) == 0)
	{
		return DistanceTier_Meh;
	}
	if (V_stricmp("Impressive", tierString) == 0)
	{
		return DistanceTier_Impressive;
	}
	if (V_stricmp("Perfect", tierString) == 0)
	{
		return DistanceTier_Perfect;
	}
	if (V_stricmp("Godlike", tierString) == 0)
	{
		return DistanceTier_Godlike;
	}
	if (V_stricmp("Ownage", tierString) == 0)
	{
		return DistanceTier_Ownage;
	}
	if (V_stricmp("Wrecker", tierString) == 0)
	{
		return DistanceTier_Wrecker;
	}
	return DistanceTier_None;
}

void KZJumpstatsService::SetBroadcastMinTier(const char *tierString)
{
	if (!tierString || !V_stricmp("", tierString))
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Broadcast Tier - Hint");
		return;
	}

	DistanceTier tier = GetDistTierFromString(tierString);

	if (tier == DistanceTier_None)
	{
		tier = static_cast<DistanceTier>(V_StringToInt32(tierString, -1));
	}

	if (tier > DistanceTier_Wrecker || tier < DistanceTier_None)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Broadcast Tier - Hint");
		return;
	}

	if (tier == this->GetBroadcastMinTier())
	{
		return;
	}

	this->broadcastMinTier = tier;
	if (tier == 0)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Broadcast Tier - Disabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Broadcast Tier - Response", tierString);
	}
}

void KZJumpstatsService::SetSoundMinTier(const char *tierString)
{
	if (!tierString || !V_stricmp("", tierString))
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Sound Tier - Hint");
		return;
	}

	DistanceTier tier = GetDistTierFromString(tierString);

	if (tier == DistanceTier_None)
	{
		tier = static_cast<DistanceTier>(V_StringToInt32(tierString, -1));
	}

	if (tier > DistanceTier_Wrecker || tier < DistanceTier_None)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Sound Tier - Hint");
		return;
	}

	if (tier == this->GetSoundMinTier())
	{
		return;
	}

	this->soundMinTier = tier;
	if (tier == 0)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Sound Tier - Disabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Sound Tier - Response", tierString);
	}
}

static_function SCMD_CALLBACK(Command_KzToggleJumpstats)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->ToggleJumpstatsReporting();
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzJSAlways)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->ToggleJSAlways();
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzJsPrintMinTier)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->SetBroadcastMinTier(args->Arg(1));
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzJsSoundMinTier)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->SetSoundMinTier(args->Arg(1));
	return MRES_SUPERCEDE;
}

void KZJumpstatsService::RegisterCommands()
{
	scmd::RegisterCmd("kz_jsbroadcast", Command_KzJsPrintMinTier);
	scmd::RegisterCmd("kz_jssound", Command_KzJsSoundMinTier);
	scmd::RegisterCmd("kz_togglestats", Command_KzToggleJumpstats);
	scmd::RegisterCmd("kz_togglejs", Command_KzToggleJumpstats);
	scmd::RegisterCmd("kz_jsalways", Command_KzJSAlways);
}
