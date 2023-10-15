#include "movement.h"
#include "utils/detours.h"

#include "tier0/memdbgon.h"

void movement::InitDetours()
{
	INIT_DETOUR(GetMaxSpeed);
	INIT_DETOUR(ProcessMovement);
	INIT_DETOUR(PlayerMoveNew);
	INIT_DETOUR(CheckParameters);
	INIT_DETOUR(CanMove);
	INIT_DETOUR(FullWalkMove);
	INIT_DETOUR(MoveInit);
	INIT_DETOUR(CheckWater);
	INIT_DETOUR(CheckVelocity);
	INIT_DETOUR(Duck);
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
	INIT_DETOUR(PlayerMovePost);
	INIT_DETOUR(PostThink);
}

f32 FASTCALL movement::Detour_GetMaxSpeed(CCSPlayerPawn *pawn)
{
	return GetMaxSpeed(pawn);
}

void FASTCALL movement::Detour_ProcessMovement(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->currentMoveData = mv;
	player->moveDataPre = CMoveData(*mv);
	player->OnStartProcessMovement();
	ProcessMovement(ms, mv);
	player->moveDataPost = CMoveData(*mv);
	player->OnStopProcessMovement();
}

bool FASTCALL movement::Detour_PlayerMoveNew(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	return PlayerMoveNew(ms, mv);
}

void FASTCALL movement::Detour_CheckParameters(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	CheckParameters(ms, mv);
}

bool FASTCALL movement::Detour_CanMove(CCSPlayerPawnBase *pawn)
{
	return CanMove(pawn);
}

void FASTCALL movement::Detour_FullWalkMove(CCSPlayer_MovementServices *ms, CMoveData *mv, bool ground)
{
	FullWalkMove(ms, mv, ground);
}

bool FASTCALL movement::Detour_MoveInit(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	return MoveInit(ms, mv);
}

bool FASTCALL movement::Detour_CheckWater(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	return CheckWater(ms, mv);
}

void FASTCALL movement::Detour_CheckVelocity(CCSPlayer_MovementServices *ms, CMoveData *mv, const char *a3)
{
	CheckVelocity(ms, mv, a3);
}

void FASTCALL movement::Detour_Duck(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->processingDuck = true;
	Duck(ms, mv);
	player->processingDuck = false;
}

bool FASTCALL movement::Detour_LadderMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	Vector oldVelocity = mv->m_vecVelocity;
	bool result = LadderMove(ms, mv);
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	MoveType_t oldMoveType = player->GetPawn()->m_MoveType();
	if (player->GetPawn()->m_lifeState() != LIFE_DEAD && !result && oldMoveType == MOVETYPE_LADDER)
	{
		// Do the setting part ourselves as well.
		utils::SetEntityMoveType(player->GetPawn(), MOVETYPE_WALK);
	}
	if (!result && oldMoveType == MOVETYPE_LADDER)
	{
		player->RegisterTakeoff(false);
		player->OnChangeMoveType(MOVETYPE_LADDER);
	}
	else if (result && oldMoveType != MOVETYPE_LADDER && player->GetPawn()->m_MoveType() == MOVETYPE_LADDER)
	{
		player->RegisterLanding(oldVelocity, false);
		player->OnChangeMoveType(MOVETYPE_WALK);
	}
	else if (result && oldMoveType == MOVETYPE_LADDER)
	{
		// Player is on the ladder and in the air, pressing jump pushes them away from the ladder.
		float curtime = utils::GetServerGlobals()->curtime;
		if (player->IsButtonDown(IN_JUMP) && player->GetPawn()->m_ignoreLadderJumpTime() <= curtime)
		{
			player->RegisterTakeoff(false);
			player->takeoffFromLadder = true;
			player->OnChangeMoveType(MOVETYPE_LADDER);
		}
	}
	return result;
}

void FASTCALL movement::Detour_CheckJumpButton(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	CheckJumpButton(ms, mv);
}

void FASTCALL movement::Detour_OnJump(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	f32 oldJumpUntil = ms->m_flJumpUntil();
	MoveType_t oldMoveType = player->GetPawn()->m_MoveType();
	OnJump(ms, mv);
	if (ms->m_flJumpUntil() != oldJumpUntil)
	{
		player->hitPerf = (oldMoveType != MOVETYPE_LADDER && !player->oldWalkMoved);
		player->RegisterTakeoff(true);
		player->OnStopTouchGround();
	}
}

void FASTCALL movement::Detour_AirAccelerate(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector &wishdir, f32 wishspeed, f32 accel)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->OnAirAcceleratePre(wishdir, wishspeed, accel);
	AirAccelerate(ms, mv, wishdir, wishspeed, accel);
	player->OnAirAcceleratePost(wishdir, wishspeed, accel);
}

void FASTCALL movement::Detour_Friction(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	Friction(ms, mv);
}

void FASTCALL movement::Detour_WalkMove(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	WalkMove(ms, mv);
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
	player->walkMoved = true;
}

void FASTCALL movement::Detour_TryPlayerMove(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	TryPlayerMove(ms, mv, pFirstDest, pFirstTrace);
}

void FASTCALL movement::Detour_CategorizePosition(CCSPlayer_MovementServices *ms, CMoveData *mv, bool bStayOnGround)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(ms);
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
}

void FASTCALL movement::Detour_FinishGravity(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	FinishGravity(ms, mv);
}

void FASTCALL movement::Detour_CheckFalling(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	CheckFalling(ms, mv);
}

void FASTCALL movement::Detour_PlayerMovePost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	PlayerMovePost(ms, mv);
}

void FASTCALL movement::Detour_PostThink(CCSPlayerPawnBase *pawn)
{
	PostThink(pawn);
}
