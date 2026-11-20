#pragma once

#include "common.h"
#include "kz/jumpstats/kz_jumpstats.h"

class Vector;

namespace KZ
{
	namespace DistancePrediction
	{
		// Velocity Verlet integration (half gravity -> move -> half gravity)
		// Then linear interpolation between the last frame/tick above targetZ and the first frame below it for the exact landing point
		f32 PredictLanding(f32 originX, f32 originY, f32 originZ, f32 velX, f32 velY, f32 velZ, f32 targetZ, f32 gravity, f32 *outPredX = nullptr,
						   f32 *outPredY = nullptr, f32 *outPredZ = nullptr, f32 *outRemainingFrames = nullptr);

		// Vertical-only Velocity Verlet simulation, same stepping as PredictLanding
		// Steps from (startZ, startVz) until the trajectory crosses currentZ, then linearly interpolates the sub-frame position
		f32 SimulateVerticalFrames(f32 startZ, f32 startVz, f32 currentZ, f32 gravity, bool rising);

		f32 GetPotentialTierThreshold(const char *modeShortName, JumpType jumpType, PotentialTier tier);
		f32 GetGravity();
		bool IsCrouching(KZPlayer *player);

		// The height a jump is predicted to land at: standing players land 9 units below the takeoff origin,
		// ducking players land at it
		f32 GetLandingTargetZ(f32 takeoffZ, bool crouching);

		// 32 units hull compensation added to a horizontal distance. Ladder Jumps take off from the ladder,
		// so it does not apply to them
		f32 GetHullCompensation(bool ladderJump);

		// Horizontal distance from the takeoff origin to where the jump is predicted to land, including the hull compensation
		f32 ComputePredictedDistance(f32 takeoffX, f32 takeoffY, f32 takeoffZ, f32 originX, f32 originY, f32 originZ, f32 velX, f32 velY, f32 velZ,
									 bool crouching, bool ladderJump);

		struct PotentialSample
		{
			f32 distance {};        // displacement so far + remaining predicted displacement, with hull compensation
			f32 progress {};        // 0-100, elapsed frames over elapsed + remaining frames
			f32 remainingFrames {}; // frames/ticks left until the predicted landing
		};

		// Samples the potential distance and jump progress for one tick
		bool ComputePotentialSample(f32 takeoffX, f32 takeoffY, f32 takeoffZ, f32 takeoffVelZ, f32 originX, f32 originY, f32 originZ, f32 velX,
									f32 velY, f32 velZ, bool crouching, bool ladderJump, PotentialSample &out);

		f32 GetPotentialRowThreshold(const char *modeShortName, JumpType jumpType, PotentialTier tier, bool preview);

		// Marks every tier whose threshold was first crossed on this tick and records the interpolated progress at which it happened
		void UpdatePotentialCrossings(const char *modeShortName, JumpType jumpType, f32 distance, f32 progress, PotentialCrossings &crossings);

		struct PotentialTierRow
		{
			PotentialTier tier {};
			f32 threshold {};
			bool hit {};
			f32 pct {};
		};

		i32 CollectPotentialTierRows(const char *modeShortName, JumpType jumpType, const PotentialCrossings &crossings, PotentialTierRow *out);
	} // namespace DistancePrediction
} // namespace KZ
