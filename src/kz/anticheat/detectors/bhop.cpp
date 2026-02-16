/*
	A bhop attempt is added when the player lands and:
	1. The player spent at least 4 ticks in the air before landing. This is to avoid cases where weird collisions cause false positives.
	2. The player did not teleport or noclip in the last 4 ticks. This is to avoid false positives from telehops or leaving noclips.
	3. sv_autobunnyhopping is off.
*/

#include "kz/anticheat/kz_anticheat.h"
#include "sdk/usercmd.h"
#include "utils/simplecmds.h"

#define MIN_AIR_TIME_FOR_BHOP    (4.0f * ENGINE_FIXED_TICK_INTERVAL) // Minimum air time to consider a jump for bhop hack detection
#define BHOP_IGNORE_DURATION     (4.0f * ENGINE_FIXED_TICK_INTERVAL) // Ignore teleports/noclips in the last 4 ticks
#define OLD_JUMP_PURGE_THRESHOLD (0.25f * ENGINE_FIXED_TICK_RATE)    // Purge jump attempts older than 0.25s
#define MIN_SAMPLE_COUNT         20                                  // Minimum number of samples before we start checking for bhop hacks
#define WINDOW_SIZE              30                                  // Number of recent jumps to consider for bhop hack detection
// Number of consecutive perfect bhops in the window to trigger a bhop hack infraction, regardless of ratio
#define NUM_CONSECUTIVE_PERFS_FOR_INFRACTION 25
// Number of consecutive perfs in the window to trigger a pattern check for bhop hack detection, regardless of ratio.
#define NUM_CONSECUTIVE_PERFS_FOR_PATTERN_CHECK 18
// Ratio of perfs to total jumps in the window to trigger a bhop hack infraction, once the window size is sufficiently large.
#define PERF_RATIO_FOR_BHOP_HACK_INFRACTION   0.9f
#define PERF_RATIO_FOR_HYPERSCROLL_INFRACTION 0.6f

#define REPETITIVE_PATTERN_THRESHOLD 0.9f // If 90% of the perfs are the same pattern, it might be a cheat...
#define LOW_PATTERN_THRESHOLD        4    // ...if the most common pattern is smaller than 4.

void KZAnticheatService::ParseCommandForJump(PlayerCommand *cmd)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	if (cmd->base().subtick_moves_size() == 0)
	{
		if (cmd->buttonstates.IsButtonNewlyPressed(IN_JUMP))
		{
			this->recentJumps.push_back(this->currentCmdNum);
			// Add to recent landing events
			for (auto &event : this->recentLandingEvents)
			{
				if (event.cmdNum >= this->currentCmdNum - OLD_JUMP_PURGE_THRESHOLD)
				{
					event.numJumpAfter++;
				}
			}
		}
	}
	else // Has subtick moves
	{
		// Assume that the +jump inputs are all legit. Faking +jump's wouldn't really help with anything.
		for (i32 i = 0; i < cmd->base().subtick_moves_size(); i++)
		{
			const CSubtickMoveStep &step = cmd->base().subtick_moves(i);
			if (step.button() == IN_JUMP && step.pressed())
			{
				this->recentJumps.push_back(this->currentCmdNum + step.when());
				for (auto &event : this->recentLandingEvents)
				{
					if (event.cmdNum + step.when() >= this->currentCmdNum - OLD_JUMP_PURGE_THRESHOLD)
					{
						event.numJumpAfter++;
					}
				}
			}
		}
	}

	// Purge old jump attempts
	// clang-format off
	this->recentJumps.erase(
		std::remove_if(
			this->recentJumps.begin(), this->recentJumps.end(),
			[&](f32 when) {
				return (this->currentCmdNum - when) > OLD_JUMP_PURGE_THRESHOLD;
			}),
		this->recentJumps.end());
	// clang-format on
}

void KZAnticheatService::CreateLandEvent()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	// Check for minimum air time
	if (this->currentAirTime < MIN_AIR_TIME_FOR_BHOP)
	{
		return;
	}
	// Check for valid movetype
	if (g_pKZUtils->GetServerGlobals()->curtime - this->lastValidMoveTypeTime < BHOP_IGNORE_DURATION)
	{
		return;
	}
	// Check for teleport
	if (this->player->JustTeleported(BHOP_IGNORE_DURATION))
	{
		return;
	}
	// Check sv_autobunnyhopping
	if (this->player->GetCvarValueFromModeStyles("sv_autobunnyhopping")->m_bValue != false)
	{
		return;
	}
	auto &event = this->recentLandingEvents.emplace_back();
	event.cmdNum = this->currentCmdNum;
	event.landingTime = this->player->landingTime;
	event.numJumpBefore = this->recentJumps.size();
}

