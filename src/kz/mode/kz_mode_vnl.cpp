#include "kz_mode_vnl.h"
#include "utils/interfaces.h"
#include "sdk/usercmd.h"
#include "sdk/tracefilter.h"
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

const CVValue_t *KZVanillaModeService::GetModeConVarValues()
{
	return modeCvarValues;
}

void KZVanillaModeService::Reset()
{
	this->airMoving = {};
	this->tpmTriggerFixOrigins.RemoveAll();
}

// 1:1 with CS2.
static_function void ClipVelocity(Vector &in, Vector &normal, Vector &out, float overbounce)
{
	f32 backoff = -(((normal.y * in.y) + (normal.z * in.z)) + (in.x * normal.x)) * overbounce;
	backoff = fmaxf(backoff, 0.0) + 0.03125;
	out = normal * backoff + in;
}

void KZVanillaModeService::OnDuckPost()
{
	this->player->UpdateTriggerTouchList();
}

// We don't actually do anything here aside from predicting triggerfix.
#define MAX_CLIP_PLANES 5

void KZVanillaModeService::OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing)
{
	this->tpmTriggerFixOrigins.RemoveAll();

	Vector origin = this->player->currentMoveData->m_vecAbsOrigin;
	Vector velocity = this->player->currentMoveData->m_vecVelocity;
	float jumpError = this->player->GetMoveServices()->m_flAccumulatedJumpError();
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
	bool inAir;

	numbumps = 4;  // Bump up to four times
	numplanes = 0; //  and not sliding along any planes

#if 0
	if (bIsSurfing)
	{
		*bIsSurfing = false;
	}
#endif

	// Check if player is in air
	inAir = (this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND) == 0;

	VectorCopy(velocity, original_velocity); // Store original velocity
	VectorCopy(velocity, primal_velocity);

	allFraction = 0;
	time_left = g_pKZUtils->GetGlobals()->frametime; // Total time for this movement operation.

	new_velocity.Init();
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);

	CTraceFilterPlayerMovementCS filter(this->player->GetPlayerPawn());
#if 0
	if (this->player->IsFakeClient())
	{
		filter.EnableInteractsWithLayer(LAYER_INDEX_CONTENTS_NPC_CLIP);
	}
#endif

#if 0 
	// AG2 Update: Kill player speed if someone is standing on top of them
	// Note that this function is only called when the player might collide with something while on the ground.
	if (this->player->GetPlayerPawn()->m_hGroundEntity().Get() != NULL)
	{
		if (IsStoodOn(this->player->GetPlayerPawn()))
		{
			velocity.Init();
		}
	}
