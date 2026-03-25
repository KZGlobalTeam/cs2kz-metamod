/*
 * Block size, edge, failstat, and always-on miss tracking.
 * Ported from GOKZ's jump_tracking.sp.
 */

#include "kz_jumpstats.h"
#include "sdk/tracefilter.h"
#include "utils/utils.h"

// ============================================================
// Trace helpers
// ============================================================

// Hull sweep from start to end with the given bounding box. Returns true if hit.
static bool TraceHullPosition(KZPlayer *player, const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, Vector &position)
{
	CCSPlayerPawnBase *pawn = player->GetPlayerPawn();
	if (!pawn)
	{
		return false;
	}
	trace_t tr;
	bbox_t bounds = {mins, maxs};
	CTraceFilterPlayerMovementCS filter(pawn);
	g_pKZUtils->TracePlayerBBox(start, end, bounds, &filter, tr);
	if (tr.DidHit())
	{
		position = tr.m_vEndPos;
		return true;
	}
	return false;
}

// Point (ray) trace from start to end. Returns true if hit.
static bool TraceRayPosition(KZPlayer *player, const Vector &start, const Vector &end, Vector &position)
{
	CCSPlayerPawnBase *pawn = player->GetPlayerPawn();
	if (!pawn)
	{
		return false;
	}
	trace_t tr;
	bbox_t bounds({vec3_origin, vec3_origin});
	CTraceFilterPlayerMovementCS filter(pawn);
	g_pKZUtils->TracePlayerBBox(start, end, bounds, &filter, tr);
	if (tr.DidHit())
	{
		position = tr.m_vEndPos;
		return true;
	}
	return false;
}

// Point trace returning both hit position and surface normal.
static bool TraceRayPositionNormal(KZPlayer *player, const Vector &start, const Vector &end, Vector &position, Vector &normal)
{
	CCSPlayerPawnBase *pawn = player->GetPlayerPawn();
	if (!pawn)
	{
		return false;
	}
	trace_t tr;
	bbox_t bounds({vec3_origin, vec3_origin});
	CTraceFilterPlayerMovementCS filter(pawn);
	g_pKZUtils->TracePlayerBBox(start, end, bounds, &filter, tr);
	if (tr.DidHit())
	{
		position = tr.m_vEndPos;
		normal = tr.m_vHitNormal;
		return true;
	}
	return false;
}

// coordDist: 0 = X-axis dominant, 1 = Y-axis dominant
// distSign: +1 (landing > takeoff along that axis) or -1
static void GetCoordOrientation(const Vector &landingPos, const Vector &takeoffPos, i32 &coordDist, i32 &distSign)
{
	coordDist = fabsf(landingPos.x - takeoffPos.x) < fabsf(landingPos.y - takeoffPos.y) ? 1 : 0;
	distSign = landingPos[coordDist] > takeoffPos[coordDist] ? 1 : -1;
}

// Downward scan to find the top surface Z of a block.
// Searches 'searchArea' units above and below origin[coord + offset].
// Returns a very large negative number on failure.
static f32 FindBlockHeight(KZPlayer *player, const Vector &origin, f32 offset, i32 coord, f32 searchArea)
{
	Vector traceStart = origin;
	traceStart[coord] += offset;
	Vector traceEnd = traceStart;
	traceStart.z += searchArea;
	traceEnd.z -= searchArea;
	Vector hitPos, hitNormal;
	if (!TraceRayPositionNormal(player, traceStart, traceEnd, hitPos, hitNormal) || fabsf(hitNormal.z - 1.0f) > JS_EPSILON)
	{
		return -1e30f;
	}
	return hitPos.z;
}

// 3-step 54-unit vertical scan. Used by AlwaysFailstat to find the block surface
// when the block has a roof (headbanger geometry).
static bool TryFindBlockHeight(KZPlayer *player, const Vector &position, Vector &result, i32 coordDist, i32 distSign)
{
	Vector traceStart = position;
	traceStart[coordDist] += (f32)distSign;
	Vector traceEnd = traceStart;
	traceStart.z += 54.0f;
	// traceEnd.z stays at position.z (below, so ray goes downward)

	for (i32 i = 0; i < 3; i++)
	{
		Vector hitPos;
		if (TraceRayPosition(player, traceStart, traceEnd, hitPos))
		{
			if (fabsf(hitPos.z - traceStart.z) > JS_EPSILON)
			{
				result = hitPos;
				result[coordDist] -= (f32)distSign;
				return true;
			}
		}
		traceStart.z += 54.0f;
		traceEnd.z += 53.0f;
	}
	return false;
}

