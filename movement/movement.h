#pragma once

#include "movement/datatypes.h"
#include "utils/cdetour.h"

#define DECLARE_MOVEMENT_DETOUR(name) DECLARE_DETOUR(name, movement::Detour_##name, &modules::server);
#define DECLARE_MOVEMENT_EXTERN_DETOUR(name) extern CDetour<decltype(movement::Detour_##name)> name;
class MovementPlayer;

namespace movement
{
	void InitDetours();

	float FASTCALL Detour_GetMaxSpeed(CCSPlayerPawn *);
	void FASTCALL Detour_ProcessMovement(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_PlayerMoveNew(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckParameters(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_CanMove(CCSPlayerPawnBase *);
	void FASTCALL Detour_FullWalkMove(CCSPlayer_MovementServices *, CMoveData *, bool );
	bool FASTCALL Detour_MoveInit(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_CheckWater(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckVelocity(CCSPlayer_MovementServices *, CMoveData *, const char *);
	void FASTCALL Detour_Duck(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_LadderMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckJumpButton(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_OnJump(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_AirAccelerate(CCSPlayer_MovementServices *, CMoveData *, Vector &, float , float );
	void FASTCALL Detour_Friction(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_WalkMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_TryPlayerMove(CCSPlayer_MovementServices *, CMoveData *, Vector *, trace_t_s2 *);
	void FASTCALL Detour_CategorizePosition(CCSPlayer_MovementServices *, CMoveData *, bool );
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

	Vector &GetOrigin();
	void SetOrigin(const Vector &origin);
	Vector &GetVelocity();
	void GetVelocity(const Vector &velocity);

	virtual void OnStartDucking();
	virtual void OnStopDucking();
	virtual void OnStartTouchGround();
	virtual void OnStopTouchGround();
	virtual void OnChangeMoveType();
	virtual void OnPlayerJump();
	virtual void OnAirAccelerate();

public:
	// General
	const uint32_t index;

	float lastProcessedCurtime{};
	uint64_t lastProcessedTickcount{};

	QAngle oldAngles;

	Vector takeoffOrigin;
	Vector takeoffVelocity;
	float takeoffTime{};
	Vector takeoffGroundOrigin;

	Vector landingOrigin;
	Vector landingVelocity;
	float landingTime{};
	Vector landingOriginActual;
	float landingTimeActual{};
};
