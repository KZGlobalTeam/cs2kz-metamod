#include "movement.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

void MovementPlayer::OnStartProcessMovement()
{
	this->duckBugged = false;
	this->hitPerf = false;
	this->processingMovement = true;
	this->walkMoved = false;
}

void MovementPlayer::OnStopProcessMovement()
{
	this->processingMovement = false;
	this->lastProcessedCurtime = utils::GetServerGlobals()->curtime;
	this->lastProcessedTickcount = utils::GetServerGlobals()->tickcount;
	this->oldAngles = this->moveDataPost.m_vecViewAngles;
	this->oldWalkMoved = this->walkMoved;
}

void MovementPlayer::OnStartTouchGround()
{
}

void MovementPlayer::OnStopTouchGround()
{
}

void MovementPlayer::OnChangeMoveType(MoveType_t oldMoveType)
{
}

void MovementPlayer::OnAirAcceleratePre(Vector &wishdir, f32 &wishspeed, f32 &accel)
{
}

void MovementPlayer::OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel)
{
}

CCSPlayerController *MovementPlayer::GetController()
{
	return dynamic_cast<CCSPlayerController *>(g_pEntitySystem->GetBaseEntity(CEntityIndex(this->index)));
}

CCSPlayerPawn *MovementPlayer::GetPawn()
{
	CCSPlayerController *controller = this->GetController();
	if (!controller) return nullptr;
	return dynamic_cast<CCSPlayerPawn *>(controller->m_hPawn().Get());
}

CCSPlayer_MovementServices *MovementPlayer::GetMoveServices()
{
	return static_cast<CCSPlayer_MovementServices *>(this->GetPawn()->m_pMovementServices());
};

void MovementPlayer::GetOrigin(Vector *origin)
{
	if (this->processingMovement && this->currentMoveData)
	{
		*origin = this->currentMoveData->m_vecAbsOrigin;
	}
	else
	{
		CBasePlayerPawn *pawn = this->GetPawn();
		if (!pawn) return;

		*origin = pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	}
}

void MovementPlayer::Teleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	// We handle angles differently.
	this->SetAngles(*angles);
	CALL_VIRTUAL(void, offsets::Teleport, pawn, origin, NULL, velocity);
}

void MovementPlayer::SetOrigin(const Vector &origin)
{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	CALL_VIRTUAL(void, offsets::Teleport, pawn, &origin, NULL, NULL);
}

void MovementPlayer::GetVelocity(Vector *velocity)
{
	if (this->processingMovement && this->currentMoveData)
	{
		*velocity = this->currentMoveData->m_vecVelocity;
	}
	else
	{
		CBasePlayerPawn *pawn = this->GetPawn();
		if (!pawn) return;
		*velocity = pawn->m_vecAbsVelocity();
	}
}

void MovementPlayer::SetVelocity(const Vector &velocity)
{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	CALL_VIRTUAL(void, offsets::Teleport, pawn, NULL, NULL, &velocity);
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
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;

	// Don't change the pitch of the absolute angles because it messes with the player model.
	QAngle absAngles = angles;
	absAngles.x = 0;

	CALL_VIRTUAL(void, offsets::Teleport, pawn, NULL, &absAngles, NULL);
	utils::SnapViewAngles(pawn, angles);
}

TurnState MovementPlayer::GetTurning()
{
	QAngle currentAngle = this->moveDataPre.m_vecViewAngles;
	bool turning = this->oldAngles.y != currentAngle.y;
	if (!turning) return TURN_NONE;
	if (currentAngle.y < this->oldAngles.y - 180
		|| (currentAngle.y > this->oldAngles.y && currentAngle.y < this->oldAngles.y + 180)) return TURN_LEFT;
	return TURN_RIGHT;
}

bool MovementPlayer::IsButtonDown(InputBitMask_t button, bool onlyDown)
{
	CCSPlayer_MovementServices *ms = this->GetMoveServices();
	if (!ms) return false;
	CInButtonState buttons = ms->m_nButtons();
	if (onlyDown)
	{
		return buttons.m_pButtonStates[0] & button;
	}
	else
	{
		bool multipleKeys = (button & (button - 1));
		if (multipleKeys)
		{
			u64 key = 0;
			while (button)
			{
				u64 keyMask = 1ull << key;
				EInButtonState keyState = (EInButtonState)(keyMask && buttons.m_pButtonStates[0] + (keyMask && buttons.m_pButtonStates[1]) * 2 + (keyMask && buttons.m_pButtonStates[2]) * 4);
				if (keyState > IN_BUTTON_DOWN_UP)
				{
					return true;
				}
				key++;
				button = (InputBitMask_t)(button >> 1);
			}
			return buttons.m_pButtonStates[0] & button;
		}
		else
		{
			EInButtonState keyState = (EInButtonState)(button & buttons.m_pButtonStates[0] + (button & buttons.m_pButtonStates[1]) * 2 + (button & buttons.m_pButtonStates[2]) * 4);
			if (keyState > IN_BUTTON_DOWN_UP)
			{
				return true;
			}
			return buttons.m_pButtonStates[0] & button;
		}
	}
}

