#pragma once
#include "common.h"
#include "movement/datatypes.h"
#include "utils/cdetour.h"

#define DECLARE_MOVEMENT_DETOUR(name) DECLARE_DETOUR(name, movement::Detour_##name, &modules::server);
#define DECLARE_MOVEMENT_EXTERN_DETOUR(name) extern CDetour<decltype(movement::Detour_##name)> name;
class MovementPlayer;

namespace movement
{
	void InitDetours();

	f32 FASTCALL Detour_GetMaxSpeed(CCSPlayerPawn *);
	void FASTCALL Detour_ProcessMovement(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_PlayerMoveNew(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckParameters(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_CanMove(CCSPlayerPawnBase *);
	void FASTCALL Detour_FullWalkMove(CCSPlayer_MovementServices *, CMoveData *, bool);
	bool FASTCALL Detour_MoveInit(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_CheckWater(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckVelocity(CCSPlayer_MovementServices *, CMoveData *, const char *);
	void FASTCALL Detour_Duck(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_LadderMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckJumpButton(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_OnJump(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_AirAccelerate(CCSPlayer_MovementServices *, CMoveData *, Vector &, f32, f32);
	void FASTCALL Detour_Friction(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_WalkMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_TryPlayerMove(CCSPlayer_MovementServices *, CMoveData *, Vector *, trace_t_s2 *);
	void FASTCALL Detour_CategorizePosition(CCSPlayer_MovementServices *, CMoveData *, bool);
	void FASTCALL Detour_FinishGravity(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckFalling(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_PlayerMovePost(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_PostThink(CCSPlayerPawnBase *);

}

class MovementPlayer
{
public:
	MovementPlayer(int i) : index(i) {}

	CCSPlayerController *GetController();
	CCSPlayerPawn *GetPawn();
	CPlayerSlot GetPlayerSlot() { return index - 1; };

	void GetOrigin(Vector *origin);
	void SetOrigin(const Vector &origin);
	void GetVelocity(Vector *velocity);
	void SetVelocity(const Vector &velocity);

	virtual void OnStartDucking();
	virtual void OnStopDucking();
	virtual void OnStartTouchGround();
	virtual void OnStopTouchGround();
	virtual void OnChangeMoveType();
	virtual void OnPlayerJump();
	virtual void OnAirAccelerate();

public:
	// General
	const i32 index;

	f32 lastProcessedCurtime{};
	u64 lastProcessedTickcount{};

	QAngle oldAngles;

	Vector takeoffOrigin;
	Vector takeoffVelocity;
	f32 takeoffTime{};
	Vector takeoffGroundOrigin;

	Vector landingOrigin;
	Vector landingVelocity;
	f32 landingTime{};
	Vector landingOriginActual;
	f32 landingTimeActual{};
};

class CMovementPlayerManager
{
public:
	CMovementPlayerManager()
	{
		for (int i = 0; i < MAXPLAYERS + 1; i++)
		{
			players[i] = new MovementPlayer(i);
		}
	}

public:
	MovementPlayer *ToPlayer(CCSPlayer_MovementServices *ms);
	MovementPlayer *ToPlayer(CCSPlayerController *controller);
	MovementPlayer *ToPlayer(CCSPlayerPawn *pawn);
	MovementPlayer *ToPlayer(CPlayerSlot slot);
	MovementPlayer *ToPlayer(CEntityIndex entIndex);
public:
	MovementPlayer *players[MAXPLAYERS + 1];
};

extern CMovementPlayerManager *g_pPlayerManager;