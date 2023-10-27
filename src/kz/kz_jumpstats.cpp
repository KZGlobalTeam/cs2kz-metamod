#include "kz.h"
#include "utils/utils.h"

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

Strafe::Strafe()
{
	this->starttime = utils::GetServerGlobals()->curtime - utils::GetServerGlobals()->frametime;
}

f32 Strafe::CalcAngleRatioAverage()
{
	f32 duration = 0.0f;
	f32 ratio = 0.0f;
	f32 prevYaw;
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
		// If this is the first tick of the strafe then we can try using the velocity's yaw.
		// If the velocity is 0 as well, then every angle is a perfect angle.
		if (this->aaCalls[i].wishspeed != 0)
		{
			VectorAngles(this->aaCalls[i].wishdir, angles);
		}
		else if (i > 0)
		{
			angles.y = prevYaw + utils::GetAngleDifference(this->aaCalls[i].currentYaw, this->aaCalls[i - 1].currentYaw, 180.0f);
		}
		else
		{
			angles = velAngles;
		}

		angles -= velAngles;
		// Get the minimum, ideal, and max yaw for gain.
		f32 minYaw = utils::NormalizeDeg(this->aaCalls[i].CalcMinYaw());
		f32 idealYaw = utils::NormalizeDeg(this->aaCalls[i].CalcIdealYaw());
		f32 maxYaw = utils::NormalizeDeg(this->aaCalls[i].CalcMaxYaw());
		
		angles.y = utils::NormalizeDeg(angles.y);
		prevYaw = angles.y + velAngles.y;

		if (this->turnstate == TURN_RIGHT || /* The ideal angle is calculated for left turns, we need to flip it for right turns. */
			this->turnstate == TURN_NONE && fabs(utils::GetAngleDifference(angles.y, idealYaw, 180.0f)) > fabs(utils::GetAngleDifference(-angles.y, idealYaw, 180.0f)))
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
			ratio += -1 * this->aaCalls[i].subtickFraction;
			duration += this->aaCalls[i].subtickFraction;
			//utils::PrintConsoleAll("No Gain: GR = %f (%f / %f), Add %f", gainRatio, this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain(), addRatio);
			continue;
		}
		else if (angles.y < idealYaw)
		{
			ratio += (gainRatio - 1) * this->aaCalls[i].subtickFraction;
			duration += this->aaCalls[i].subtickFraction;
			//utils::PrintConsoleAll("Slow Gain: GR = %f (%f / %f), Add %f", gainRatio, this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain(), addRatio);
		}
		else if (angles.y < maxYaw)
		{
			ratio = (1 - gainRatio) * this->aaCalls[i].subtickFraction;
			duration += this->aaCalls[i].subtickFraction;
			//utils::PrintConsoleAll("Fast Gain: GR = %f (%f / %f), Add %f", gainRatio, this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain(), addRatio);
		}
		else
		{
			ratio = 1.0f;
			duration += this->aaCalls[i].subtickFraction;
			//utils::PrintConsoleAll("TooFast Gain: GR = %f (%f / %f), Add %f", gainRatio, this->aaCalls[i].velocityPost.Length2D() - this->aaCalls[i].velocityPre.Length2D(), this->aaCalls[i].CalcIdealGain(), addRatio);
		}
	}
	utils::PrintConsoleAll("%f", ratio / duration);
	return ratio / duration;
}

void Jump::Invalidate()
{
	this->validJump = false;
}

void Jump::Update()
{

}

void Jump::End()
{
	this->landingOrigin = this->player->landingOrigin;
	this->adjustedLandingOrigin = this->player->landingOriginActual;
	FOR_EACH_VEC(this->strafes, i)
	{
		this->strafes[i].CalcAngleRatioAverage();
	}
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
		float time = utils::GetServerGlobals()->curtime - utils::GetServerGlobals()->frametime;
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