// Short point trace; returns true if it hits and the hit normal is aligned with coordDist.
static bool BlockTraceAligned(KZPlayer *player, const Vector &start, const Vector &end, i32 coordDist)
{
	Vector pos, normal;
	if (!TraceRayPositionNormal(player, start, end, pos, normal))
	{
		return false;
	}
	return fabsf(fabsf(normal[coordDist]) - 1.0f) <= JS_EPSILON;
}

// Verifies that the faces of the takeoff and landing blocks are parallel.
static bool BlockAreEdgesParallel(KZPlayer *player, const Vector &startBlock, const Vector &endBlock, f32 deviation, i32 coordDist, i32 coordDev)
{
	f32 offset = startBlock[coordDist] > endBlock[coordDist] ? 0.1f : -0.1f;

	Vector start, end;
	start[coordDist] = startBlock[coordDist] - offset;
	start[coordDev] = startBlock[coordDev] - deviation;
	start.z = startBlock.z;
	end[coordDist] = startBlock[coordDist] + offset;
	end[coordDev] = startBlock[coordDev] - deviation;
	end.z = startBlock.z;

	if (BlockTraceAligned(player, start, end, coordDist))
	{
		start[coordDist] = endBlock[coordDist] + offset;
		end[coordDist] = endBlock[coordDist] - offset;
		if (BlockTraceAligned(player, start, end, coordDist))
		{
			return true;
		}
		start[coordDist] = startBlock[coordDist] - offset;
		end[coordDist] = startBlock[coordDist] + offset;
	}

	start[coordDev] = startBlock[coordDev] + deviation;
	end[coordDev] = startBlock[coordDev] + deviation;

	if (BlockTraceAligned(player, start, end, coordDist))
	{
		start[coordDist] = endBlock[coordDist] + offset;
		end[coordDist] = endBlock[coordDist] - offset;
		if (BlockTraceAligned(player, start, end, coordDist))
		{
			return true;
		}
	}

	return false;
}

// TODO: Add landing edge tracking
f32 Jump::GetEdge(bool landing)
{
	return this->edge;
}

// ============================================================
// RecordPose - ring-buffer snapshot, used only by AlwaysFailstat
// ============================================================

void Jump::RecordPose()
{
	this->poseIndex = (this->poseIndex + 1) % JS_FAILSTATS_MAX_TRACKED_TICKS;
	if (this->poseCount < JS_FAILSTATS_MAX_TRACKED_TICKS)
	{
		this->poseCount++;
	}
	JumpPose &p = this->poseHistory[this->poseIndex];
	p.position = this->player->currentMoveData->m_vecAbsOrigin;
	p.airtime = this->airtime;

	// Snapshot cumulative sync/BA/OL/DA from ended strafes.
	// These running totals are updated whenever a strafe is closed.
	f32 totalDuration = this->poseEndedDuration;
	f32 syncDuration = this->poseEndedSyncDuration;
	f32 baDuration = this->poseEndedBadAnglesDuration;
	f32 olDuration = this->poseEndedOverlapDuration;
	f32 daDuration = this->poseEndedDeadAirDuration;
	p.sync = totalDuration > 0.0f ? syncDuration / totalDuration : 0.0f;
	p.badAngles = totalDuration > 0.0f ? baDuration / totalDuration : 0.0f;
	p.overlap = totalDuration > 0.0f ? olDuration / totalDuration : 0.0f;
	p.deadAir = totalDuration > 0.0f ? daDuration / totalDuration : 0.0f;
}

