#include "movement.h"
#include "utils/utils.h"
#include "utils/detours.h"
#include "tier0/memdbgon.h"

void MovementPlayer::OnProcessMovement()
{
	this->duckBugged = false;
	this->processingMovement = true;
	this->walkMoved = false;
	this->takeoffFromLadder = false;
	this->collidingWithWorld = false;

	bool onGround = this->GetPlayerPawn()->m_fFlags() & FL_ONGROUND;
	if (!this->previousOnGround && onGround)
	{
		Vector velocity;
		this->GetVelocity(&velocity);
		this->RegisterLanding(velocity, false);
		this->OnStartTouchGround();
	}
	else if (this->previousOnGround && !onGround)
	{
		this->RegisterTakeoff(false);
		this->takeoffFromLadder = false;
		this->OnStopTouchGround();
	}
}

void MovementPlayer::OnProcessMovementPost()
{
	// On ground or a ladder, definitely not in a perf.
	if (this->GetPlayerPawn()->m_fFlags() & FL_ONGROUND || this->GetMoveType() != MOVETYPE_WALK)
	{
		this->inRealPerf = false;
		this->inPerf = false;
	}
	this->processingMovement = false;
	if (g_pKZUtils->GetGlobals()->frametime > 0.0f)
	{
		this->oldAngles = this->moveDataPost.m_vecViewAngles;
	}
	this->oldWalkMoved = this->walkMoved;
	this->previousOnGround = this->GetPlayerPawn()->m_fFlags() & FL_ONGROUND;
}

CCSPlayer_MovementServices *MovementPlayer::GetMoveServices()
{
	if (!this->GetPlayerPawn())
	{
		return nullptr;
	}
	return static_cast<CCSPlayer_MovementServices *>(this->GetPlayerPawn()->m_pMovementServices());
};

void MovementPlayer::GetOrigin(Vector *origin)
{
	if (this->processingMovement && this->currentMoveData)
	{
		*origin = this->currentMoveData->m_vecAbsOrigin;
	}
	else
	{
		CBasePlayerPawn *pawn = this->GetPlayerPawn();
		if (!pawn)
		{
			return;
		}
		*origin = pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	}
}

void MovementPlayer::Teleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	CBasePlayerPawn *pawn = this->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	// We handle angles differently.
	this->SetAngles(*angles);
	pawn->Teleport(origin, NULL, velocity);
}

void MovementPlayer::SetOrigin(const Vector &origin)
{
	if (this->processingMovement && this->currentMoveData)
	{
		this->currentMoveData->m_vecAbsOrigin = origin;
	}
	else
	{
		CBasePlayerPawn *pawn = this->GetPlayerPawn();
		if (!pawn)
		{
			return;
		}
		pawn->Teleport(&origin, NULL, NULL);
	}
	// Origin teleported, remove all errors.
	this->GetMoveServices()->m_flAccumulatedJumpError() = 0;
}

void MovementPlayer::GetVelocity(Vector *velocity)
{
	if (this->processingMovement && this->currentMoveData)
	{
		*velocity = this->currentMoveData->m_vecVelocity;
	}
	else
	{
		CBasePlayerPawn *pawn = this->GetPlayerPawn();
		if (!pawn)
		{
			return;
		}
		*velocity = pawn->m_vecAbsVelocity();
	}
}

void MovementPlayer::SetVelocity(const Vector &velocity)
{
	if (this->processingMovement && this->currentMoveData)
	{
		this->currentMoveData->m_vecVelocity = velocity;
	}
	else
	{
		CBasePlayerPawn *pawn = this->GetPlayerPawn();
		if (!pawn)
		{
			return;
		}
		pawn->Teleport(NULL, NULL, &velocity);
	}
}

void MovementPlayer::GetAngles(QAngle *angles)
{
	if (this->processingMovement && this->currentMoveData)
	{
		*angles = this->currentMoveData->m_vecViewAngles;
	}
	else
	{
		*angles = this->moveDataPost.m_vecViewAngles;
	}
}

void MovementPlayer::SetAngles(const QAngle &angles)
{
	CBasePlayerPawn *pawn = this->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}

	// Don't change the pitch of the absolute angles because it messes with the player model.
	QAngle absAngles = angles;
	absAngles.x = 0;

	pawn->Teleport(NULL, &absAngles, NULL);
	g_pKZUtils->SnapViewAngles(pawn, angles);
}

