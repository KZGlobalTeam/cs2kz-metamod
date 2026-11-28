#include "distance_prediction.h"

#include "kz/mode/kz_mode.h"
#include "mathlib/vector.h"

#include <cstring>

namespace KZ
{
	namespace DistancePrediction
	{
		static const f32 POTENTIAL_TIER_THRESHOLDS_CKZ[JUMPTYPE_COUNT - 3][POTENTIALTIER_COUNT] = {
			{255.0f, 260.0f, 265.0f, 270.0f, 275.0f, 278.0f, 280.0f, 282.0f, 284.0f}, // LongJump
			{217.0f, 275.0f, 280.0f, 287.0f, 292.0f, 295.0f},                         // Bhop
			{217.0f, 275.0f, 280.0f, 287.0f, 292.0f, 295.0f},                         // MultiBhop
			{217.0f, 275.0f, 280.0f, 287.0f, 292.0f, 295.0f},                         // WeirdJump
			{120.0f, 160.0f, 170.0f, 180.0f, 190.0f, 200.0f},                         // LadderJump
			{217.0f, 260.0f, 265.0f, 270.0f, 275.0f, 278.0f},                         // LadderBhop
			{217.0f, 275.0f, 280.0f, 287.0f, 292.0f, 295.0f},                         // Jumpbug
		};

		static const f32 POTENTIAL_TIER_THRESHOLDS_VNL[JUMPTYPE_COUNT - 3][POTENTIALTIER_COUNT] = {
			{215.0f, 230.0f, 235.0f, 240.0f, 245.0f, 248.0f}, // LongJump
			{150.0f, 230.0f, 233.0f, 238.0f, 240.0f, 242.0f}, // Bhop
			{150.0f, 232.0f, 237.0f, 242.0f, 245.0f, 248.0f}, // MultiBhop
			{150.0f, 230.0f, 235.0f, 240.0f, 244.0f, 246.0f}, // WeirdJump
			{50.0f, 80.0f, 90.0f, 100.0f, 105.0f, 108.0f},    // LadderJump
			{215.0f, 250.0f, 253.0f, 258.0f, 261.0f, 263.0f}, // LadderBhop
			{215.0f, 255.0f, 260.0f, 265.0f, 270.0f, 272.0f}, // Jumpbug
		};

		f32 GetPotentialTierThreshold(const char *modeShortName, JumpType jumpType, PotentialTier tier)
		{
			if ((i32)jumpType < 0 || (i32)jumpType >= JUMPTYPE_COUNT - 3 || tier >= POTENTIALTIER_COUNT)
			{
				return 0.0f;
			}
			const bool vanilla = modeShortName != nullptr && !std::strcmp(modeShortName, "VNL");
			return (vanilla ? POTENTIAL_TIER_THRESHOLDS_VNL : POTENTIAL_TIER_THRESHOLDS_CKZ)[(i32)jumpType][(i32)tier];
		}

		f32 PredictLanding(f32 originX, f32 originY, f32 originZ, f32 velX, f32 velY, f32 velZ, f32 targetZ, f32 gravity, f32 *outPredX,
						   f32 *outPredY, f32 *outPredZ, f32 *outRemainingFrames)
		{
			if (outPredX)
			{
				*outPredX = originX;
			}
			if (outPredY)
			{
				*outPredY = originY;
			}
			if (outPredZ)
			{
				*outPredZ = originZ;
			}
			if (outRemainingFrames)
			{
				*outRemainingFrames = 0.0f;
			}

			if (gravity <= 0.0f || originZ < targetZ)
			{
				return 0.0f;
			}

			constexpr f32 dt = 0.0078125f;
			const f32 halfGrav = gravity * dt * 0.5f;

			f32 curX = originX, curY = originY, curZ = originZ;
			f32 curVz = velZ;
			f32 prevX = curX, prevY = curY, prevZ = curZ;

			i32 frameCount = 0;
			bool found = false;

			for (i32 i = 0; i < 500; i++)
			{
				prevX = curX;
				prevY = curY;
				prevZ = curZ;

				// Velocity Verlet: half gravity -> move -> half gravity
				curVz -= halfGrav;
				curX += velX * dt;
				curY += velY * dt;
				curZ += curVz * dt;
				curVz -= halfGrav;

				frameCount++;

				if (curZ < targetZ)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				return 0.0f;
			}

			// cur is the first frame below targetZ, prev is the frame before it
			// The landing happens fraction of the way between them
			f32 predX = curX, predY = curY, predZ = curZ;
			f32 fraction = 1.0f;
			const f32 denom = prevZ - curZ;
			if (denom != 0.0f)
			{
				fraction = (prevZ - targetZ) / denom;
				predX = prevX + (curX - prevX) * fraction;
				predY = prevY + (curY - prevY) * fraction;
				predZ = prevZ + (curZ - prevZ) * fraction;
			}

			if (outPredX)
			{
				*outPredX = predX;
			}
			if (outPredY)
			{
				*outPredY = predY;
			}
			if (outPredZ)
			{
				*outPredZ = predZ;
			}

			const f32 remainingFrames = (f32)frameCount - 1.0f + fraction;
			if (outRemainingFrames)
			{
				*outRemainingFrames = remainingFrames;
			}

			return remainingFrames * dt;
		}