// ============================================================
// GetFailOrigin - position-based linear interpolation
//
// Finds the X/Y position at planeHeight by interpolating between
// poseHistory[poseOffset] (at or above planeHeight) and
// poseHistory[poseOffset+1] (the next newer pose, below planeHeight).
// ============================================================

bool Jump::GetFailOrigin(f32 planeHeight, Vector &result, i32 poseOffset)
{
	const i32 size = JS_FAILSTATS_MAX_TRACKED_TICKS;
	i32 idxOld = ((this->poseIndex + poseOffset) % size + size) % size;
	i32 idxNew = ((this->poseIndex + poseOffset + 1) % size + size) % size;

	const Vector &posOld = this->poseHistory[idxOld].position;
	const Vector &posNew = this->poseHistory[idxNew].position;

	f32 dz = posNew.z - posOld.z;
	if (fabsf(dz) < JS_EPSILON)
	{
		return false;
	}

	f32 scale = (planeHeight - posOld.z) / dz;
	result.x = posOld.x + (posNew.x - posOld.x) * scale;
	result.y = posOld.y + (posNew.y - posOld.y) * scale;
	result.z = planeHeight;
	return true;
}

// ============================================================
// UpdateFailstat - called per-subtick during flight
//
// For LadderJumps: detects the destination block when the player
// is 10+ units above takeoff in their arc (before they start
// descending back to block Z), then fires once they cross below.
//
// For all other types: failstatBlockHeight = adjustedTakeoffOrigin.z
// is set at Init(), so as soon as the player drops below that Z
// this function interpolates the crossing position and calls
// CalcBlockStats to find block + edge.
// ============================================================