TurnState MovementPlayer::GetTurning()
{
	QAngle currentAngle;
	if (this->processingMovement && this->currentMoveData)
	{
		currentAngle = this->currentMoveData->m_vecViewAngles;
	}
	else
	{
		currentAngle = this->moveDataPre.m_vecViewAngles;
	}
	bool turning = this->oldAngles.y != currentAngle.y;
	if (!turning)
	{
		return TURN_NONE;
	}
	if (currentAngle.y < this->oldAngles.y - 180 || (currentAngle.y > this->oldAngles.y && currentAngle.y < this->oldAngles.y + 180))
	{
		return TURN_LEFT;
	}
	return TURN_RIGHT;
}

bool MovementPlayer::IsButtonPressed(InputBitMask_t button, bool onlyDown)
{
	CCSPlayer_MovementServices *ms = this->GetMoveServices();
	if (!ms)
	{
		return false;
	}
	return ms->m_nButtons()->IsButtonPressed(button, onlyDown);
}

f32 MovementPlayer::GetGroundPosition()
{
	CMoveData *mv = this->currentMoveData;
	if (!this->processingMovement)
	{
		mv = &this->moveDataPost;
	}

	CTraceFilterPlayerMovementCS filter;
	g_pKZUtils->InitPlayerMovementTraceFilter(filter, this->GetPlayerPawn(),
											  this->GetPlayerPawn()->m_Collision().m_collisionAttribute().m_nInteractsWith(),
											  COLLISION_GROUP_PLAYER_MOVEMENT);

	Vector ground = mv->m_vecAbsOrigin;
	ground.z -= 2;

	f32 standableZ = 0.7;

	static_persist ConVar *sv_standable_normal;
	if (!sv_standable_normal)
	{
		ConVarHandle cvarHandle = g_pCVar->FindConVar("sv_standable_normal");
		if (cvarHandle.IsValid())
		{
			sv_standable_normal = g_pCVar->GetConVar(cvarHandle);
		}
	}
	if (sv_standable_normal)
	{
		standableZ = reinterpret_cast<CVValue_t *>(&sv_standable_normal->values)->m_flValue;
	}

	bbox_t bounds;
	bounds.mins = {-16.0, -16.0, 0.0};
	bounds.maxs = {16.0, 16.0, 72.0};

	if (this->GetMoveServices()->m_bDucked())
	{
		bounds.maxs.z = 54.0;
	}

	trace_t trace;
	g_pKZUtils->InitGameTrace(&trace);

	g_pKZUtils->TracePlayerBBox(mv->m_vecAbsOrigin, ground, bounds, &filter, trace);

	// Doesn't hit anything, fall back to the original ground
	if (trace.m_bStartInSolid || trace.m_flFraction == 1.0f)
	{
		return mv->m_vecAbsOrigin.z;
	}

	return trace.m_vEndPos.z;
}

void MovementPlayer::RegisterTakeoff(bool jumped)
{
	CMoveData *mv = this->currentMoveData;
	if (!this->processingMovement)
	{
		mv = &this->moveDataPost;
	}
	this->takeoffOrigin = mv->m_vecAbsOrigin;
	this->takeoffTime = g_pKZUtils->GetGlobals()->curtime - g_pKZUtils->GetGlobals()->frametime;
	this->takeoffVelocity = mv->m_vecVelocity;
	this->takeoffGroundOrigin = mv->m_vecAbsOrigin;
	this->takeoffGroundOrigin.z = this->GetGroundPosition();
	this->inRealPerf = this->inPerf;
	this->jumped = jumped;
}