		f32 SimulateVerticalFrames(f32 startZ, f32 startVz, f32 currentZ, f32 gravity, bool rising)
		{
			if (gravity <= 0.0f)
			{
				return 0.0f;
			}

			constexpr f32 dt = 0.0078125f;
			const f32 halfGrav = gravity * dt * 0.5f;

			f32 simZ = startZ;
			f32 simVz = startVz;
			f32 prevSimZ = simZ;
			i32 simFrames = 0;
			f32 simFrac = 1.0f;
			bool crossed = false;

			for (i32 i = 0; i < 500; i++)
			{
				prevSimZ = simZ;
				simVz -= halfGrav;
				simZ += simVz * dt;
				simVz -= halfGrav;
				simFrames++;

				if (simVz > 0.0f)
				{
					// Rising segment: only count it when the player is also rising
					if (rising && simZ >= currentZ)
					{
						crossed = true;
						break;
					}
				}
				else
				{
					// Past the apex: descending to currentZ counts regardless of player state
					if (simZ <= currentZ)
					{
						crossed = true;
						break;
					}
				}
			}

			if (!crossed)
			{
				return 0.0f;
			}

			const f32 denom = simZ - prevSimZ;
			if (denom != 0.0f)
			{
				simFrac = (currentZ - prevSimZ) / denom;
			}

			return (f32)simFrames - 1.0f + simFrac;
		}

		f32 GetGravity()
		{
			ConVarRefAbstract *svGravity = KZ::mode::modeCvarRefs[MODECVAR_SV_GRAVITY];
			if (!svGravity)
			{
				return 800.0f;
			}
			const f32 gravity = svGravity->GetFloat();
			return gravity > 0.0f ? gravity : 800.0f;
		}

		bool IsCrouching(KZPlayer *player)
		{
			CCSPlayerPawn *pawn = player->GetPlayerPawn();
			if (!pawn)
			{
				return false;
			}
			auto *moveServices = player->GetMoveServices();
			return (pawn->m_fFlags & FL_DUCKING) || (moveServices && moveServices->m_bDucking());
		}

		f32 GetLandingTargetZ(f32 takeoffZ, bool crouching)
		{
			return takeoffZ - (crouching ? 0.0f : 9.0f);
		}

		f32 GetHullCompensation(bool ladderJump)
		{
			return ladderJump ? 0.0f : 32.0f;
		}

		f32 ComputePredictedDistance(f32 takeoffX, f32 takeoffY, f32 takeoffZ, f32 originX, f32 originY, f32 originZ, f32 velX, f32 velY, f32 velZ,
									 bool crouching, bool ladderJump)
		{
			const f32 gravity = GetGravity();
			const f32 targetZ = GetLandingTargetZ(takeoffZ, crouching);
			f32 predX, predY, predZ;
			if (PredictLanding(originX, originY, originZ, velX, velY, velZ, targetZ, gravity, &predX, &predY, &predZ) <= 0.0f)
			{
				return 0.0f;
			}
			const f32 dx = predX - takeoffX;
			const f32 dy = predY - takeoffY;
			return sqrt(dx * dx + dy * dy) + GetHullCompensation(ladderJump);
		}