void Jump::UpdateFailstat()
{
	if (this->jumpType == JumpType_FullInvalid)
	{
		return;
	}

	const Vector &currentPos = this->player->currentMoveData->m_vecAbsOrigin;
	const Vector &prePos = this->player->moveDataPre.m_vecAbsOrigin;

	i32 coordDist, distSign;
	GetCoordOrientation(currentPos, this->adjustedTakeoffOrigin, coordDist, distSign);

	// For LAJs, detect the destination block on the way up
	if (!this->failstatBlockDetected)
	{
		// Fire when player has peaked above 10 units and is now descending back toward takeoff Z
		if (currentPos.z - this->adjustedTakeoffOrigin.z < 10.0f && this->currentMaxHeight > 10.0f)
		{
			this->failstatBlockDetected = true;

			Vector traceStart = this->adjustedTakeoffOrigin;
			traceStart.z -= 5.0f;
			Vector blockEnd = traceStart;
			traceStart[coordDist] += JS_MIN_LAJ_BLOCK_DISTANCE * distSign;
			blockEnd[coordDist] += JS_MAX_LAJ_FAILSTAT_DISTANCE * distSign;

			Vector hitPos;
			if (!TraceHullPosition(this->player, traceStart, blockEnd, Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 0.0f), hitPos))
			{
				return;
			}

			hitPos.z += 5.0f;
			this->failstatBlockHeight = FindBlockHeight(this->player, hitPos, (f32)distSign * 17.0f, coordDist, 10.0f) - JS_OFFSET_EPSILON;
		}
	}

	// Wait until player has crossed below the block surface Z
	if (currentPos.z >= this->failstatBlockHeight)
	{
		// Crossing is provisional until we stay below this plane.
		this->failstatValid = false;
		return;
	}

	// Latch the first valid fail-crossing sample while staying below the plane.
	// If the player rises back above failstatBlockHeight, failstatValid is cleared above.
	if (this->failstatValid)
	{
		return;
	}

	// Interpolate the crossing point using pre-tick velocity.
	Vector failstatPosition;
	const Vector &preVel = this->player->moveDataPre.m_vecVelocity;
	const f32 tickDt = g_pKZUtils->GetGlobals()->frametime;
	f32 velStepZ = preVel.z * tickDt;
	if (fabsf(velStepZ) < JS_EPSILON || prePos.z <= this->failstatBlockHeight)
	{
		return;
	}

	// Reject non-physical crossings (e.g. hull origin snaps) and wait for a real descent crossing.
	f32 predictedZ = prePos.z + velStepZ;
	if (predictedZ >= this->failstatBlockHeight)
	{
		return;
	}

	f32 scale = (this->failstatBlockHeight - prePos.z) / velStepZ;
	failstatPosition.x = prePos.x + (preVel.x * tickDt) * scale;
	failstatPosition.y = prePos.y + (preVel.y * tickDt) * scale;
	failstatPosition.z = this->failstatBlockHeight;

	// Horizontal distance from takeoff to fail position (NOT yet including +32 player model)
	f32 rawDist = (failstatPosition - this->adjustedTakeoffOrigin).Length2D();

	// Construct the estimated landing block position by mirroring failstatPosition around takeoff
	Vector block = this->adjustedTakeoffOrigin;
	block[coordDist] = 2.0f * failstatPosition[coordDist] - this->adjustedTakeoffOrigin[coordDist];
	block[1 - coordDist] = failstatPosition[1 - coordDist];
	block.z = this->failstatBlockHeight;

	bool isLadderJump = (this->jumpType == JumpType_LadderJump);
	bool hadValidFailstat = this->failstatValid;
	f32 prevBlock = this->block;
	f32 prevEdge = this->edge;
	f32 prevDistance = this->failstatDistance;
	f32 prevOffset = this->failstatOffset;
	f32 prevSync = this->failstatSync;
	f32 prevBadAngles = this->failstatBadAngles;

	if (!isLadderJump && rawDist >= (f32)JS_MIN_BLOCK_DISTANCE)
	{
		this->CalcBlockStats(block, true);
	}
	else if (isLadderJump && rawDist >= (f32)JS_MIN_LAJ_BLOCK_DISTANCE)
	{
		this->CalcLadderBlockStats(block, true);
	}
	else
	{
		return;
	}

	if (this->block > 0.0f)
	{
		// Use interpolated fail-crossing distance for failstat reporting.
		this->failstatDistance = rawDist + (isLadderJump ? 0.0f : 32.0f);
		this->failstatOffset = failstatPosition.z - this->adjustedTakeoffOrigin.z;

		f32 syncDuration = 0.0f;
		f32 baDuration = 0.0f;
		f32 totalDuration = 0.0f;
		FOR_EACH_VEC(this->strafes, i)
		{
			syncDuration += this->strafes[i].GetSyncDuration();
			baDuration += this->strafes[i].GetBadAngleDuration();
			totalDuration += this->strafes[i].GetStrafeDuration();
		}
		this->failstatSync = totalDuration > 0.0f ? syncDuration / totalDuration : 0.0f;
		this->failstatBadAngles = totalDuration > 0.0f ? baDuration / totalDuration : 0.0f;
		this->failstatValid = true;
	}
	else if (hadValidFailstat)
	{
		// Keep the last valid estimate if a later subtick sample is invalid.
		this->block = prevBlock;
		this->edge = prevEdge;
		this->failstatDistance = prevDistance;
		this->failstatOffset = prevOffset;
		this->failstatSync = prevSync;
		this->failstatBadAngles = prevBadAngles;
		this->failstatValid = true;
	}
}

// ============================================================
// EndBlockDistance - block stats on a successful landing
// ============================================================

void Jump::EndBlockDistance()
{
	bool isLadderJump = (this->jumpType == JumpType_LadderJump);
	f32 dist = this->GetDistance();
	if (!isLadderJump && dist >= (f32)JS_MIN_BLOCK_DISTANCE)
	{
		this->CalcBlockStats(this->adjustedLandingOrigin);
	}
	else if (isLadderJump && dist >= (f32)JS_MIN_LAJ_BLOCK_DISTANCE)
	{
		this->CalcLadderBlockStats(this->adjustedLandingOrigin);
	}
}

// ============================================================
// CalcBlockStats - compute block size and takeoff edge
// ============================================================

