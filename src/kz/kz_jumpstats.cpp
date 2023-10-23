#include "kz.h"


f32 AACall::CalcIdealYaw(bool useRadians)
{
	double accelspeed = this->accel * this->wishspeed * this->surfaceFriction * this->subtickFraction * 1/64; // Hardcoding tickrate
	if (accelspeed <= 0.0)
		return useRadians ? M_PI : RAD2DEG(M_PI);

	if (this->velocityPre.Length2D() == 0.0)
		return 0.0;

	const double wishspeed_capped = 30; // Hardcoding for now.
	double tmp = wishspeed_capped - accelspeed;
	if (tmp <= 0.0)
		return useRadians ? M_PI / 2 : RAD2DEG(M_PI / 2);

	double speed = this->velocityPre.Length2D();
	if (tmp < speed)
		return useRadians ? std::acos(tmp / speed) : RAD2DEG(std::acos(tmp / speed));

	return 0.0;
}
f32 AACall::GetAngleEfficiency()
{
	return 0.0;
}

void Jump::Invalidate()
{

}

void Jump::Update()
{

}

void Jump::End()
{
	this->landingOrigin = this->player->landingOrigin;
	this->adjustedLandingOrigin = this->player->landingOriginActual;
}

u8 Jump::DetermineJumpType()
{
	return 0; // TODO
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
		Strafe strafe = Strafe();
		strafe.turnstate = this->player->GetTurning();
		this->strafes.AddToTail(strafe);
	}
	// Turn state didn't change, it's the same strafe. No need to do anything.

	return &this->strafes.Tail();
}

f32 Jump::GetDistance() 
{ 
	return (this->adjustedLandingOrigin - this->adjustedTakeoffOrigin).Length2D() + 32.0f;
}

f32 Jump::GetMaxSpeed() { return 0.0f; } // TODO
f32 Jump::GetSync() { return 0.0f; } // TODO
f32 Jump::GetBadAngles() { return 0.0f; } // TODO
f32 Jump::GetOverlap() { return 0.0f; } // TODO
f32 Jump::GetDeadAir() { return 0.0f; } // TODO
f32 Jump::GetMaxHeight() { return 0.0f; } // TODO
f32 Jump::GetWidth() { return 0.0f; } // TODO
f32 Jump::GetEdge(bool landing) { return 0.0f; } // TODO
f32 Jump::GetGain() { return 0.0f; } // TODO
f32 Jump::GetLoss() { return 0.0f; } // TODO
f32 Jump::GetStrafeEfficiency() { return 0.0f; } // TODO
f32 Jump::GetAirPath() { return 0.0f; } // TODO
f32 Jump::GetDuckTime(bool endOnly) { return 0.0f; } // TODO