		bool ComputePotentialSample(f32 takeoffX, f32 takeoffY, f32 takeoffZ, f32 takeoffVelZ, f32 originX, f32 originY, f32 originZ, f32 velX,
									f32 velY, f32 velZ, bool crouching, bool ladderJump, PotentialSample &out)
		{
			const f32 gravity = GetGravity();
			const f32 targetZ = GetLandingTargetZ(takeoffZ, crouching);
			f32 predX, predY, predZ;
			f32 remainingFrames = 0.0f;
			PredictLanding(originX, originY, originZ, velX, velY, velZ, targetZ, gravity, &predX, &predY, &predZ, &remainingFrames);

			if (remainingFrames <= 0.0f)
			{
				return false;
			}

			// Potential distance = horizontal distance from takeoff to the current origin + horizontal distance from the current origin to the
			// predicted landing point + 32
			const f32 currDx = originX - takeoffX;
			const f32 currDy = originY - takeoffY;
			const f32 predDx = predX - originX;
			const f32 predDy = predY - originY;
			out.distance = sqrt(currDx * currDx + currDy * currDy) + sqrt(predDx * predDx + predDy * predDy) + GetHullCompensation(ladderJump);

			// Jump progress: vertical trajectory from takeoff to the current height, over that plus the frames left
			const bool rising = velZ > 0.0f;
			const f32 elapsedFrames = SimulateVerticalFrames(takeoffZ, takeoffVelZ, originZ, gravity, rising);
			const f32 totalFrames = elapsedFrames + remainingFrames;
			out.progress = 0.0f;
			if (totalFrames > 0.0f)
			{
				out.progress = 100.0f * elapsedFrames / totalFrames;
				out.progress = MAX(0.0f, MIN(out.progress, 100.0f));
			}
			out.remainingFrames = remainingFrames;
			return true;
		}

		f32 GetPotentialRowThreshold(const char *modeShortName, JumpType jumpType, PotentialTier tier, bool preview)
		{
			const f32 threshold = GetPotentialTierThreshold(modeShortName, jumpType, tier);
			if (preview && threshold <= 0.0f)
			{
				// Edit mode
				return GetPotentialTierThreshold(nullptr, JumpType_LongJump, tier);
			}
			return threshold;
		}

		void UpdatePotentialCrossings(const char *modeShortName, JumpType jumpType, f32 distance, f32 progress, PotentialCrossings &crossings)
		{
			for (i32 t = (i32)PotentialTier_Meh; t < (i32)POTENTIALTIER_COUNT; t++)
			{
				if (crossings.hit[t])
				{
					continue;
				}
				const f32 threshold = GetPotentialTierThreshold(modeShortName, jumpType, (PotentialTier)t);
				if (threshold <= 0.0f || distance < threshold)
				{
					continue;
				}
				// Interpolate between the last two ticks so a threshold crossed mid-tick reports where it actually happened rather than the current
				// tick's progress
				f32 hitPct = progress;
				if (crossings.hasPrev && crossings.prevDistance < threshold)
				{
					const f32 denom = distance - crossings.prevDistance;
					const f32 frac = (denom > 0.0f) ? ((threshold - crossings.prevDistance) / denom) : 1.0f;
					hitPct = crossings.prevProgress + (progress - crossings.prevProgress) * frac;
				}
				crossings.hit[t] = true;
				crossings.pct[t] = hitPct;
			}
			crossings.prevDistance = distance;
			crossings.prevProgress = progress;
			crossings.hasPrev = true;
		}

		i32 CollectPotentialTierRows(const char *modeShortName, JumpType jumpType, const PotentialCrossings &crossings, PotentialTierRow *out)
		{
			i32 count = 0;
			for (i32 t = (i32)PotentialTier_Meh; t < (i32)POTENTIALTIER_COUNT; t++)
			{
				const f32 threshold = GetPotentialTierThreshold(modeShortName, jumpType, (PotentialTier)t);
				if (threshold <= 0.0f)
				{
					continue;
				}
				PotentialTierRow &row = out[count++];
				row.tier = (PotentialTier)t;
				row.threshold = threshold;
				row.hit = crossings.hit[t];
				row.pct = crossings.pct[t];
			}
			return count;
		}
	} // namespace DistancePrediction
} // namespace KZ
