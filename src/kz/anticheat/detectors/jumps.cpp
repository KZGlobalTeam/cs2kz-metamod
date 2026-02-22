/*
	Very basic autostrafe detector that kicks players who strafe perfectly.
	Based on jumpstats.
*/

#include "kz/anticheat/kz_anticheat.h"
#include "kz/jumpstats/kz_jumpstats.h"

#define MIN_JUMP_DURATION_FOR_DETECTION 0.6f // Only consider jumps longer than this duration
#define MIN_SYNC_FOR_DETECTION          0.7f // Minimum sync ratio to consider for detection

#define NUM_JUMPS_WINDOW_SIZE           20 // How many recent jumps to consider for autostrafe detection
#define BASE_SUSPICIOUS_JUMPS_THRESHOLD 15 // Minimum number of high strafe count jumps within the window to consider for detection.

// If the player has a high number of strafes per second, mark them as suspicious.
#define BASE_STRAFES_PER_SECOND_THRESHOLD 20.0f

// Sometimes unstable mouse can cause 1 tick strafes.
#define MIN_STRAFE_DURATION              0.02f // If we ignore strafes shorter than this duration...
#define REAL_STRAFE_PER_SECOND_THRESHOLD 18.0f // then this is the threshold for suspicious strafes per second.

#define MAX_STRAFES_PER_SECOND_THRESHOLD 30.0f // At this many strafes per second, even a few jumps are suspicious
#define MIN_SUSPICIOUS_JUMPS_THRESHOLD   5     //... in which case we lower the threshold to this many jumps.

// High gain efficiency jumps are suspicious if the player has a high number of strafes per second or if the jump distance is significant.
#define GAIN_EFFICIENCY_THRESHOLD               0.95f // ~397 max for CKZ LJ
#define STRAFE_PER_SECOND_THRESHOLD_FOR_GAINEFF 6.0f
#define JUMP_DISTANCE_THRESHOLD_FOR_GAINEFF     240.0f

// A player will not have this many AA calls in a single tick without using some sort of auto-strafe hack.
#define NUM_AA_CALLS_PER_TICK_FOR_SUSPICION 10

void KZAnticheatService::OnJumpFinish(Jump *jump)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	if (!this->ShouldRunDetections())
	{
		return;
	}
	if (this->player->styleServices.Count() > 0)
	{
		// Styles can modify player's strafes in unpredictable ways, so skip detection for styled players.
		return;
	}
	if (jump->invalidateReason[0] != '\0' || !jump->IsValid())
	{
		return;
	}
	if (jump->airtime < MIN_JUMP_DURATION_FOR_DETECTION)
	{
		return;
	}

	if (jump->GetSync() <= MIN_SYNC_FOR_DETECTION)
	{
		this->recentJumpStatuses.push_back(JumpStatus::Normal);
	}
	else if (jump->strafes.Count() / jump->airtime > MAX_STRAFES_PER_SECOND_THRESHOLD)
	{
		this->recentJumpStatuses.push_back(JumpStatus::VeryHighStrafeCount);
	}
	else if (jump->strafes.Count() / jump->airtime > BASE_STRAFES_PER_SECOND_THRESHOLD)
	{
		this->recentJumpStatuses.push_back(JumpStatus::HighStrafeCount);
	}
	else
	{
		i32 numEffectiveStrafes = 0;
		FOR_EACH_VEC(jump->strafes, i)
		{
			if (jump->strafes[i].duration >= MIN_STRAFE_DURATION)
			{
				numEffectiveStrafes++;
			}
		}
		f32 effectiveStrafesPerSecond = (f32)numEffectiveStrafes / jump->airtime;
		if (effectiveStrafesPerSecond > REAL_STRAFE_PER_SECOND_THRESHOLD)
		{
			this->recentJumpStatuses.push_back(JumpStatus::HighStrafeCount);
		}
		else if (effectiveStrafesPerSecond > STRAFE_PER_SECOND_THRESHOLD_FOR_GAINEFF
				 && (jump->GetGainEfficiency() > GAIN_EFFICIENCY_THRESHOLD && jump->GetDistance() > JUMP_DISTANCE_THRESHOLD_FOR_GAINEFF))
		{
			this->recentJumpStatuses.push_back(JumpStatus::HighEfficiency);
		}
		else
		{
			this->recentJumpStatuses.push_back(JumpStatus::Normal);
		}
	}
	// Keep only the recent N jumps.
	while (this->recentJumpStatuses.size() > NUM_JUMPS_WINDOW_SIZE)
	{
		this->recentJumpStatuses.pop_front();
	}
	// Count suspicious jumps.
	i32 suspiciousJumpCount = 0;
	i32 numVeryHighStrafeJumps = 0;
	for (auto status : this->recentJumpStatuses)
	{
		if (status != JumpStatus::Normal)
		{
			suspiciousJumpCount++;
		}
		if (status == JumpStatus::VeryHighStrafeCount)
		{
			numVeryHighStrafeJumps++;
		}
	}
	if (suspiciousJumpCount >= BASE_SUSPICIOUS_JUMPS_THRESHOLD
		|| (suspiciousJumpCount >= MIN_SUSPICIOUS_JUMPS_THRESHOLD && numVeryHighStrafeJumps > 0))
	{
		std::string details = tfm::format("Strafe hack detected: %d suspicious jumps out of last %d (%.2f%%).", suspiciousJumpCount,
										  NUM_JUMPS_WINDOW_SIZE, (f32)suspiciousJumpCount / (f32)NUM_JUMPS_WINDOW_SIZE * 100.0f);
		META_CONPRINTF("%s\n", details.c_str());
		this->MarkInfraction(Infraction::Type::StrafeHack, details);
	}
}
