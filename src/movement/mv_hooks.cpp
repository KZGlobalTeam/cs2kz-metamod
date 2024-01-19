#include "movement.h"
#include "utils/detours.h"

#include "tier0/memdbgon.h"

void movement::InitDetours()
{
	INIT_DETOUR(GetMaxSpeed);
	INIT_DETOUR(ProcessUsercmds);
	INIT_DETOUR(ProcessMovement);
	INIT_DETOUR(PlayerMoveNew);
	INIT_DETOUR(CheckParameters);
	INIT_DETOUR(CanMove);
	INIT_DETOUR(FullWalkMove);
	INIT_DETOUR(MoveInit);
	INIT_DETOUR(CheckWater);
	INIT_DETOUR(WaterMove);
	INIT_DETOUR(CheckVelocity);
	INIT_DETOUR(Duck);
	INIT_DETOUR(CanUnduck);
	INIT_DETOUR(LadderMove);
	INIT_DETOUR(CheckJumpButton);
	INIT_DETOUR(OnJump);
	INIT_DETOUR(AirAccelerate);
	INIT_DETOUR(Friction);
	INIT_DETOUR(WalkMove);
	INIT_DETOUR(TryPlayerMove);
	INIT_DETOUR(CategorizePosition);
	INIT_DETOUR(FinishGravity);
	INIT_DETOUR(CheckFalling);
	INIT_DETOUR(PostPlayerMove);
	INIT_DETOUR(PostThink);
}

f32 FASTCALL movement::Detour_GetMaxSpeed(CCSPlayerPawn *pawn)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(pawn);
	f32 newMaxSpeed = player->GetPlayerMaxSpeed();

	if (newMaxSpeed <= 0.0f)
	{
		return GetMaxSpeed(pawn);
	}
	return newMaxSpeed;
}

i32 FASTCALL movement::Detour_ProcessUsercmds(CBasePlayerPawn *pawn, void *cmds, int numcmds, bool paused)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(pawn);
	player->OnProcessUsercmds(cmds, numcmds);
	auto retValue = ProcessUsercmds(pawn, cmds, numcmds, paused);
	player->OnProcessUsercmdsPost(cmds, numcmds);
	return retValue;
}

void FASTCALL movement::Detour_ProcessMovement(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->currentMoveData = mv;
	player->moveDataPre = CMoveData(*mv);
	player->OnProcessMovement();
	ProcessMovement(ms, mv);
	player->moveDataPost = CMoveData(*mv);
	player->OnProcessMovementPost();
}

bool FASTCALL movement::Detour_PlayerMoveNew(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnPlayerMove();
	auto retValue = PlayerMoveNew(ms, mv);
	player->OnPlayerMovePost();
	return retValue;
}

void FASTCALL movement::Detour_CheckParameters(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnCheckParameters();
	CheckParameters(ms, mv);
	player->OnCheckParametersPost();
}

bool FASTCALL movement::Detour_CanMove(CCSPlayerPawnBase *pawn)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(pawn);
	player->OnCanMove();
	auto retValue = CanMove(pawn);
	player->OnCanMovePost();
	return retValue;
}

void FASTCALL movement::Detour_FullWalkMove(CCSPlayer_MovementServices *ms, CMoveData *mv, bool ground)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnFullWalkMove(ground);
	FullWalkMove(ms, mv, ground);
	player->OnFullWalkMovePost(ground);
}

bool FASTCALL movement::Detour_MoveInit(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnMoveInit();
	auto retValue = MoveInit(ms, mv);
	player->OnMoveInitPost();
	return retValue;
}

bool FASTCALL movement::Detour_CheckWater(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnCheckWater();
	auto retValue = CheckWater(ms, mv);
	player->OnCheckWaterPost();
#ifdef WATER_FIX
	if (player->enableWaterFixThisTick)
	{
		return player->GetPawn()->m_flWaterLevel() > 0.5f;
	}
#endif
	return retValue;
}

void FASTCALL movement::Detour_WaterMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnWaterMove();
#ifdef WATER_FIX
	if (player->enableWaterFixThisTick)
	{
		player->ignoreNextCategorizePosition = true;
	}
#endif
	WaterMove(ms, mv);
	player->OnWaterMovePost();
}

void FASTCALL movement::Detour_CheckVelocity(CCSPlayer_MovementServices *ms, CMoveData *mv, const char *a3)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnCheckVelocity(a3);
	CheckVelocity(ms, mv, a3);
	player->OnCheckVelocityPost(a3);
}

void FASTCALL movement::Detour_Duck(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnDuck();
	player->processingDuck = true;
	Duck(ms, mv);
	player->processingDuck = false;
	player->OnDuckPost();
}

bool FASTCALL movement::Detour_CanUnduck(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	int overrideValue = player->OnCanUnduck();
	bool canUnduck = CanUnduck(ms, mv);
	if (overrideValue != -1)
	{
		canUnduck = !!overrideValue;
	}
	player->OnCanUnduckPost();
	return canUnduck;
}