void KZAnticheatService::OnChangeMoveType(MoveType_t oldMoveType)
{
	if (this->player->GetMoveType() != MOVETYPE_WALK)
	{
		this->lastValidMoveTypeTime = g_pKZUtils->GetServerGlobals()->curtime;
	}
}

void KZAnticheatService::OnProcessMovement()
{
	this->airMovedThisFrame = false;
}

void KZAnticheatService::OnAirMove()
{
	this->airMovedThisFrame = true;
	this->currentAirTime += g_pKZUtils->GetGlobals()->frametime;
}

void KZAnticheatService::OnProcessMovementPost()
{
	if (!this->airMovedThisFrame)
	{
		this->currentAirTime = 0.0f;
		if (!this->recentLandingEvents.empty())
		{
			this->recentLandingEvents.back().pendingPerf = false;
		}
	}
}

void KZAnticheatService::OnJump()
{
	if (this->player->IsPerfing(true) && !this->recentLandingEvents.empty() && this->recentLandingEvents.back().pendingPerf)
	{
		recentLandingEvents.back().hasPerfectBhop = true;
	}
}

void KZAnticheatService::CheckLandingEvents()
{
	while (this->recentLandingEvents.size() > WINDOW_SIZE)
	{
		this->recentLandingEvents.pop_front();
	}
	if (this->recentLandingEvents.size() >= MIN_SAMPLE_COUNT)
	{
		u32 numPerfect = 0;
		u32 maxConsecutivePerfect = 0;
		u32 currentConsecutivePerfect = 0;
		std::unordered_map<u32, u32> patterns;
		u32 averagePattern;
		for (const auto &event : this->recentLandingEvents)
		{
			if (event.hasPerfectBhop)
			{
				numPerfect++;
				currentConsecutivePerfect++;
				maxConsecutivePerfect = Max(maxConsecutivePerfect, currentConsecutivePerfect);
				patterns[event.numJumpBefore + event.numJumpAfter]++;
				averagePattern += event.numJumpBefore + event.numJumpAfter;
			}
			else
			{
				currentConsecutivePerfect = 0;
			}
		}
		if (numPerfect > 0)
		{
			averagePattern /= numPerfect;
		}
		if (maxConsecutivePerfect >= NUM_CONSECUTIVE_PERFS_FOR_INFRACTION)
		{
			this->MarkInfraction(KZAnticheatService::Infraction::Type::BhopHack,
								 tfm::format("%d/%d consecutive perfect bhops", maxConsecutivePerfect, (u32)this->recentLandingEvents.size()));
		}
		else if (maxConsecutivePerfect >= NUM_CONSECUTIVE_PERFS_FOR_PATTERN_CHECK)
		{
			// Check if it's the same pattern over and over again
			u32 maxPatternCount = 0;
			u32 patternWithMaxCount = 0;
			for (const auto &pattern : patterns)
			{
				if (pattern.second > maxPatternCount)
				{
					maxPatternCount = pattern.second;
					patternWithMaxCount = pattern.first;
				}
			}
			if (maxPatternCount >= numPerfect * REPETITIVE_PATTERN_THRESHOLD && patternWithMaxCount < LOW_PATTERN_THRESHOLD)
			{
				this->MarkInfraction(KZAnticheatService::Infraction::Type::BhopHack,
									 tfm::format("%d/%d occurrences of pattern %d", maxPatternCount, numPerfect, patternWithMaxCount));
			}
		}
		else
		{
			f32 ratio = (f32)numPerfect / (f32)this->recentLandingEvents.size();
			if (this->recentLandingEvents.size() >= MIN_SAMPLE_COUNT)
			{
				if (ratio >= PERF_RATIO_FOR_BHOP_HACK_INFRACTION)
				{
					this->MarkInfraction(
						KZAnticheatService::Infraction::Type::BhopHack,
						tfm::format("%.2f%% perfect bhops (%d/%d)", ratio * 100.0f, numPerfect, (u32)this->recentLandingEvents.size()));
				}
				else if (ratio >= PERF_RATIO_FOR_HYPERSCROLL_INFRACTION && averagePattern > 16)
				{
					this->MarkInfraction(KZAnticheatService::Infraction::Type::Hyperscroll, "");
				}
			}
		}
	}
}