void Jump::CalcBlockStats(Vector landingOrigin, bool checkOffset)
{
	i32 coordDist, distSign;
	GetCoordOrientation(landingOrigin, this->adjustedTakeoffOrigin, coordDist, distSign);
	i32 coordDev = 1 - coordDist;

	f32 deviation = fabsf(landingOrigin[coordDev] - this->adjustedTakeoffOrigin[coordDev]);

	// Midpoint of the jump gap - assumed to be over open air, not inside a block
	Vector middle;
	middle[coordDist] = (this->adjustedTakeoffOrigin[coordDist] + landingOrigin[coordDist]) / 2.0f;
	middle[coordDev] = (this->adjustedTakeoffOrigin[coordDev] + landingOrigin[coordDev]) / 2.0f;
	middle.z = this->adjustedTakeoffOrigin.z - 1.0f;

	// Sweep line perpendicular to jump direction, wide enough to catch the blocks despite deviation
	Vector sweepMin = vec3_origin;
	Vector sweepMax = vec3_origin;
	sweepMin[coordDev] = -(deviation + 16.0f);
	sweepMax[coordDev] = deviation + 16.0f;

	// Start of the gap (takeoff side): player bounding box places the wall 16 units back
	Vector startBlock = middle;
	startBlock[coordDist] = this->adjustedTakeoffOrigin[coordDist] - distSign * 16.0f;

	// End of the gap (landing side): extend slightly past block face to catch the JS_OFFSET_EPSILON case
	Vector endBlock = middle;
	endBlock[coordDist] = landingOrigin[coordDist] + distSign * (16.0f + JS_OFFSET_EPSILON);

	if (!TraceHullPosition(this->player, middle, startBlock, sweepMin, sweepMax, startBlock)
		|| !TraceHullPosition(this->player, middle, endBlock, sweepMin, sweepMax, endBlock))
	{
		return;
	}

	if (!BlockAreEdgesParallel(this->player, startBlock, endBlock, deviation + 32.0f, coordDist, coordDev))
	{
		this->block = 0.0f;
		this->edge = -1.0f;
		return;
	}

	// For failstats: verify that the block surface is actually at failstatBlockHeight
	if (checkOffset)
	{
		Vector checkPos = endBlock;
		checkPos.z += 1.0f;
		f32 blockSurfZ = FindBlockHeight(this->player, checkPos, (f32)distSign * 17.0f, coordDist, 1.0f);
		if (fabsf(blockSurfZ - landingOrigin.z) > JS_OFFSET_EPSILON)
		{
			return;
		}
	}

	f32 rawBlock = fabsf(endBlock[coordDist] - startBlock[coordDist]);
	this->block = (f32)RoundFloatToInt(rawBlock);
	// Trace stops JS_OFFSET_EPSILON units in front of the actual block face; compensate
	this->edge = fabsf(startBlock[coordDist] - this->adjustedTakeoffOrigin[coordDist] + (16.0f - JS_OFFSET_EPSILON) * distSign);

	if (this->block < (f32)JS_MIN_BLOCK_DISTANCE)
	{
		this->block = 0.0f;
		this->edge = -1.0f;
	}
}

// ============================================================
// CalcLadderBlockStats - block stats for LadderJumps
// ============================================================

