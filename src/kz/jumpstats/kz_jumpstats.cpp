#include "../kz.h"
#include "kz_jumpstats.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

#define IGNORE_JUMP_TIME 0.2f
#define JS_MAX_LADDERJUMP_OFFSET 2.0f
#define JS_MAX_BHOP_GROUND_TIME 0.05f
#define JS_MAX_DUCKBUG_RESET_TIME 0.05f
#define JS_MAX_WEIRDJUMP_FALL_OFFSET 64.0f

const char *jumpTypeShortStr[JUMPTYPE_COUNT] =
{
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

/*
* AACall stuff
*/

f32 AACall::CalcIdealYaw(bool useRadians)
{
	f64 accelspeed;
	if (this->wishspeed != 0)
	{
		accelspeed = this->accel * this->wishspeed * this->surfaceFriction * this->subtickFraction / 64; // Hardcoding tickrate
	}
	else
	{
		accelspeed = this->accel * this->maxspeed * this->surfaceFriction * this->subtickFraction / 64; // Hardcoding tickrate
	}
	if (accelspeed <= 0.0)
		return useRadians ? M_PI : RAD2DEG(M_PI);

	if (this->velocityPre.Length2D() == 0.0)
		return 0.0;

	const f64 wishspeedcapped = 30; // Hardcoding for now.
	f64 tmp = wishspeedcapped - accelspeed;
	if (tmp <= 0.0)
		return useRadians ? M_PI / 2 : RAD2DEG(M_PI / 2);

	f64 speed = this->velocityPre.Length2D();
	if (tmp < speed)
		return useRadians ? acos(tmp / speed) : RAD2DEG(acos(tmp / speed));

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
		return this->CalcIdealYaw();

	return useRadians ? acos(numer / denom) : RAD2DEG(acos(numer / denom));
}

f32 AACall::CalcAccelSpeed(bool tryMaxSpeed)
{
	if (tryMaxSpeed && this->wishspeed == 0)
	{
		return this->accel * this->maxspeed * this->surfaceFriction * this->subtickFraction / 64;
	}
	return this->accel * this->wishspeed * this->surfaceFriction * this->subtickFraction / 64;
}

f32 AACall::CalcIdealGain()
{
	// sqrt(v^2+a^2+2*v*a*cos(yaw)
	f32 idealSpeed = sqrt(this->velocityPre.Length2DSqr()
		+ MIN(this->CalcAccelSpeed(true), 30) * MIN(this->CalcAccelSpeed(true), 30)
		+ 2 * MIN(this->CalcAccelSpeed(true), 30) * this->velocityPre.Length2D() * cos(this->CalcIdealYaw(true)));
	return idealSpeed - this->velocityPre.Length2D();
}

/*
* Strafe stuff
*/

void Strafe::End()
{
	FOR_EACH_VEC(this->aaCalls, i)
	{
		this->duration += this->aaCalls[i].subtickFraction / 64;
		// Calculate BA/DA/OL
		if (this->aaCalls[i].wishspeed == 0)
		{
			u64 buttonBits = IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT;
			if (utils::IsButtonDown(&this->aaCalls[i].buttons, buttonBits))
			{
				this->overlap += this->aaCalls[i].subtickFraction / 64;
			}
			else
			{
				this->deadAir += this->aaCalls[i].subtickFraction / 64;
			}
		}
		else if ((this->aaCalls[i].velocityPost - this->aaCalls[i].velocityPre).Length2D() <= 0.03125f)
		{
			// This gain could just be from quantized float stuff.
			this->badAngles += this->aaCalls[i].subtickFraction / 64;
		}
		// Calculate sync.
		else if (this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D() > 0.03125f)
		{
			this->syncDuration += this->aaCalls[i].subtickFraction / 64;
		}

		// Gain/loss.
		this->maxGain += this->aaCalls[i].CalcIdealGain();
		f32 speedDiff = this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D();
		if (speedDiff > 0)
		{
			this->gain += speedDiff;
		}
		else
		{
			this->loss += speedDiff;
		}
		f32 externalSpeedDiff = this->aaCalls[i].externalSpeedDiff;
		if (externalSpeedDiff > 0)
		{
			this->externalGain += speedDiff;
		}
		else
		{
			this->externalLoss += speedDiff;
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

		// If no attempt to gain speed was made, use the angle of the last call as a reference, and add yaw relative to last tick's yaw.
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
			(this->turnstate == TURN_NONE && fabs(utils::GetAngleDifference(angles.y, idealYaw, 180.0f)) > fabs(utils::GetAngleDifference(-angles.y, idealYaw, 180.0f))))
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
		
		//utils::PrintConsoleAll("%f %f %f %f | %f / %f / %f | %f -> %f | %f %f | ws %f wd %f %f %f accel %f fraction %f",
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
		//	this->aaCalls[i].subtickFraction);
		if (angles.y > maxYaw + 20.0f || angles.y < minYaw - 20.0f)
		{
			continue;
		}
		f32 gainRatio = (this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D()) / this->aaCalls[i].CalcIdealGain();
		if (angles.y < minYaw)
		{
			totalRatios += -1 * this->aaCalls[i].subtickFraction;
			totalDuration += this->aaCalls[i].subtickFraction;
			ratios.AddToTail(-1 * this->aaCalls[i].subtickFraction);
			//utils::PrintConsoleAll("No Gain: GR = %f (%f / %f), Add %f", gainRatio, this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain(), addRatio);
			continue;
		}
		else if (angles.y < idealYaw)
		{
			totalRatios += (gainRatio - 1) * this->aaCalls[i].subtickFraction;
			totalDuration += this->aaCalls[i].subtickFraction;
			ratios.AddToTail(-1 * this->aaCalls[i].subtickFraction);
			//utils::PrintConsoleAll("Slow Gain: GR = %f (%f / %f), Add %f", gainRatio, this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain(), addRatio);
		}
		else if (angles.y < maxYaw)
		{
			totalRatios = (1 - gainRatio) * this->aaCalls[i].subtickFraction;
			totalDuration += this->aaCalls[i].subtickFraction;
			ratios.AddToTail(-1 * this->aaCalls[i].subtickFraction);
			//utils::PrintConsoleAll("Fast Gain: GR = %f (%f / %f), Add %f", gainRatio, this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain(), addRatio);
		}
		else
		{
			totalRatios = 1.0f;
			totalDuration += this->aaCalls[i].subtickFraction;
			ratios.AddToTail(-1 * this->aaCalls[i].subtickFraction);
			//utils::PrintConsoleAll("TooFast Gain: GR = %f (%f / %f), Add %f", gainRatio, this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain(), addRatio);
		}
	}

	// This can return nan if the duration is 0, this is intended...
	if (totalDuration == 0.0f) return false;
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
	this->jumpType = this->player->jumpstatsService->DetermineJumpType();
}

void Jump::Invalidate()
{
	this->valid = false;
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
	for (int i = 0; i < 3; i++)
	{
		call->buttons.m_pButtonStates[i] = this->player->GetMoveServices()->m_nButtons()->m_pButtonStates[i];
	}
	call->wishdir = wishdir;
	call->wishspeed = wishspeed;
	call->accel = accel;
	call->surfaceFriction = this->player->GetMoveServices()->m_flSurfaceFriction();
	call->subtickFraction = this->player->currentMoveData->m_flSubtickFraction;
	call->ducking = this->player->GetMoveServices()->m_bDucked;
	this->player->GetVelocity(&call->velocityPost);
}

void Jump::Update()
{
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
	FOR_EACH_VEC(this->strafes, i)
	{
		FOR_EACH_VEC(this->strafes[i].aaCalls, j)
		{
			if (this->strafes[i].aaCalls[j].ducking)
			{
				this->duckDuration += this->strafes[i].aaCalls[j].subtickFraction / 64;
				this->duckEndDuration += this->strafes[i].aaCalls[j].subtickFraction / 64;
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

	}
	this->width /= this->strafes.Count();
	this->overlap /= jumpDuration;
	this->deadAir /= jumpDuration;
	this->badAngles /= jumpDuration;
	this->sync /= jumpDuration;
	this->ended = true;
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

f32 Jump::GetDistance() 
{
	// TODO: don't add 32 to lajs
	return (this->adjustedLandingOrigin - this->adjustedTakeoffOrigin).Length2D() + 32.0f;
}

f32 Jump::GetEdge(bool landing) { return 0.0f; } // TODO

f32 Jump::GetAirPath()
{
	if (this->totalDistance <= 0.0f) return 0.0;
	return this->totalDistance / (this->GetDistance() - 32.0f);
}

f32 Jump::GetDeviation()
{
	f32 distanceX = fabs(adjustedLandingOrigin.x - adjustedTakeoffOrigin.x);
	f32 distanceY = fabs(adjustedLandingOrigin.y - adjustedTakeoffOrigin.y);
	if (distanceX > distanceY) return distanceY;
	return distanceX;
}

JumpType KZJumpstatsService::DetermineJumpType()
{
	if (this->player->takeoffFromLadder)
	{
		if (this->player->GetPawn()->m_ignoreLadderJumpTime() > utils::GetServerGlobals()->curtime
			&& this->player->jumpstatsService->lastJumpButtonTime > this->player->GetPawn()->m_ignoreLadderJumpTime() - IGNORE_JUMP_TIME
			&& this->player->jumpstatsService->lastJumpButtonTime < this->player->GetPawn()->m_ignoreLadderJumpTime() + 1/64)
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
	else if (!this->player->jumped)
	{
		return JumpType_Fall;
	}
	else if (this->player->duckBugged)
	{
		if (this->player->jumpstatsService->jumps.Tail().GetOffset() < 0.03125 && this->player->jumpstatsService->jumps.Tail().GetJumpType() == JumpType_LongJump)
		{
			return JumpType_Jumpbug;
		}
		else
		{
			return JumpType_Invalid;
		}
	}
	else if (this->HitBhop() && !this->HitDuckbugRecently())
	{
		// Check for no offset
		if (fabs(this->player->jumpstatsService->jumps.Tail().GetOffset()) < 0.03125)
		{
			switch (this->player->jumpstatsService->jumps.Tail().GetJumpType())
			{
			case JumpType_LongJump:return JumpType_Bhop;
			case JumpType_Bhop:return JumpType_MultiBhop;
			case JumpType_MultiBhop:return JumpType_MultiBhop;
			default:return JumpType_Other;
			}
		}
		// Check for weird jump
		else if (this->player->jumpstatsService->jumps.Tail().GetJumpType() == JumpType_Fall &&
			this->ValidWeirdJumpDropDistance())
		{
			return JumpType_WeirdJump;
		}
		else
		{
			return JumpType_Other;
		}
	}
	if (this->HitDuckbugRecently() || !this->GroundSpeedCappedRecently())
	{
		return JumpType_Invalid;
	}
	return JumpType_LongJump;
}

void KZJumpstatsService::OnStartProcessMovement()
{
	// Always ensure that the player has at least an ongoing jump.
	// This is mostly to prevent crash, it's not a valid jump.
	if (this->jumps.Count() == 0)
	{
		this->AddJump();
		this->InvalidateJumpstats();
	}
	// Invalidate jumpstats if movetype is invalid.
	if (this->player->GetPawn()->m_MoveType() != MOVETYPE_WALK && this->player->GetPawn()->m_MoveType() != MOVETYPE_LADDER )
	{
		this->InvalidateJumpstats();
	}
}

void KZJumpstatsService::OnChangeMoveType(MoveType_t oldMoveType)
{
	if (oldMoveType == MOVETYPE_LADDER && this->player->GetPawn()->m_MoveType() == MOVETYPE_WALK)
	{
		this->AddJump();
	}
}
bool KZJumpstatsService::HitBhop()
{
	return this->player->takeoffTime - this->player->landingTime < JS_MAX_BHOP_GROUND_TIME;
}

bool KZJumpstatsService::HitDuckbugRecently()
{
	return utils::GetServerGlobals()->curtime - this->lastDuckbugTime <= JS_MAX_DUCKBUG_RESET_TIME;
}
bool KZJumpstatsService::ValidWeirdJumpDropDistance()
{
	return this->jumps.Tail().GetOffset() > -1 * JS_MAX_WEIRDJUMP_FALL_OFFSET;
}

bool KZJumpstatsService::GroundSpeedCappedRecently()
{
	return this->lastGroundSpeedCappedTime == this->lastMovementProcessedTime;
}

void KZJumpstatsService::OnAirAcceleratePre()
{
	AACall call;
	this->player->GetVelocity(&call.velocityPre);

	// moveDataPost is still the movedata from last tick.
	call.externalSpeedDiff = call.velocityPre.Length2D() - this->player->moveDataPost.m_vecVelocity.Length2D();
	call.prevYaw = this->player->oldAngles.y;
	call.curtime = utils::GetServerGlobals()->curtime;
	call.tickcount = utils::GetServerGlobals()->tickcount;
	Strafe *strafe = this->jumps.Tail().GetCurrentStrafe();
	strafe->aaCalls.AddToTail(call);
}

void KZJumpstatsService::OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel)
{
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
}
void KZJumpstatsService::EndJump()
{
	if (this->jumps.Count() > 0)
	{
		Jump *jump = &this->jumps.Tail();

		// Prevent stats being calculated twice.
		if (jump->AlreadyEnded()) return;
		jump->End();
		if ((jump->GetOffset() > -0.03125 && jump->IsValid()) || this->jsAlways)
		{
			// TODO: darkblue>green>darkred>gold>orchid
			utils::CPrintChat(this->player->GetPawn(), "{lime}KZ {grey}| {olive}%s: %.1f {grey}| {olive}%i {grey}Strafes | {olive}%2.f%% {grey}Sync | {olive}%.2f {grey}Pre | {olive}%.2f {grey}Max\n\
					{grey}BA {olive}%.0f%% {grey}| OL {olive}%.0f%% {grey}| DA {olive}%.0f%% {grey}| {olive}%.1f {grey}Deviation | {olive}%.1f {grey}Width | {olive}%.2f {grey}Height",
					jumpTypeShortStr[jump->GetJumpType()],
					jump->GetDistance(),
					jump->strafes.Count(),
					jump->GetSync() * 100.0f,
					this->player->takeoffVelocity.Length2D(),
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

void KZJumpstatsService::InvalidateJumpstats()
{
	if (this->jumps.Count() > 0)
	{
		this->jumps.Tail().Invalidate();
	}
}

void KZJumpstatsService::TrackJumpstatsVariables()
{
	this->lastJumpButtonTime = this->player->GetPawn()->m_ignoreLadderJumpTime();
	if (this->player->GetPawn()->m_MoveType == MOVETYPE_NOCLIP)
	{
		this->lastNoclipTime = utils::GetServerGlobals()->curtime;	
	}
	if (this->player->duckBugged)
	{
		this->lastDuckbugTime = utils::GetServerGlobals()->curtime;	
	}
	if (this->player->walkMoved)
	{
		this->lastGroundSpeedCappedTime = utils::GetServerGlobals()->curtime;
	}
	this->lastMovementProcessedTime = utils::GetServerGlobals()->curtime;
}

void KZJumpstatsService::ToggleJSAlways()
{
	this->jsAlways = !this->jsAlways;
	utils::PrintChat(player->GetController(), "[KZ] JSAlways %s.", this->jsAlways ? "enabled" : "disabled");
}