bool FASTCALL movement::Detour_LadderMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnLadderMove();
	Vector oldVelocity = mv->m_vecVelocity;
	MoveType_t oldMoveType = player->GetPawn()->m_MoveType();
	bool result = LadderMove(ms, mv);
	if (player->GetPawn()->m_lifeState() != LIFE_DEAD && !result && oldMoveType == MOVETYPE_LADDER)
	{
		// Do the setting part ourselves as well.
		player->GetPawn()->SetMoveType(MOVETYPE_WALK);
	}
	if (!result && oldMoveType == MOVETYPE_LADDER)
	{
		player->RegisterTakeoff(false);
		player->takeoffFromLadder = true;
		// Ladderjump takeoff detection is delayed by one process movement call, we have to use the previous origin.
		player->takeoffOrigin = player->lastValidLadderOrigin;
		player->takeoffGroundOrigin = player->lastValidLadderOrigin;
		player->OnChangeMoveType(MOVETYPE_LADDER);
	}
	else if (result && oldMoveType != MOVETYPE_LADDER && player->GetPawn()->m_MoveType() == MOVETYPE_LADDER)
	{
		player->RegisterLanding(oldVelocity, false);
		player->OnChangeMoveType(MOVETYPE_WALK);
	}
	else if (result && oldMoveType == MOVETYPE_LADDER && player->GetPawn()->m_MoveType() == MOVETYPE_WALK)
	{
		// Player is on the ladder, pressing jump pushes them away from the ladder.
		float curtime = g_pKZUtils->GetServerGlobals()->curtime;
		player->RegisterTakeoff(player->IsButtonPressed(IN_JUMP));
		player->takeoffFromLadder = true;
		player->OnChangeMoveType(MOVETYPE_LADDER);
	}

	if (result && player->GetPawn()->m_MoveType() == MOVETYPE_LADDER)
	{
		player->GetOrigin(&player->lastValidLadderOrigin);
	}
	player->OnLadderMovePost();
	return result;
}

void FASTCALL movement::Detour_CheckJumpButton(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
#ifdef WATER_FIX
	if (player->enableWaterFixThisTick && ms->pawn->m_MoveType() == MOVETYPE_WALK && ms->pawn->m_flWaterLevel() > 0.5f)
	{
		if (ms->m_nButtons()->m_pButtonStates[0] & IN_JUMP)
		{
			ms->m_nButtons()->m_pButtonStates[1] |= IN_JUMP;
		}
		movement::Detour_Duck(ms, mv);
	}
#endif
	player->OnCheckJumpButton();
	CheckJumpButton(ms, mv);
	player->OnCheckJumpButtonPost();
}

void FASTCALL movement::Detour_OnJump(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnJump();
	f32 oldJumpUntil = ms->m_flJumpUntil();
	MoveType_t oldMoveType = player->GetPawn()->m_MoveType();
	OnJump(ms, mv);
	if (ms->m_flJumpUntil() != oldJumpUntil)
	{
		player->hitPerf = (oldMoveType != MOVETYPE_LADDER && !player->oldWalkMoved);
		player->RegisterTakeoff(true);
		player->OnStopTouchGround();
	}
	player->OnJumpPost();
}

void FASTCALL movement::Detour_AirAccelerate(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector &wishdir, f32 wishspeed, f32 accel)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnAirAccelerate(wishdir, wishspeed, accel);
	AirAccelerate(ms, mv, wishdir, wishspeed, accel);
	player->OnAirAcceleratePost(wishdir, wishspeed, accel);
}

void FASTCALL movement::Detour_Friction(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnFriction();
	Friction(ms, mv);
	player->OnFrictionPost();
}

void FASTCALL movement::Detour_WalkMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnWalkMove();
	WalkMove(ms, mv);
	player->walkMoved = true;
	player->OnWalkMovePost();
}

void FASTCALL movement::Detour_TryPlayerMove(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnTryPlayerMove(pFirstDest, pFirstTrace);
	TryPlayerMove(ms, mv, pFirstDest, pFirstTrace);
	player->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
}

void FASTCALL movement::Detour_CategorizePosition(CCSPlayer_MovementServices *ms, CMoveData *mv, bool bStayOnGround)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
#ifdef WATER_FIX
	if (player->enableWaterFixThisTick && player->ignoreNextCategorizePosition)
	{
		player->ignoreNextCategorizePosition = false;
		return;
	}
#endif
	player->OnCategorizePosition(bStayOnGround);
	Vector oldVelocity = mv->m_vecVelocity;
	bool oldOnGround = !!(player->GetPawn()->m_fFlags() & FL_ONGROUND);

	CategorizePosition(ms, mv, bStayOnGround);

	bool ground = !!(player->GetPawn()->m_fFlags() & FL_ONGROUND);

	if (oldOnGround != ground)
	{
		if (ground)
		{
			player->RegisterLanding(oldVelocity);
			player->duckBugged = player->processingDuck;
			player->OnStartTouchGround();
		}
		else
		{
			player->RegisterTakeoff(false);
			player->OnStopTouchGround();
		}
	}
	player->OnCategorizePositionPost(bStayOnGround);
}

void FASTCALL movement::Detour_FinishGravity(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnFinishGravity();
	FinishGravity(ms, mv);
	player->OnFinishGravityPost();
}

void FASTCALL movement::Detour_CheckFalling(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnCheckFalling();
	CheckFalling(ms, mv);
	player->OnCheckFallingPost();
}

void FASTCALL movement::Detour_PostPlayerMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnPostPlayerMove();
	PostPlayerMove(ms, mv);
	player->OnPostPlayerMovePost();
}

void FASTCALL movement::Detour_PostThink(CCSPlayerPawnBase *pawn)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(pawn);
	player->OnPostThink();
	PostThink(pawn);
	player->OnPostThinkPost();
}