void Jump::CalcLadderBlockStats(Vector landingOrigin, bool checkOffset)
{
	i32 coordDist, distSign;
	GetCoordOrientation(landingOrigin, this->adjustedTakeoffOrigin, coordDist, distSign);
	i32 coordDev = 1 - coordDist;

	Vector ladderNormal = this->player->GetMoveServices()->m_vecLadderNormal();
	if (fabsf(fabsf(ladderNormal[coordDist]) - 1.0f) > JS_EPSILON)
	{
		return;
	}

	Vector ladderPosition = this->adjustedTakeoffOrigin;
	Vector endBlock = landingOrigin;
	endBlock.z -= 1.0f;
	ladderPosition.z = endBlock.z;

	Vector sweepMin = vec3_origin;
	Vector sweepMax = vec3_origin;
	sweepMin[coordDev] = -20.0f;
	sweepMax[coordDev] = 20.0f;

	Vector middle;
	middle[coordDist] = ladderPosition[coordDist] + (f32)distSign * (f32)JS_MIN_LAJ_BLOCK_DISTANCE;
	middle[coordDev] = endBlock[coordDev];
	middle.z = ladderPosition.z;

	if (!TraceHullPosition(this->player, ladderPosition, middle, sweepMin, sweepMax, ladderPosition))
	{
		return;
	}

	Vector blockPosition, blockNormal;
	endBlock[coordDist] += (f32)distSign * 16.0f;
	if (!TraceRayPositionNormal(this->player, middle, endBlock, blockPosition, blockNormal)
		|| fabsf(fabsf(blockNormal[coordDist]) - 1.0f) > JS_EPSILON)
	{
		return;
	}

	if (checkOffset)
	{
		Vector checkPos = blockPosition;
		checkPos.z += 1.0f;
		f32 blockSurfZ = FindBlockHeight(this->player, checkPos, (f32)distSign, coordDist, 1.0f) - JS_OFFSET_EPSILON;

		// TraceLadderOffset: verify the ladder top is at blockSurfZ
		Vector traceOrigin, traceEnd, ladderTop;
		traceOrigin.x = this->adjustedTakeoffOrigin.x - 10.0f * ladderNormal.x;
		traceOrigin.y = this->adjustedTakeoffOrigin.y - 10.0f * ladderNormal.y;
		traceOrigin.z = this->adjustedTakeoffOrigin.z + 5.0f;
		traceEnd = traceOrigin;
		traceEnd.z = this->adjustedTakeoffOrigin.z - 10.0f;

		if (!TraceHullPosition(this->player, traceOrigin, traceEnd, Vector(-20.0f, -20.0f, 0.0f), Vector(20.0f, 20.0f, 0.0f), ladderTop)
			|| fabsf(ladderTop.z - blockSurfZ) > JS_OFFSET_EPSILON)
		{
			return;
		}
	}

	this->block = (f32)RoundFloatToInt(fabsf(blockPosition[coordDist] - ladderPosition[coordDist]));
	this->edge = fabsf(this->adjustedTakeoffOrigin[coordDist] - ladderPosition[coordDist]) - 16.0f;

	if (this->block < (f32)JS_MIN_LAJ_BLOCK_DISTANCE)
	{
		this->block = 0.0f;
		this->edge = -1.0f;
	}
}

// ============================================================
// CalcAlwaysEdge - takeoff edge for always-on stats when block is 0
// ============================================================

void Jump::CalcAlwaysEdge()
{
	if (this->jumpType == JumpType_LadderJump)
	{
		// Ladder: edge = distance from player center to ladder surface
		Vector ladderNormal = this->player->GetMoveServices()->m_vecLadderNormal();
		i32 coordDist = fabsf(ladderNormal.x) > fabsf(ladderNormal.y) ? 0 : 1;

		Vector sweepMin = vec3_origin;
		Vector sweepMax = vec3_origin;
		sweepMin[1 - coordDist] = -20.0f;
		sweepMax[1 - coordDist] = 20.0f;

		Vector traceEnd = this->adjustedTakeoffOrigin;
		traceEnd[coordDist] += 27.0f;

		Vector hitPos;
		if (TraceHullPosition(this->player, this->adjustedTakeoffOrigin, traceEnd, sweepMin, sweepMax, hitPos))
		{
			this->edge = fabsf(hitPos[coordDist] - this->adjustedTakeoffOrigin[coordDist]) - 16.0f;
		}
	}
	else
	{
		// Standard: trace backward along takeoff velocity to find the block face
		Vector vel = this->takeoffVelocity;
		i32 coordDist = fabsf(vel.x) < fabsf(vel.y) ? 1 : 0;
		i32 distSign = vel[coordDist] > 0.0f ? 1 : -1;

		this->edge = -1.0f;

		// Sweep back from slightly behind the takeoff edge
		Vector traceEnd = this->adjustedTakeoffOrigin;
		traceEnd[coordDist] -= 16.0f * distSign;
		traceEnd.z -= 1.0f;

		Vector traceStart = traceEnd;
		traceStart[coordDist] += 20.0f * distSign;

		Vector hitPos;
		if (TraceRayPosition(this->player, traceStart, traceEnd, hitPos))
		{
			if (fabsf(hitPos[coordDist] - traceStart[coordDist]) > JS_EPSILON)
			{
				this->edge = fabsf(hitPos[coordDist] - this->adjustedTakeoffOrigin[coordDist] + (16.0f - JS_OFFSET_EPSILON) * distSign);
			}
		}
	}
}

