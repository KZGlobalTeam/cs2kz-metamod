#include "kz_mode_vnl.h"
#include "utils/interfaces.h"
#include "sdk/usercmd.h"
#include "sdk/entity/cbasetrigger.h"

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
	// clang-format off
	if (jumpType == JumpType_Invalid
		|| jumpType == JumpType_FullInvalid
		|| jumpType == JumpType_Fall
		|| jumpType == JumpType_Other
		|| distance > 500.0f)
	// clang-format on
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

void KZVanillaModeService::Reset()
{
	this->airMoving = {};
	this->tpmTriggerFixOrigins.RemoveAll();
}

// 1:1 with CS2.
static_function void ClipVelocity(Vector &in, Vector &normal, Vector &out)
{
	f32 backoff = -((in.x * normal.x) + ((normal.z * in.z) + (in.y * normal.y))) * 1;
	backoff = fmaxf(backoff, 0.0) + 0.03125;
	out = normal * backoff + in;
}

void KZVanillaModeService::OnDuckPost()
{
	this->player->UpdateTriggerTouchList();
}

// We don't actually do anything here aside from predicting triggerfix.
#define MAX_CLIP_PLANES 5

static_function f32 QuantizeFloat(f32 value)
{
	constexpr const f32 offset = 196608.0f;

	return (value + offset) - offset;
}

void KZVanillaModeService::OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace)
{
	this->tpmTriggerFixOrigins.RemoveAll();

	Vector origin, oldVelocity;
	int bumpcount, numbumps;
	Vector dir;
	float d;
	int numplanes;
	Vector planes[MAX_CLIP_PLANES];
	Vector primal_velocity, original_velocity;
	Vector new_velocity;
	int i, j;
	trace_t pm;
	Vector end;

	float time_left, allFraction;

	numbumps = 4; // Bump up to four times

	numplanes = 0; //  and not sliding along any planes

	this->player->GetOrigin(&origin);
	Vector &velocity = this->player->currentMoveData->m_vecVelocity;
	this->player->GetVelocity(&oldVelocity);
	g_pKZUtils->InitGameTrace(&pm);

	VectorCopy(velocity, original_velocity); // Store original velocity
	VectorCopy(velocity, primal_velocity);

	allFraction = 0;
	time_left = g_pKZUtils->GetGlobals()->frametime; // Total time for this movement operation.

	new_velocity.Init();
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);

	CTraceFilterPlayerMovementCS filter;
	g_pKZUtils->InitPlayerMovementTraceFilter(filter, this->player->GetPlayerPawn(),
											  this->player->GetPlayerPawn()->m_pCollision()->m_collisionAttribute().m_nInteractsWith,
											  COLLISION_GROUP_PLAYER_MOVEMENT);

	f32 originalError = this->player->GetMoveServices()->m_flAccumulatedJumpError();
	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (velocity.Length() == 0.0)
		{
			break;
		}
		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA(origin, time_left, velocity, end);
		if ((this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND) == 0 && time_left != 0.0f)
		{
			f32 zMoveDistance = time_left * velocity.z;
			f32 preciseZMoveDistance = zMoveDistance + this->player->GetMoveServices()->m_flAccumulatedJumpError;
			f32 quantizedMoveDistance = QuantizeFloat(zMoveDistance);
			end.z = QuantizeFloat(preciseZMoveDistance + origin.z);
			f32 zFinal = preciseZMoveDistance - quantizedMoveDistance;
			quantizedMoveDistance += 199608.0f;
			zFinal += quantizedMoveDistance;
			zFinal += origin.z;
			zFinal -= 199608.0f;
			end.z = zFinal;
			this->player->GetMoveServices()->m_flAccumulatedJumpError = preciseZMoveDistance - (end.z - origin.z);
		}
		// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
		if (pFirstDest && (end == *pFirstDest))
		{
			pm = *pFirstTrace;
		}
		else
		{
			g_pKZUtils->TracePlayerBBox(origin, end, bounds, &filter, pm);
		}
		if (allFraction == 0)
		{
			if (pm.m_flFraction < 1 && time_left * velocity.Length() >= 0.03125 && pm.m_flFraction * velocity.Length() * time_left < 0.03125)
			{
				pm.m_flFraction = 0;
			}
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and
		//  zero the plane counter.
		if (pm.m_flFraction * MAX(1.0, velocity.Length()) > 0.03125)
		{
			// actually covered some distance
			origin = (pm.m_vEndPos);
			VectorCopy(velocity, original_velocity);
		}

		allFraction += pm.m_flFraction;
		// If we covered the entire distance, we are done
		//  and can return.

		// Triggerfix related
		this->tpmTriggerFixOrigins.AddToTail(pm.m_vEndPos);

		if (pm.m_flFraction == 1)
		{
			break; // moved the entire distance
		}
		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= pm.m_flFraction * time_left;

		// Did we run out of planes to clip against?
		// 2024-11-07 update also adds a low velocity check... This is only correct as long as you don't collide with other players.
		f32 standableZ = reinterpret_cast<CVValue_t *>(&(KZ::mode::modeCvars[MODECVAR_SV_STANDABLE_NORMAL]->values))->m_flValue;
		if (numplanes >= MAX_CLIP_PLANES || (pm.m_vHitNormal.z >= standableZ && velocity.Length2D() < 1.0f))
		{
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy(vec3_origin, velocity);
			break;
		}

		// Set up next clipping plane
		VectorCopy(pm.m_vHitNormal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping
		// in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if (numplanes == 1 && this->player->GetPlayerPawn()->m_MoveType() == MOVETYPE_WALK
			&& this->player->GetPlayerPawn()->m_hGroundEntity().Get() == NULL)
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
						{
							break; // not ok
						}
					}
				}

				if (j == numplanes) // Didn't have to clip, so we're ok
				{
					break;
				}
			}

			// Did we go all the way through plane set
			if (i == numplanes)
			{ // go along the crease
				if (numplanes != 2)
				{
					VectorCopy(vec3_origin, velocity);
					break;
				}
				CrossProduct(planes[0], planes[1], dir);
				dir.NormalizeInPlace();
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
				// Con_DPrintf("Back\n");
				VectorCopy(vec3_origin, velocity);
				break;
			}
		}
	}
	if (allFraction == 0)
	{
		VectorCopy(vec3_origin, velocity);
	}
	this->player->GetMoveServices()->m_flAccumulatedJumpError = originalError;
	this->player->currentMoveData->m_vecVelocity = oldVelocity;
}