#endif
	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (velocity.Length() == 0.0)
		{
			break;
		}

		if (/*enableJumpPrecision && */ inAir && time_left != 0.0f)
		{
			float initialZDistance = time_left * velocity.z;
			float desiredZDistance = initialZDistance + jumpError;

			end.x = (time_left * velocity.x) + origin.x;
			end.y = (time_left * velocity.y) + origin.y;

			float finalZPosition =
				(((desiredZDistance - ((initialZDistance + 196608.0f) - 196608.0f)) + (((initialZDistance + 196608.0f) - 196608.0f) + 196608.0f))
				 + origin.z)
				- 196608.0f;
			end.z = finalZPosition;

			jumpError = desiredZDistance - (finalZPosition - origin.z);
		}
		else
		{
			VectorMA(origin, time_left, velocity, end);
		}

		// AG2 Update: Fixed several cases where a player would get stuck on map geometry while surfing
		if (numplanes == 1)
		{
			VectorMA(end, 0.03125f, planes[0], end);
		}

		// See if we can make it from origin to end point.
		// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
		if (pFirstDest && (end == *pFirstDest))
		{
			pm = *pFirstTrace;
		}
		else
		{
#if 0
			this->player->GetMoveServices()->m_nTraceCount++;
#endif
			g_pKZUtils->TracePlayerBBox(origin, end, bounds, &filter, pm);
		}

		if (allFraction == 0.0f)
		{
			if (pm.m_flFraction < 1.0f && velocity.Length() * time_left >= 0.03125f && (pm.m_flFraction * velocity.Length() * time_left) < 0.03125f)
			{
				pm.m_flFraction = 0.0f;
			}
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and
		//  zero the plane counter.
		if (pm.m_flFraction * MAX(1.0, velocity.Length()) > 0.03125f)
		{
			// actually covered some distance
			origin = pm.m_vEndPos;
			VectorCopy(velocity, original_velocity);
			numplanes = 0;
		}

		allFraction += pm.m_flFraction;

		// Triggerfix related
		this->tpmTriggerFixOrigins.AddToTail(pm.m_vEndPos);

		if (pm.m_flFraction == 1.0f)
		{
			break; // moved the entire distance
		}

#if 0
		AddToTouched(this->player->currentMoveData, pm, velocity);
#endif
		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= pm.m_flFraction * time_left;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy(vec3_origin, velocity);
			// Con_DPrintf("Too many planes 4\n");

			break;
		}

		// 2024-11-07 update also adds a low velocity check... This is only correct as long as you don't collide with other players.
		if (inAir && velocity.z < 0.0f)
		{
			float standableZ = KZ::mode::modeCvarRefs[MODECVAR_SV_STANDABLE_NORMAL]->GetFloat();
			if (pm.m_vHitNormal.z >= standableZ /*&& this->player->GetMoveServices()->CanStandOn(pm)*/ && velocity.Length2D() < 1.0f)
			{
				velocity.Init();
				break;
			}
		}

		// Set up next clipping plane
		VectorCopy(pm.m_vHitNormal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if (numplanes == 1)
		{
			float sv_bounce = KZ::mode::modeCvarRefs[MODECVAR_SV_BOUNCE]->GetFloat();
			float walkableZ = KZ::mode::modeCvarRefs[MODECVAR_SV_WALKABLE_NORMAL]->GetFloat();
			for (i = 0; i < numplanes; i++)
			{
				if (planes[i][2] > walkableZ)
				{
					// floor or slope
					ClipVelocity(original_velocity, planes[i], new_velocity, 1);
					VectorCopy(new_velocity, original_velocity);
				}
				else
				{
					ClipVelocity(original_velocity, planes[i], new_velocity,
								 1.0 + sv_bounce * (1 - this->player->GetMoveServices()->m_flSurfaceFriction()));

#if 0
					if (bIsSurfing)
					{
						*bIsSurfing = true;
					}
#endif
				}
			}

			VectorCopy(new_velocity, velocity);
			VectorCopy(new_velocity, original_velocity);
		}
		else
		{
			for (i = 0; i < numplanes; i++)
			{
				ClipVelocity(original_velocity, planes[i], velocity, 1);

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
			if (velocity.Dot(primal_velocity) <= 0.0f)
			{
				VectorCopy(vec3_origin, velocity);
				break;
			}
		}
	}

	if (allFraction == 0 && numplanes > 0)
	{
		VectorCopy(vec3_origin, velocity);
	}

#if 0
	float primalSpeed2D = primal_velocity.Length2D();
	float currentSpeed2D = velocity.Length2D();
	float lateralStoppingAmount = primalSpeed2D - currentSpeed2D;
	
	if (lateralStoppingAmount > 1160.0f)
	{
		PlayerLandingRoughEffect(this->player, 1.0f);
	}
	else if (lateralStoppingAmount > 580.0f)
	{
		PlayerLandingRoughEffect(this->player, 0.85f);
	}
	this->player->currentMoveData->m_vecVelocity = velocity;
#endif
}

void KZVanillaModeService::OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing)
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

void KZVanillaModeService::OnPlayerMove()
{
	this->player->currentMoveData->m_flMaxSpeed = MIN(this->player->currentMoveData->m_flMaxSpeed, 250.0f);
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
