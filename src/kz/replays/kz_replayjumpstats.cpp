#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/mode/kz_mode.h"
#include "kz/language/kz_language.h"
#include "kz/spec/kz_spec.h"
#include "kz_replay.h"

void RpJumpStats::FromJump(RpJumpStats &stats, Jump *jump)
{
	// Overall stats
	stats.overall.serverTick = jump->serverTick;
	if (jump->IsValid() && jump->player->styleServices.Count() == 0)
	{
		stats.overall.distanceTier = jump->player->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	}
	stats.overall.takeoffOrigin[0] = jump->takeoffOrigin.x;
	stats.overall.takeoffOrigin[1] = jump->takeoffOrigin.y;
	stats.overall.takeoffOrigin[2] = jump->takeoffOrigin.z;
	stats.overall.adjustedTakeoffOrigin[0] = jump->adjustedTakeoffOrigin.x;
	stats.overall.adjustedTakeoffOrigin[1] = jump->adjustedTakeoffOrigin.y;
	stats.overall.adjustedTakeoffOrigin[2] = jump->adjustedTakeoffOrigin.z;
	stats.overall.takeoffVelocity[0] = jump->takeoffVelocity.x;
	stats.overall.takeoffVelocity[1] = jump->takeoffVelocity.y;
	stats.overall.takeoffVelocity[2] = jump->takeoffVelocity.z;
	stats.overall.landingOrigin[0] = jump->landingOrigin.x;
	stats.overall.landingOrigin[1] = jump->landingOrigin.y;
	stats.overall.landingOrigin[2] = jump->landingOrigin.z;
	stats.overall.adjustedLandingOrigin[0] = jump->adjustedLandingOrigin.x;
	stats.overall.adjustedLandingOrigin[1] = jump->adjustedLandingOrigin.y;
	stats.overall.adjustedLandingOrigin[2] = jump->adjustedLandingOrigin.z;
	stats.overall.jumpType = static_cast<u8>(jump->GetJumpType());
	stats.overall.totalDistance = jump->totalDistance;
	stats.overall.maxSpeed = jump->currentMaxSpeed;
	stats.overall.maxHeight = jump->currentMaxHeight;
	stats.overall.airtime = jump->airtime;
	stats.overall.duckDuration = jump->duckDuration;
	stats.overall.duckEndDuration = jump->duckEndDuration;
	stats.overall.release = jump->release;
	stats.overall.block = -1.0f; // TODO
	stats.overall.edge = jump->GetEdge(false);
	stats.overall.landingEdge = jump->GetEdge(true);
	V_strncpy(stats.overall.invalidateReason, jump->invalidateReason, sizeof(stats.overall.invalidateReason));

	// Strafe stats
	for (int i = 0; i < jump->strafes.Count(); i++)
	{
		Strafe &s = jump->strafes[i];
		RpJumpStats::StrafeData strafe;
		strafe.duration = s.GetStrafeDuration();
		strafe.badAngles = s.GetBadAngleDuration();
		strafe.overlap = s.GetOverlapDuration();
		strafe.deadAir = s.GetDeadAirDuration();
		strafe.syncDuration = s.GetSyncDuration();
		strafe.width = s.GetWidth();
		strafe.airGain = s.GetGain();
		strafe.maxGain = s.GetMaxGain();
		strafe.airLoss = s.GetLoss();
		strafe.collisionGain = s.collisionGain;
		strafe.collisionLoss = s.collisionLoss;
		strafe.externalGain = s.externalGain;
		strafe.externalLoss = s.externalLoss;
		strafe.strafeMaxSpeed = s.GetStrafeMaxSpeed();
		// AR stats
		strafe.hasArStats = s.arStats.available;
		strafe.arMax = s.arStats.max;
		strafe.arMedian = s.arStats.median;
		strafe.arAverage = s.arStats.average;
		stats.strafes.push_back(strafe);

		// AACall stats
		for (int j = 0; j < s.aaCalls.Count(); j++)
		{
			AACall &a = s.aaCalls[j];
			RpJumpStats::AAData aa;
			aa.strafeIndex = i;
			aa.externalSpeedDiff = a.externalSpeedDiff;
			aa.prevYaw = a.prevYaw;
			aa.currentYaw = a.currentYaw;
			aa.wishDir[0] = a.wishdir.x;
			aa.wishDir[1] = a.wishdir.y;
			aa.wishDir[2] = a.wishdir.z;
			aa.wishSpeed = a.wishspeed;
			aa.accel = a.accel;
			aa.surfaceFriction = a.surfaceFriction;
			aa.duration = a.duration;
			aa.buttons[0] = a.buttons[0];
			aa.buttons[1] = a.buttons[1];
			aa.buttons[2] = a.buttons[2];
			aa.velocityPre[0] = a.velocityPre.x;
			aa.velocityPre[1] = a.velocityPre.y;
			aa.velocityPre[2] = a.velocityPre.z;
			aa.velocityPost[0] = a.velocityPost.x;
			aa.velocityPost[1] = a.velocityPost.y;
			aa.velocityPost[2] = a.velocityPost.z;
			aa.ducking = a.ducking;
			stats.aaCalls.push_back(aa);
		}
	}
}

