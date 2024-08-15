#pragma once
#include "common.h"
#include "player/player.h"
#include "steam/steam_api_common.h"
#include "steam/steamclientpublic.h"
#include "steam/isteamuser.h"

class CCSPlayer_MovementServices;

#include "sdk/datatypes.h"
#include "sdk/services.h"
// TODO: better error sound
#define MV_SND_ERROR "Buttons.snd8"

class CCSPlayerController;
class MovementPlayer;
class CCSPlayerPawnBase;
class CUserCmd;

namespace movement
{
	void InitDetours();

	void FASTCALL Detour_PhysicsSimulate(CCSPlayerController *);
	f32 FASTCALL Detour_GetMaxSpeed(CCSPlayerPawn *);
	void FASTCALL Detour_SetupMove(CCSPlayer_MovementServices *, CUserCmd *, CMoveData *);
	i32 FASTCALL Detour_ProcessUsercmds(CCSPlayerController *, void *, int, bool, float);
	void FASTCALL Detour_ProcessMovement(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_PlayerMoveNew(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckParameters(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_CanMove(CCSPlayerPawnBase *);
	void FASTCALL Detour_FullWalkMove(CCSPlayer_MovementServices *, CMoveData *, bool);
	bool FASTCALL Detour_MoveInit(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_CheckWater(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_WaterMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckVelocity(CCSPlayer_MovementServices *, CMoveData *, const char *);
	void FASTCALL Detour_Duck(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_CanUnduck(CCSPlayer_MovementServices *, CMoveData *);
	bool FASTCALL Detour_LadderMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckJumpButton(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_OnJump(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_AirMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_AirAccelerate(CCSPlayer_MovementServices *, CMoveData *, Vector &, f32, f32);
	void FASTCALL Detour_Friction(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_WalkMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_TryPlayerMove(CCSPlayer_MovementServices *, CMoveData *, Vector *, trace_t *);
	void FASTCALL Detour_CategorizePosition(CCSPlayer_MovementServices *, CMoveData *, bool);
	void FASTCALL Detour_FinishGravity(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_CheckFalling(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_PostPlayerMove(CCSPlayer_MovementServices *, CMoveData *);
	void FASTCALL Detour_PostThink(CCSPlayerPawnBase *);
} // namespace movement

class MovementPlayer : public Player
{
	using Player::Player;

public:
	virtual void Reset() override;
	virtual CCSPlayer_MovementServices *GetMoveServices();

	// This doesn't work during movement processing!
	virtual void Teleport(const Vector *origin, const QAngle *angles, const Vector *velocity);

	virtual void GetOrigin(Vector *origin);
	virtual void SetOrigin(const Vector &origin);
	virtual void GetVelocity(Vector *velocity);
	virtual void SetVelocity(const Vector &velocity);
	virtual void GetAngles(QAngle *angles);
	// It is not recommended use this to change the angle inside movement processing, it might not work!
	virtual void SetAngles(const QAngle &angles);

	virtual void GetBBoxBounds(bbox_t *bounds);

	virtual TurnState GetTurning();

	virtual bool IsButtonPressed(InputBitMask_t button, bool onlyDown = false);

	virtual void RegisterTakeoff(bool jumped);
	virtual void RegisterLanding(const Vector &landingVelocity, bool distbugFix = true);
	virtual f32 GetGroundPosition();

	virtual META_RES GetPlayerMaxSpeed(f32 &maxSpeed);

	// Movement hooks
	virtual void OnPhysicsSimulate();
	virtual void OnPhysicsSimulatePost();

	virtual void OnProcessUsercmds(void *, int) {}

	virtual void OnProcessUsercmdsPost(void *, int) {}

	virtual void OnSetupMove(CUserCmd *) {}

	virtual void OnSetupMovePost(CUserCmd *) {}

	virtual void OnProcessMovement();
	virtual void OnProcessMovementPost();

	virtual void OnPlayerMove() {}

	virtual void OnPlayerMovePost() {}

	virtual void OnCheckParameters() {}

	virtual void OnCheckParametersPost() {}

	virtual void OnCanMove() {}

	virtual void OnCanMovePost() {}

	virtual void OnFullWalkMove(bool &) {}

	virtual void OnFullWalkMovePost(bool) {}

	virtual void OnMoveInit() {}

	virtual void OnMoveInitPost() {}

	virtual void OnCheckWater() {}

	virtual void OnCheckWaterPost() {}

	virtual void OnWaterMove() {}

	virtual void OnWaterMovePost() {}

	virtual void OnCheckVelocity(const char *) {}

	virtual void OnCheckVelocityPost(const char *) {}

	virtual void OnDuck() {}

	virtual void OnDuckPost() {}

	// Make an exception for this as it is the only time where we need to change the return value.
	virtual void OnCanUnduck() {}

	virtual void OnCanUnduckPost(bool &) {}

	virtual void OnLadderMove() {}

	virtual void OnLadderMovePost() {}

	virtual void OnCheckJumpButton() {}

	virtual void OnCheckJumpButtonPost() {}

	virtual void OnJump() {}

	virtual void OnJumpPost() {}

	virtual void OnAirMove() {}

	virtual void OnAirMovePost() {}

	virtual void OnAirAccelerate(Vector &wishdir, f32 &wishspeed, f32 &accel) {}

	virtual void OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel) {}

	virtual void OnFriction() {}

	virtual void OnFrictionPost() {}

	virtual void OnWalkMove() {}

	virtual void OnWalkMovePost() {}

	virtual void OnTryPlayerMove(Vector *, trace_t *) {}

	virtual void OnTryPlayerMovePost(Vector *, trace_t *) {}

	virtual void OnCategorizePosition(bool) {}

	virtual void OnCategorizePositionPost(bool) {}

	virtual void OnFinishGravity() {}

	virtual void OnFinishGravityPost() {}

	virtual void OnCheckFalling() {}

	virtual void OnCheckFallingPost() {}

	virtual void OnPostPlayerMove() {}

	virtual void OnPostPlayerMovePost() {}

	virtual void OnPostThink();

	virtual void OnPostThinkPost() {}

	// Movement events
	virtual void OnStartTouchGround() {}

	virtual void OnStopTouchGround() {}

	virtual void OnChangeMoveType(MoveType_t oldMoveType) {}

	// Other events
	virtual void OnChangeTeamPost(i32 team) {}

	virtual void OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity) {}

	virtual bool OnTriggerStartTouch(CBaseTrigger *trigger)
	{
		return true;
	}

	virtual bool OnTriggerTouch(CBaseTrigger *trigger)
	{
		return true;
	}

	virtual bool OnTriggerEndTouch(CBaseTrigger *trigger)
	{
		return true;
	}

	bool IsAlive()
	{
		return this->GetPlayerPawn() ? this->GetPlayerPawn()->IsAlive() : false;
	}

	void SetMoveType(MoveType_t newMoveType, bool fireCallback = true);

	MoveType_t GetMoveType()
	{
		return this->GetPlayerPawn() ? this->GetPlayerPawn()->m_MoveType() : MOVETYPE_NONE;
	}

	StandardCollisionGroups_t GetCollisionGroup()
	{
		return this->GetPlayerPawn() ? (StandardCollisionGroups_t)this->GetPlayerPawn()->m_Collision().m_CollisionGroup()
									 : LAST_SHARED_COLLISION_GROUP;
	}

	void SetCollidingWithWorld()
	{
		this->collidingWithWorld = true;
	}

	bool IsCollidingWithWorld()
	{
		return this->currentMoveData->m_TouchList.Count() > 0 || this->collidingWithWorld;
	}

public:
	// General
	bool processingMovement {};
	CMoveData *currentMoveData {};
	CMoveData moveDataPre;
	CMoveData moveDataPost;
	QAngle oldAngles;

	bool processingDuck {};
	bool duckBugged {};
	bool walkMoved {};
	bool oldWalkMoved {};
	bool inPerf {};
	bool jumped {};
	bool takeoffFromLadder {};
	Vector lastValidLadderOrigin;

	Vector takeoffOrigin;
	Vector takeoffVelocity;
	f32 takeoffTime {};
	Vector takeoffGroundOrigin;

	Vector landingOrigin;
	Vector landingVelocity;
	f32 landingTime {};
	Vector landingOriginActual;
	f32 landingTimeActual {};

	bool enableWaterFix {};
	bool ignoreNextCategorizePosition {};

	Vector velocityBeforeTriggerTouch;
	Vector originBeforeTriggerTouch;
	CUtlVector<CEntityHandle> pendingStartTouchTriggers;
	CUtlVector<CEntityHandle> pendingEndTouchTriggers;
	CUtlVector<CEntityHandle> touchedTriggers;

private:
	bool collidingWithWorld {};
	// Movetype changes that occur outside of movement processing
	MoveType_t lastKnownMoveType {};
};

class MovementPlayerManager : public PlayerManager
{
public:
	MovementPlayerManager()
	{
		for (int i = 0; i < MAXPLAYERS + 1; i++)
		{
			delete players[i];
			players[i] = new MovementPlayer(i);
		}
	}

public:
	MovementPlayer *ToPlayer(CPlayerPawnComponent *component);
	MovementPlayer *ToPlayer(CBasePlayerController *controller);
	MovementPlayer *ToPlayer(CBasePlayerPawn *pawn);
	MovementPlayer *ToPlayer(CPlayerSlot slot);
	MovementPlayer *ToPlayer(CEntityIndex entIndex);
	MovementPlayer *ToPlayer(CPlayerUserId userID);
	MovementPlayer *ToPlayer(u32 index);

	MovementPlayer *ToMovementPlayer(Player *player)
	{
		return static_cast<MovementPlayer *>(player);
	}
};
