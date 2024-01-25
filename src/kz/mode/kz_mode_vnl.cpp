#include "kz_mode_vnl.h"
#include "utils/interfaces.h"

const char *KZVanillaModeService::GetModeName()
{
	return "Vanilla";
}
const char *KZVanillaModeService::GetModeShortName()
{
	return "VNL";
}

DistanceTier KZVanillaModeService::GetDistanceTier(JumpType jumpType, f32 distance)
{
	// No tiers given for 'Invalid' jumps.
	if (jumpType == JumpType_Invalid || jumpType == JumpType_FullInvalid
		|| jumpType == JumpType_Fall || jumpType == JumpType_Other
		|| distance > 500.0f)
	{
		return DistanceTier_None;
	}

	// Get highest tier distance that the jump beats
	DistanceTier tier = DistanceTier_None;
	while (tier + 1 < DISTANCETIER_COUNT && distance >= distanceTiers[jumpType][tier])
	{
		tier = (DistanceTier)(tier + 1);
	}

	return tier;
}

const char **KZVanillaModeService::GetModeConVarValues()
{
	return modeCvarValues;
}

internal void ClipVelocity(Vector &in, Vector &normal, Vector &out)
{
	// Determine how far along plane to slide based on incoming direction.
	f32 backoff = DotProduct(in, normal);

	for (i32 i = 0; i < 3; i++)
	{
		f32 change = normal[i] * backoff;
		out[i] = in[i] - change;
	}

	// Rampbug/wallbug fix: always move a little bit away from the plane
	float adjust = -0.0078125f;
	out -= (normal * adjust);
}

// We don't actually do anything here aside from seeing if we collided with anything.
#define MAX_CLIP_PLANES 5
void KZVanillaModeService::OnTryPlayerMove(Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	Vector velocity, origin;
	int			bumpcount, numbumps;
	Vector		dir;
	float		d;
	int			numplanes;
	Vector		planes[MAX_CLIP_PLANES];
	Vector		primal_velocity, original_velocity;
	Vector      new_velocity;
	int			i, j;
	trace_t_s2	pm;
	Vector		end;
	bool valid = true;
	float		time, time_left, allFraction, stuckFrametime, traceFraction;

	numbumps = 4;           // Bump up to four times

	numplanes = 0;           //  and not sliding along any planes

	this->player->GetOrigin(&origin);
	this->player->GetVelocity(&velocity);
	g_pKZUtils->InitGameTrace(&pm);

	VectorCopy(velocity, original_velocity);  // Store original velocity
	VectorCopy(velocity, primal_velocity);

	allFraction = 0;
	time_left = g_pKZUtils->GetServerGlobals()->frametime;   // Total time for this movement operation.

	new_velocity.Init();
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);

	CTraceFilterPlayerMovementCS filter;
	g_pKZUtils->InitPlayerMovementTraceFilter(filter, this->player->GetPawn(), this->player->GetPawn()->m_pCollision()->m_collisionAttribute().m_nInteractsWith, COLLISION_GROUP_PLAYER_MOVEMENT);

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (velocity.Length() == 0.0)
			break;
		if (valid)
		{
			time = time_left;
		}
		else
		{
			time = stuckFrametime;
		}
		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA(origin, time, velocity, end);

		// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
		if (pFirstDest && (end == *pFirstDest))
		{
			pm = *pFirstTrace;
		}
		else
		{
			g_pKZUtils->TracePlayerBBox(origin, end, bounds, &filter, pm);
		}

		allFraction += pm.fraction;

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and 
		//  zero the plane counter.
		if (pm.fraction > 0.03125)
		{
			// There's a precision issue with terrain tracing that can cause a swept box to successfully trace
			// when the end position is stuck in the triangle.  Re-run the test with an uswept box to catch that
			// case until the bug is fixed.
			// If we detect getting stuck, don't allow the movement
			trace_t_s2 stuck;
			g_pKZUtils->InitGameTrace(&stuck);
			g_pKZUtils->TracePlayerBBox(pm.endpos, pm.endpos, bounds, &filter, stuck);
			if (stuck.startsolid || stuck.fraction != 1.0f)
			{
				valid = false;
				stuckFrametime = time_left * stuck.fraction * 0.9f;
				VectorCopy(vec3_origin, velocity);
				continue;
			}
			// actually covered some distance
			origin = (pm.endpos);
			VectorCopy(velocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		
		// AddToTouched

		if (pm.fraction == 1)
		{
			break;		// moved the entire distance
		}

		
		if (valid)
		{
			traceFraction = pm.fraction * time_left;
		}
		else
		{
			valid = true;
			traceFraction = pm.fraction * stuckFrametime;
		}
		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= traceFraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy(vec3_origin, velocity);
			break;
		}

		// Set up next clipping plane
		VectorCopy(pm.planeNormal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity 
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if (numplanes == 1 &&
			this->player->GetPawn()->m_MoveType() == MOVETYPE_WALK &&
			this->player->GetPawn()->m_hGroundEntity().Get() == NULL)
		{
			ClipVelocity(original_velocity, planes[0], new_velocity);

			VectorCopy(new_velocity, velocity);
			VectorCopy(new_velocity, original_velocity);
		}
		else
		{
			for (i = 0; i < numplanes; i++)
			{
				ClipVelocity(original_velocity, planes[i], velocity);

				for (j = 0; j < numplanes; j++)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (velocity.Dot(planes[j]) < 0)
							break;	// not ok
					}
				}

				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}

			// Did we go all the way through plane set
			if (i == numplanes)
			{	// go along the crease
				if (numplanes != 2)
				{
					VectorCopy(vec3_origin, velocity);
					break;
				}
				CrossProduct(planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(velocity);
				VectorScale(dir, d, velocity);
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d = velocity.Dot(primal_velocity);
			if (d <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy(vec3_origin, velocity);
				break;
			}
		}
	}

	if (allFraction == 0)
	{
		VectorCopy(vec3_origin, velocity);
	}
}