void KZVanillaModeService::OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace)
{
	if (this->airMoving)
	{
		if (this->tpmTriggerFixOrigins.Count() > 1)
		{
			bbox_t bounds;
			this->player->GetBBoxBounds(&bounds);
			for (int i = 0; i < this->tpmTriggerFixOrigins.Count() - 1; i++)
			{
				this->player->TouchTriggersAlongPath(this->tpmTriggerFixOrigins[i], this->tpmTriggerFixOrigins[i + 1], bounds);
			}
		}
		this->player->UpdateTriggerTouchList();
	}
}

void KZVanillaModeService::OnAirMove()
{
	this->airMoving = true;
}

void KZVanillaModeService::OnAirMovePost()
{
	this->airMoving = false;
}

// Only touch timer triggers on full ticks.
bool KZVanillaModeService::OnTriggerStartTouch(CBaseTrigger *trigger)
{
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	f32 time = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_RATE;
	if (fabs(roundf(time) - time) < 0.001f)
	{
		return true;
	}

	return false;
}

bool KZVanillaModeService::OnTriggerTouch(CBaseTrigger *trigger)
{
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	f32 time = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_RATE;
	if (fabs(roundf(time) - time) < 0.001f)
	{
		return true;
	}
	return false;
}

bool KZVanillaModeService::OnTriggerEndTouch(CBaseTrigger *trigger)
{
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	f32 time = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_RATE;
	if (fabs(roundf(time) - time) < 0.001f)
	{
		return true;
	}
	return false;
}

void KZVanillaModeService::OnSetupMove(PlayerCommand *pc)
{
	// We make subtick inputs "less subticky" so that float precision error doesn't impact jump height (or at least minimalize it)
	auto subtickMoves = pc->mutable_base()->mutable_subtick_moves();
	f64 frameTime = 0;
	for (i32 j = 0; j < pc->mutable_base()->subtick_moves_size(); j++)
	{
		CSubtickMoveStep *subtickMove = pc->mutable_base()->mutable_subtick_moves(j);
		frameTime = subtickMove->when() * ENGINE_FIXED_TICK_INTERVAL;
		f32 approxOffsetCurtime = g_pKZUtils->GetGlobals()->curtime - frameTime + ENGINE_FIXED_TICK_INTERVAL;
		f64 exactCurtime = g_pKZUtils->GetGlobals()->curtime - frameTime + ENGINE_FIXED_TICK_INTERVAL;
		f64 precisionLoss = approxOffsetCurtime - exactCurtime;
		subtickMove->set_when(subtickMove->when() - precisionLoss / ENGINE_FIXED_TICK_INTERVAL);
	}
}

void KZVanillaModeService::OnProcessMovementPost()
{
	this->player->UpdateTriggerTouchList();
}

// Only happens when triggerfix happens.
void KZVanillaModeService::OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
{
	if (!this->player->processingMovement)
	{
		return;
	}
	if (newPosition)
	{
		this->player->currentMoveData->m_vecAbsOrigin = *newPosition;
	}
	if (newVelocity)
	{
		this->player->currentMoveData->m_vecVelocity = *newVelocity;
	}
}

void KZVanillaModeService::OnStartTouchGround()
{
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);
	Vector ground = this->player->landingOrigin;
	ground.z = this->player->GetGroundPosition() - 0.03125f;
	this->player->TouchTriggersAlongPath(this->player->landingOrigin, ground, bounds);
}
