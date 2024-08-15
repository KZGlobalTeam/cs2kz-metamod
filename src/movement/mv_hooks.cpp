#include "cs_usercmd.pb.h"
#include "movement.h"
#include "utils/detours.h"
#include "utils/gameconfig.h"
#include "tier0/memdbgon.h"
#include "sdk/usercmd.h"
extern CGameConfig *g_pGameConfig;

void movement::InitDetours()
{
	INIT_DETOUR(g_pGameConfig, PhysicsSimulate);
	INIT_DETOUR(g_pGameConfig, GetMaxSpeed);
	INIT_DETOUR(g_pGameConfig, ProcessUsercmds);
	INIT_DETOUR(g_pGameConfig, SetupMove);
	INIT_DETOUR(g_pGameConfig, ProcessMovement);
	INIT_DETOUR(g_pGameConfig, PlayerMoveNew);
	INIT_DETOUR(g_pGameConfig, CheckParameters);
	INIT_DETOUR(g_pGameConfig, CanMove);
	INIT_DETOUR(g_pGameConfig, FullWalkMove);
	INIT_DETOUR(g_pGameConfig, MoveInit);
	INIT_DETOUR(g_pGameConfig, CheckWater);
	INIT_DETOUR(g_pGameConfig, WaterMove);
	INIT_DETOUR(g_pGameConfig, CheckVelocity);
	INIT_DETOUR(g_pGameConfig, Duck);
	INIT_DETOUR(g_pGameConfig, CanUnduck);
	INIT_DETOUR(g_pGameConfig, LadderMove);
	INIT_DETOUR(g_pGameConfig, CheckJumpButton);
	INIT_DETOUR(g_pGameConfig, OnJump);
	INIT_DETOUR(g_pGameConfig, AirMove);
	INIT_DETOUR(g_pGameConfig, AirAccelerate);
	INIT_DETOUR(g_pGameConfig, Friction);
	INIT_DETOUR(g_pGameConfig, WalkMove);
	INIT_DETOUR(g_pGameConfig, TryPlayerMove);
	INIT_DETOUR(g_pGameConfig, CategorizePosition);
	INIT_DETOUR(g_pGameConfig, FinishGravity);
	INIT_DETOUR(g_pGameConfig, CheckFalling);
	INIT_DETOUR(g_pGameConfig, PostPlayerMove);
	INIT_DETOUR(g_pGameConfig, PostThink);
}

MovementPlayerManager *playerManager = static_cast<MovementPlayerManager *>(g_pPlayerManager);

void FASTCALL movement::Detour_PhysicsSimulate(CCSPlayerController *controller)
{
	MovementPlayer *player = playerManager->ToPlayer(controller);
	if (controller->m_bIsHLTV)
	{
		return;
	}
	player->OnPhysicsSimulate();
	PhysicsSimulate(controller);
	player->OnPhysicsSimulatePost();
}

f32 FASTCALL movement::Detour_GetMaxSpeed(CCSPlayerPawn *pawn)
{
	MovementPlayer *player = playerManager->ToPlayer(pawn);
	f32 maxSpeed = GetMaxSpeed(pawn);
	f32 newMaxSpeed = maxSpeed;

	if (player->GetPlayerMaxSpeed(newMaxSpeed) != MRES_IGNORED)
	{
		return newMaxSpeed;
	}
	return maxSpeed;
}

i32 FASTCALL movement::Detour_ProcessUsercmds(CCSPlayerController *controller, void *cmds, int numcmds, bool paused, float margin)
{
	MovementPlayer *player = playerManager->ToPlayer(controller);
	player->OnProcessUsercmds(cmds, numcmds);
	auto retValue = ProcessUsercmds(controller, cmds, numcmds, paused, margin);
	player->OnProcessUsercmdsPost(cmds, numcmds);
	return retValue;
}

void FASTCALL movement::Detour_SetupMove(CCSPlayer_MovementServices *ms, CUserCmd *pb, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	CBasePlayerController *controller = player->GetController();
	player->OnSetupMove(pb);
	SetupMove(ms, pb, mv);
	player->OnSetupMovePost(pb);
}