void MovementPlayer::RegisterLanding(const Vector &landingVelocity, bool distbugFix)
{
	CMoveData *mv = this->currentMoveData;
	if (!this->processingMovement)
	{
		mv = &this->moveDataPost;
	}
	this->inPerf = false;
	this->inRealPerf = false;
	this->landingOrigin = mv->m_vecAbsOrigin;
	this->landingTime = g_pKZUtils->GetGlobals()->curtime;
	this->landingTimeServer = g_pKZUtils->GetServerGlobals()->curtime;
	this->landingVelocity = landingVelocity;
	if (!distbugFix)
	{
		this->landingOriginActual = this->landingOrigin;
		this->landingTimeActual = this->landingTime;
	}
	// Distbug shenanigans
	if (mv->m_TouchList.Count() > 0) // bugged
	{
		// The true landing origin from TryPlayerMove, use this whenever you can
		f32 normal = 0.7;

		static_persist ConVar *sv_walkable_normal;
		if (!sv_walkable_normal)
		{
			ConVarHandle cvarHandle = g_pCVar->FindConVar("sv_walkable_normal");
			if (cvarHandle.IsValid())
			{
				sv_walkable_normal = g_pCVar->GetConVar(cvarHandle);
			}
		}
		if (sv_walkable_normal)
		{
			normal = reinterpret_cast<CVValue_t *>(&sv_walkable_normal->values)->m_flValue;
		}

		FOR_EACH_VEC(mv->m_TouchList, i)
		{
			if (mv->m_TouchList[i].trace.m_vHitNormal.z > normal)
			{
				this->landingOriginActual = mv->m_TouchList[i].trace.m_vEndPos;
				this->landingTimeActual =
					this->landingTime
					- (1 - mv->m_TouchList[i].trace.m_flFraction) * g_pKZUtils->GetGlobals()->frametime; // TODO: make sure this is right
				return;
			}
		}
	}
	// reverse bugged
	f32 diffZ = mv->m_vecAbsOrigin.z - this->GetGroundPosition();
	if (diffZ <= 0) // Ledgegrabbed, just use the current origin.
	{
		this->landingOriginActual = mv->m_vecAbsOrigin;
		this->landingTimeActual = this->landingTime;
	}
	else
	{
		// Predicts the landing origin if reverse bug happens
		// Doesn't match the theoretical values for probably floating point limitation reasons, but it's good enough
		static_persist ConVar *sv_gravity;
		if (!sv_gravity)
		{
			ConVarHandle cvarHandle = g_pCVar->FindConVar("sv_gravity");
			if (cvarHandle.IsValid())
			{
				sv_gravity = g_pCVar->GetConVar(cvarHandle);
			}
		}
		Vector gravity = {0, 0, -800};
		if (sv_gravity)
		{
			gravity.z = reinterpret_cast<CVValue_t *>(&sv_gravity->values)->m_flValue;
		}
		// basic x + vt + (0.5a)t^2 = 0;
		const double delta = landingVelocity.z * landingVelocity.z - 2 * gravity.z * diffZ;
		const double time = (-landingVelocity.z - sqrt(delta)) / (gravity.z);
		this->landingOriginActual = mv->m_vecAbsOrigin + landingVelocity * time + 0.5 * gravity * time * time;
		this->landingTimeActual = this->landingTime + time;
	}
}

void MovementPlayer::OnPostThink() {}

void MovementPlayer::SetMoveType(MoveType_t newMoveType, bool fireCallback)
{
	MoveType_t oldMoveType = this->GetMoveType();
	if (oldMoveType != newMoveType)
	{
		this->GetPlayerPawn()->SetMoveType(newMoveType);
		if (fireCallback)
		{
			this->OnChangeMoveType(oldMoveType);
		}
	}
}

bool MovementPlayer::IsPerfing(bool framePerfect)
{
	return framePerfect ? this->inRealPerf : this->inPerf;
}

void MovementPlayer::Reset()
{
	Player::Reset();
	this->processingDuck = false;
	this->duckBugged = false;
	this->walkMoved = false;
	this->oldWalkMoved = false;
	this->inRealPerf = false;
	this->inPerf = false;
	this->jumped = false;
	this->takeoffFromLadder = false;
	this->lastValidLadderOrigin.Init();
	this->takeoffOrigin.Init();
	this->takeoffVelocity.Init();
	this->takeoffTime = 0.0f;
	this->takeoffGroundOrigin.Init();
	this->landingOrigin.Init();
	this->landingVelocity.Init();
	this->landingTime = 0.0f;
	this->landingTimeServer = 0.0f;
	this->landingOriginActual.Init();
	this->landingTimeActual = 0.0f;
	this->collidingWithWorld = false;
	this->enableWaterFix = false;
	this->ignoreNextCategorizePosition = false;
	this->pendingStartTouchTriggers.RemoveAll();
	this->pendingEndTouchTriggers.RemoveAll();
	this->touchedTriggers.RemoveAll();
	this->collidingWithWorld = false;
	this->previousOnGround = false;
}

META_RES MovementPlayer::GetPlayerMaxSpeed(f32 &maxSpeed)
{
	return MRES_IGNORED;
}

void MovementPlayer::GetBBoxBounds(bbox_t *bounds)
{
	bounds->mins = {-16.0f, -16.0f, 0.0f};
	bounds->maxs = {16.0f, 16.0f, 72.0f};
	if (this->GetMoveServices() && this->GetMoveServices()->m_bDucked())
	{
		bounds->maxs.z = 54.0f;
	}
}

void MovementPlayer::OnPhysicsSimulate()
{
	if (this->GetMoveType() != this->lastKnownMoveType)
	{
		this->OnChangeMoveType(this->lastKnownMoveType);
	}
}

void MovementPlayer::OnPhysicsSimulatePost()
{
	this->lastKnownMoveType = this->GetMoveType();
}