f32 MovementPlayer::GetGroundPosition()
{
	CMoveData *mv = this->currentMoveData;
	if (!this->processingMovement) mv = &this->moveDataPost;
	i32 traceCounter = 0;
	CTraceFilterPlayerMovementCS filter;
	utils::InitPlayerMovementTraceFilter(filter, this->GetPawn(), this->GetPawn()->m_Collision().m_collisionAttribute().m_nInteractsWith(), COLLISION_GROUP_PLAYER_MOVEMENT);
	Vector ground = mv->m_vecAbsOrigin;
	ground.z -= 2;
	trace_t_s2 trace;
	utils::InitGameTrace(&trace);
	f32 standableZ = 0.7;
	Vector hullMin = { -16.0, -16.0, 0.0 };
	Vector hullMax = { 16.0, 16.0, 72.0 };
	if (this->GetMoveServices()->m_bDucked()) hullMax.z = 54.0;
	utils::TracePlayerBBoxForGround(mv->m_vecAbsOrigin, ground, hullMin, hullMax, &filter, trace, standableZ, false, &traceCounter);

	f32 highestPoint = trace.endpos.z;

	if (trace.startsolid) return mv->m_vecAbsOrigin.z;

	while (trace.fraction != 1.0 && !trace.startsolid && traceCounter < 32 && trace.planeNormal.z >= standableZ)
	{
		ground.z = highestPoint;
		utils::TracePlayerBBoxForGround(mv->m_vecAbsOrigin, ground, hullMin, hullMax, &filter, trace, standableZ, false, &traceCounter); // Ghetto trace function
		if (trace.endpos.z <= highestPoint) return highestPoint;
		highestPoint = trace.endpos.z;
	}
	return highestPoint;
}

void MovementPlayer::RegisterTakeoff(bool jumped)
{
	CMoveData *mv = this->currentMoveData;
	if (!this->processingMovement) mv = &this->moveDataPost;
	this->takeoffOrigin = mv->m_vecAbsOrigin;
	this->takeoffTime = utils::GetServerGlobals()->curtime - utils::GetServerGlobals()->frametime;
	this->takeoffVelocity = mv->m_vecVelocity;
	this->takeoffGroundOrigin = mv->m_vecAbsOrigin;
	this->takeoffGroundOrigin.z = this->GetGroundPosition();
	this->jumped = jumped;
}

void MovementPlayer::RegisterLanding(const Vector &landingVelocity, bool distbugFix)
{
	CMoveData *mv = this->currentMoveData;
	if (!this->processingMovement) mv = &this->moveDataPost;
	this->landingOrigin = mv->m_vecAbsOrigin;
	this->landingTime = utils::GetServerGlobals()->curtime;
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
		FOR_EACH_VEC(mv->m_TouchList, i)
		{
			if (mv->m_TouchList[i].trace.planeNormal.z > 0.7)
			{
				this->landingOriginActual = mv->m_TouchList[i].trace.endpos;
				this->landingTimeActual = this->landingTime - (1 - mv->m_TouchList[i].trace.fraction) * utils::GetServerGlobals()->frametime; // TODO: make sure this is right
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
		Vector gravity = { 0, 0, -800 }; // TODO: Hardcoding 800 gravity right now, waiting for CVar stuff to be done
		// basic x + vt + (0.5a)t^2 = 0;
		const double delta = landingVelocity.z * landingVelocity.z - 2 * gravity.z * diffZ;
		const double time = (-landingVelocity.z - sqrt(delta)) / (gravity.z);
		this->landingOriginActual = mv->m_vecAbsOrigin + landingVelocity * time + 0.5 * gravity * time * time;
		this->landingTimeActual = this->landingTime + time;
	}
}

void MovementPlayer::Reset()
{
	this->processingDuck = false;
	this->duckBugged = false;
	this->walkMoved = false;
	this->oldWalkMoved = false;
	this->hitPerf = false;
	this->jumped = false;
	this->takeoffFromLadder = false;
	this->takeoffOrigin.Init();
	this->takeoffVelocity.Init();
	this->takeoffTime = 0.0f;
	this->takeoffGroundOrigin.Init();
	this->landingOrigin.Init();
	this->landingVelocity.Init();
	this->landingTime = 0.0f;
	this->landingOriginActual.Init();
	this->landingTimeActual = 0.0f;
}