void FASTCALL movement::Detour_ProcessMovement(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->currentMoveData = mv;
	player->moveDataPre = CMoveData(*mv);
	player->OnProcessMovement();
	ProcessMovement(ms, mv);
	player->moveDataPost = CMoveData(*mv);
	player->OnProcessMovementPost();
}

bool FASTCALL movement::Detour_PlayerMoveNew(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnPlayerMove();
	auto retValue = PlayerMoveNew(ms, mv);
	player->OnPlayerMovePost();
	return retValue;
}

void FASTCALL movement::Detour_CheckParameters(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnCheckParameters();
	CheckParameters(ms, mv);
	player->OnCheckParametersPost();
}

bool FASTCALL movement::Detour_CanMove(CCSPlayerPawnBase *pawn)
{
	MovementPlayer *player = playerManager->ToPlayer(pawn);
	player->OnCanMove();
	auto retValue = CanMove(pawn);
	player->OnCanMovePost();
	return retValue;
}

void FASTCALL movement::Detour_FullWalkMove(CCSPlayer_MovementServices *ms, CMoveData *mv, bool ground)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnFullWalkMove(ground);
	FullWalkMove(ms, mv, ground);
	player->OnFullWalkMovePost(ground);
}

bool FASTCALL movement::Detour_MoveInit(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnMoveInit();
	auto retValue = MoveInit(ms, mv);
	player->OnMoveInitPost();
	return retValue;
}

bool FASTCALL movement::Detour_CheckWater(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnCheckWater();
	auto retValue = CheckWater(ms, mv);
	player->OnCheckWaterPost();
#ifdef WATER_FIX
	if (player->enableWaterFix)
	{
		return player->GetPlayerPawn()->m_flWaterLevel() > 0.5f;
	}
#endif
	return retValue;
}

void FASTCALL movement::Detour_WaterMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnWaterMove();
#ifdef WATER_FIX
	if (player->enableWaterFix)
	{
		player->ignoreNextCategorizePosition = true;
	}
#endif
	WaterMove(ms, mv);
	player->OnWaterMovePost();
}

void FASTCALL movement::Detour_CheckVelocity(CCSPlayer_MovementServices *ms, CMoveData *mv, const char *a3)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnCheckVelocity(a3);
	CheckVelocity(ms, mv, a3);
	player->OnCheckVelocityPost(a3);
}

void FASTCALL movement::Detour_Duck(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnDuck();
	player->processingDuck = true;
	Duck(ms, mv);
	player->processingDuck = false;
	player->OnDuckPost();
}

bool FASTCALL movement::Detour_CanUnduck(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnCanUnduck();
	bool canUnduck = CanUnduck(ms, mv);
	player->OnCanUnduckPost(canUnduck);
	return canUnduck;
}

bool FASTCALL movement::Detour_LadderMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnLadderMove();
	Vector oldVelocity = mv->m_vecVelocity;
	MoveType_t oldMoveType = player->GetPlayerPawn()->m_MoveType();
	bool result = LadderMove(ms, mv);
	if (player->GetPlayerPawn()->m_lifeState() != LIFE_DEAD && !result && oldMoveType == MOVETYPE_LADDER)
	{
		// Do the setting part ourselves as well, but do not fire callback because it will be called later with the correct parameters.
		player->SetMoveType(MOVETYPE_WALK, false);
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
	else if (result && oldMoveType != MOVETYPE_LADDER && player->GetPlayerPawn()->m_MoveType() == MOVETYPE_LADDER
			 && !(player->GetPlayerPawn()->m_fFlags & FL_ONGROUND))
	{
		player->RegisterLanding(oldVelocity, false);
		player->OnChangeMoveType(MOVETYPE_WALK);
	}
	else if (result && oldMoveType == MOVETYPE_LADDER && player->GetPlayerPawn()->m_MoveType() == MOVETYPE_WALK)
	{
		// Player is on the ladder, pressing jump pushes them away from the ladder.
		player->RegisterTakeoff(player->IsButtonPressed(IN_JUMP));
		player->takeoffFromLadder = true;
		player->OnChangeMoveType(MOVETYPE_LADDER);
	}

	if (result && player->GetPlayerPawn()->m_MoveType() == MOVETYPE_LADDER)
	{
		player->GetOrigin(&player->lastValidLadderOrigin);
	}
	player->OnLadderMovePost();
	return result;
}