// ============================================================
// AlwaysFailstat - always-on miss calculation, called at End()
// ============================================================

void Jump::AlwaysFailstat()
{
	if (this->jumpType == JumpType_FullInvalid || this->failstatValid)
	{
		return;
	}

	this->miss = 0.0f;

	// Compute takeoff edge if regular block detection failed
	if (this->block == 0.0f && this->jumpType != JumpType_LadderJump)
	{
		this->CalcAlwaysEdge();
	}

	// Determine jump direction from takeoff velocity
	Vector vel = this->takeoffVelocity;
	i32 coordDist = fabsf(vel.x) < fabsf(vel.y) ? 1 : 0;
	i32 distSign = vel[coordDist] > 0.0f ? 1 : -1;

	const Vector &currentPos = this->player->currentMoveData->m_vecAbsOrigin;

	// Search forward for the target block (tall trace box to find the block wall)
	Vector traceStart = currentPos;
	Vector traceEnd = currentPos;
	traceEnd[coordDist] += 100.0f * distSign;

	Vector traceLongMaxs = vec3_origin;
	traceLongMaxs.z = 200.0f;

	Vector tracePos;
	if (!TraceHullPosition(this->player, traceStart, traceEnd, vec3_origin, traceLongMaxs, tracePos))
	{
		return;
	}

	// Find the block surface Z and its horizontal face position
	Vector landingPos;
	bool foundHeight = false;
	tracePos.z = currentPos.z;

	if (this->jumpType == JumpType_LadderJump)
	{
		foundHeight = TryFindBlockHeight(this->player, tracePos, landingPos, coordDist, distSign);
		if (!foundHeight)
		{
			Vector traceShortMaxs = vec3_origin;
			traceShortMaxs.z = 54.0f;
			Vector tracePos2;
			if (TraceHullPosition(this->player, traceStart, traceEnd, vec3_origin, traceShortMaxs, tracePos2))
			{
				tracePos2.z = currentPos.z;
				foundHeight = TryFindBlockHeight(this->player, tracePos2, landingPos, coordDist, distSign);
			}
		}
		if (!foundHeight)
		{
			return;
		}
	}
	else
	{
		// Non-ladder: destination block is at the same Z as the takeoff floor
		foundHeight = TryFindBlockHeight(this->player, tracePos, landingPos, coordDist, distSign);
		if (!foundHeight)
		{
			Vector traceShortMaxs = vec3_origin;
			traceShortMaxs.z = 54.0f;
			Vector tracePos2;
			if (TraceHullPosition(this->player, traceStart, traceEnd, vec3_origin, traceShortMaxs, tracePos2))
			{
				tracePos2.z = currentPos.z;
				foundHeight = TryFindBlockHeight(this->player, tracePos2, landingPos, coordDist, distSign);
			}
		}
		if (!foundHeight)
		{
			return;
		}
		// Always use takeoff Z for non-ladder block height
		landingPos.z = this->adjustedTakeoffOrigin.z;
	}

	f32 referenceZ = landingPos.z;

	// Walk backward through pose history to find the last subtick the player was above referenceZ
	const i32 size = JS_FAILSTATS_MAX_TRACKED_TICKS;
	for (i32 i = 0; i < this->poseCount; i++)
	{
		i32 idx = ((this->poseIndex - i) % size + size) % size;
		if (this->poseHistory[idx].position.z >= referenceZ)
		{
			Vector failOrigin;
			if (!this->GetFailOrigin(referenceZ, failOrigin, -i))
			{
				break;
			}

			this->miss = fabsf(failOrigin[coordDist] - landingPos[coordDist]) - 16.0f;

			// Restore sync/BA/OL/DA from the historical snapshot at the crossing point
			const JumpPose &p = this->poseHistory[idx];
			this->airtime = p.airtime;
			this->badAngles = p.badAngles;
			this->overlap = p.overlap;
			this->deadAir = p.deadAir;
			this->sync = p.sync;
			break;
		}
	}
}