void RpJumpStats::ToJump(Jump &out, RpJumpStats *js)
{
	// Overall stats
	out.serverTick = js->overall.serverTick;
	out.takeoffOrigin.x = js->overall.takeoffOrigin[0];
	out.takeoffOrigin.y = js->overall.takeoffOrigin[1];
	out.takeoffOrigin.z = js->overall.takeoffOrigin[2];
	out.adjustedTakeoffOrigin.x = js->overall.adjustedTakeoffOrigin[0];
	out.adjustedTakeoffOrigin.y = js->overall.adjustedTakeoffOrigin[1];
	out.adjustedTakeoffOrigin.z = js->overall.adjustedTakeoffOrigin[2];
	out.takeoffVelocity.x = js->overall.takeoffVelocity[0];
	out.takeoffVelocity.y = js->overall.takeoffVelocity[1];
	out.takeoffVelocity.z = js->overall.takeoffVelocity[2];
	out.landingOrigin.x = js->overall.landingOrigin[0];
	out.landingOrigin.y = js->overall.landingOrigin[1];
	out.landingOrigin.z = js->overall.landingOrigin[2];
	out.adjustedLandingOrigin.x = js->overall.adjustedLandingOrigin[0];
	out.adjustedLandingOrigin.y = js->overall.adjustedLandingOrigin[1];
	out.adjustedLandingOrigin.z = js->overall.adjustedLandingOrigin[2];
	out.jumpType = static_cast<JumpType>(js->overall.jumpType);
	out.totalDistance = js->overall.totalDistance;
	out.currentMaxSpeed = js->overall.maxSpeed;
	out.currentMaxHeight = js->overall.maxHeight;
	out.airtime = js->overall.airtime;
	out.duckDuration = js->overall.duckDuration;
	out.duckEndDuration = js->overall.duckEndDuration;
	out.release = js->overall.release;
	V_strncpy(out.invalidateReason, js->overall.invalidateReason, sizeof(out.invalidateReason));

	// Clear existing strafes just in case
	out.strafes.RemoveAll();

	// Recreate strafes from replay data
	for (size_t i = 0; i < js->strafes.size(); i++)
	{
		Strafe *strafe = out.strafes.AddToTailGetPtr();
		const RpJumpStats::StrafeData &strafeData = js->strafes[i];
		strafe->duration = strafeData.duration;
		strafe->badAngles = strafeData.badAngles;
		strafe->overlap = strafeData.overlap;
		strafe->deadAir = strafeData.deadAir;
		strafe->syncDuration = strafeData.syncDuration;
		strafe->width = strafeData.width;
		strafe->airGain = strafeData.airGain;
		strafe->maxGain = strafeData.maxGain;
		strafe->airLoss = strafeData.airLoss;
		strafe->collisionGain = strafeData.collisionGain;
		strafe->collisionLoss = strafeData.collisionLoss;
		strafe->externalGain = strafeData.externalGain;
		strafe->externalLoss = strafeData.externalLoss;
		strafe->strafeMaxSpeed = strafeData.strafeMaxSpeed;
		strafe->arStats.available = strafeData.hasArStats;
		strafe->arStats.max = strafeData.arMax;
		strafe->arStats.median = strafeData.arMedian;
		strafe->arStats.average = strafeData.arAverage;
		// Add AACall data for this strafe
		for (size_t j = 0; j < js->aaCalls.size(); j++)
		{
			AACall *aa = strafe->aaCalls.AddToTailGetPtr();
			const RpJumpStats::AAData &aaData = js->aaCalls[j];
			if (aaData.strafeIndex == i)
			{
				aa->externalSpeedDiff = aaData.externalSpeedDiff;
				aa->prevYaw = aaData.prevYaw;
				aa->currentYaw = aaData.currentYaw;
				aa->wishdir.x = aaData.wishDir[0];
				aa->wishdir.y = aaData.wishDir[1];
				aa->wishdir.z = aaData.wishDir[2];
				aa->wishspeed = aaData.wishSpeed;
				aa->accel = aaData.accel;
				aa->surfaceFriction = aaData.surfaceFriction;
				aa->duration = aaData.duration;
				aa->buttons[0] = aaData.buttons[0];
				aa->buttons[1] = aaData.buttons[1];
				aa->buttons[2] = aaData.buttons[2];
				aa->velocityPre.x = aaData.velocityPre[0];
				aa->velocityPre.y = aaData.velocityPre[1];
				aa->velocityPre.z = aaData.velocityPre[2];
				aa->velocityPost.x = aaData.velocityPost[0];
				aa->velocityPost.y = aaData.velocityPost[1];
				aa->velocityPost.z = aaData.velocityPost[2];
				aa->ducking = aaData.ducking;
			}
		}
	}

	// Calculate derived stats from strafe data
	out.deadAir = 0.0f;
	out.overlap = 0.0f;
	out.badAngles = 0.0f;
	out.sync = 0.0f;
	out.width = 0.0f;

	f32 jumpDuration = 0.0f;
	f32 gain = 0.0f;
	f32 maxGain = 0.0f;

	for (int i = 0; i < out.strafes.Count(); i++)
	{
		out.deadAir += out.strafes[i].GetDeadAirDuration();
		out.overlap += out.strafes[i].GetOverlapDuration();
		out.badAngles += out.strafes[i].GetBadAngleDuration();
		out.sync += out.strafes[i].GetSyncDuration();
		out.width += out.strafes[i].GetWidth();
		jumpDuration += out.strafes[i].GetStrafeDuration();
		gain += out.strafes[i].GetGain();
		maxGain += out.strafes[i].GetMaxGain();
	}

	if (out.strafes.Count() > 0)
	{
		out.width /= out.strafes.Count();
	}
	if (jumpDuration > 0.0f)
	{
		out.overlap /= jumpDuration;
		out.deadAir /= jumpDuration;
		out.badAngles /= jumpDuration;
		out.sync /= jumpDuration;
	}
	if (maxGain > 0.0f)
	{
		out.gainEff = gain / maxGain;
	}

	out.ended = true;
	out.valid = strlen(out.invalidateReason) == 0;
}

void RpJumpStats::PrintJump(KZPlayer *bot)
{
	Jump jump(bot);
	RpJumpStats::ToJump(jump, this);
	bool valid = jump.GetOffset() > -JS_EPSILON && jump.IsValid();
	for (KZPlayer *pl = bot->specService->GetNextSpectator(nullptr); pl != nullptr; pl = bot->specService->GetNextSpectator(pl))
	{
		if (!valid && !pl->jumpstatsService->jsAlways)
		{
			continue;
		}

		KZJumpstatsService::PlayJumpstatSound(pl, &jump);
		KZJumpstatsService::PrintJumpToChat(pl, &jump);
		KZJumpstatsService::PrintJumpToConsole(pl, &jump);
	}
}