void FASTCALL movement::Detour_CheckJumpButton(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
#ifdef WATER_FIX
	if (player->enableWaterFix && ms->pawn->m_MoveType() == MOVETYPE_WALK && ms->pawn->m_flWaterLevel() > 0.5f)
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
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnJump();
	f32 oldJumpUntil = ms->m_flJumpUntil();
	MoveType_t oldMoveType = player->GetPlayerPawn()->m_MoveType();
	OnJump(ms, mv);
	if (ms->m_flJumpUntil() != oldJumpUntil)
	{
		player->inPerf = (oldMoveType != MOVETYPE_LADDER && !player->oldWalkMoved);
		player->RegisterTakeoff(true);
		player->OnStopTouchGround();
	}
	player->OnJumpPost();
}

void FASTCALL movement::Detour_AirMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnAirMove();
	AirMove(ms, mv);
	player->OnAirMovePost();
}

void FASTCALL movement::Detour_AirAccelerate(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector &wishdir, f32 wishspeed, f32 accel)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnAirAccelerate(wishdir, wishspeed, accel);
	AirAccelerate(ms, mv, wishdir, wishspeed, accel);
	player->OnAirAcceleratePost(wishdir, wishspeed, accel);
}

void FASTCALL movement::Detour_Friction(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnFriction();
	Friction(ms, mv);
	player->OnFrictionPost();
}

void FASTCALL movement::Detour_WalkMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnWalkMove();
	WalkMove(ms, mv);
	player->walkMoved = true;
	player->OnWalkMovePost();
}

void FASTCALL movement::Detour_TryPlayerMove(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t *pFirstTrace)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnTryPlayerMove(pFirstDest, pFirstTrace);
	Vector oldVelocity = mv->m_vecVelocity;
	TryPlayerMove(ms, mv, pFirstDest, pFirstTrace);
	if (mv->m_vecVelocity != oldVelocity)
	{
		// Velocity changed, must have collided with something.
		// In case of walkmove+stairs, it's possible that this is incorrect because TryPlayerMove won't immediately change the player's data,
		// but for now this doesn't matter.
		player->SetCollidingWithWorld();
	}
	player->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
}

void FASTCALL movement::Detour_CategorizePosition(CCSPlayer_MovementServices *ms, CMoveData *mv, bool bStayOnGround)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
#ifdef WATER_FIX
	if (player->enableWaterFix && player->ignoreNextCategorizePosition)
	{
		player->ignoreNextCategorizePosition = false;
		return;
	}
#endif
	player->OnCategorizePosition(bStayOnGround);
	Vector oldVelocity = mv->m_vecVelocity;
	bool oldOnGround = !!(player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND);

	CategorizePosition(ms, mv, bStayOnGround);

	bool ground = !!(player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND);

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
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnFinishGravity();
	FinishGravity(ms, mv);
	player->OnFinishGravityPost();
}

void FASTCALL movement::Detour_CheckFalling(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnCheckFalling();
	CheckFalling(ms, mv);
	player->OnCheckFallingPost();
}

void FASTCALL movement::Detour_PostPlayerMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnPostPlayerMove();
	PostPlayerMove(ms, mv);
	player->OnPostPlayerMovePost();
}

void FASTCALL movement::Detour_PostThink(CCSPlayerPawnBase *pawn)
{
	MovementPlayer *player = playerManager->ToPlayer(pawn);
	player->OnPostThink();
	PostThink(pawn);
	player->OnPostThinkPost();
}
