#pragma once

#include "common.h"
#include "movement/movement.h"
#include "utils/datatypes.h"

#define KZ_COLLISION_GROUP_STANDARD LAST_SHARED_COLLISION_GROUP
#define KZ_COLLISION_GROUP_NOTRIGGER COLLISION_GROUP_DEBRIS

extern CMovementPlayerManager *g_pPlayerManager;

class KZPlayer;
class Jump;

class KZPlayer : public MovementPlayer
{
public:
	KZPlayer(i32 i) : MovementPlayer(i)
	{
		inNoclip = false;
		currentCpIndex = 0;
		checkpoints = CUtlVector<Checkpoint>(1, 0);
	}
	virtual void Reset() override;
	virtual void OnStartProcessMovement() override;
	virtual void OnStopProcessMovement() override;
	virtual void OnAirAcceleratePre(Vector &wishdir, f32 &wishspeed, f32 &accel) override;
	virtual void OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel) override;
	virtual void OnStartTouchGround() override;
	virtual void OnStopTouchGround() override;
private:
	bool inNoclip;
	TurnState previousTurnState;
public:
	void ToggleHide();
	void DisableNoclip();
	void ToggleNoclip();
	void EnableGodMode();
	void HandleMoveCollision();
	void UpdatePlayerModelAlpha();
	
	// Checkpoint stuff
	struct Checkpoint
	{
		Vector origin;
		QAngle angles;
		Vector ladderNormal;
		bool onLadder;
		CHandle< CBaseEntity > groundEnt;
		f32 slopeDropOffset;
		f32 slopeDropHeight;
	};
	
	i32 currentCpIndex;
	bool holdingStill;
	f32 teleportTime;

	CUtlVector<Checkpoint> checkpoints;

	void SetCheckpoint();
	void DoTeleport(i32 index);
	void TpHoldPlayerStill();
	void TpToCheckpoint();
	void TpToPrevCp();
	void TpToNextCp();


	// Jumpstats
	CUtlVector<Jump> jumps;

	// misc
	bool hideOtherPlayers;
};

class AACall
{
public:
	f32 externalGain{};
	f32 currentYaw{};
	Vector wishdir;
	f32 maxspeed{};
	f32 wishspeed{};
	f32 accel{};

	f32 surfaceFriction{};
	f32 subtickFraction{};
	CInButtonState buttons;
	Vector velocityPre;
	Vector velocityPost;
	f32 curtime{};
	i32 tickcount{};
	bool ducking{};

public:
	f32 CalcIdealYaw(bool useRadians = false);
	f32 CalcMinYaw(bool useRadians = false);
	f32 CalcMaxYaw(bool useRadians = false);
	f32 CalcAccelSpeed(bool tryMaxSpeed = false);
	f32 CalcIdealGain();
};

class Strafe
{
public:
	CCopyableUtlVector<AACall> aaCalls;
	TurnState turnstate;

private:
	float starttime;
public:
	Strafe();

	f32 GetStrafeStartTime();
	f32 GetStrafeDuration();

	f32 CalcSync();
	f32 CalcGain();
	f32 CalcLoss();
	f32 CalcWidth();

	// BA/OL/DA
	f32 CalcBadAngleDuration();
	f32 CalcOverlapDuration();
	f32 CalcDeadAirDuration();

	// Calculate the ratio for each strafe.
	// The ratio is 0 if the angle is perfect, closer to -1 if it's too slow
	// Closer to 1 if it passes the optimal value.
	
	// If the player jumps in place, no velocity and no attempt to move at all, any angle will be "perfect".
	f32 CalcAngleRatioMin();
	f32 CalcAngleRatioMax();
	f32 CalcAngleRatioAverage();
	f32 CalcGainEfficiency();
};

class Jump
{
private:
	KZPlayer *player;

	Vector takeoffOrigin;
	Vector adjustedTakeoffOrigin;

	Vector landingOrigin;
	Vector adjustedLandingOrigin;

	bool validJump;
public:
	CCopyableUtlVector<Strafe> strafes;
public:
	Jump() {}
	Jump(KZPlayer *player) : player(player)
	{
		this->takeoffOrigin = player->takeoffOrigin;
		this->adjustedTakeoffOrigin = player->takeoffGroundOrigin;
	}
	void Update();
	void End();
	void Invalidate();
	u8 DetermineJumpType();

	Strafe *GetCurrentStrafe();

	f32 GetOffset() { return adjustedLandingOrigin.z - adjustedTakeoffOrigin.z; };
	f32 GetDistance();
	f32 GetMaxSpeed();
	f32 GetSync();
	f32 GetBadAngles();
	f32 GetOverlap();
	f32 GetDeadAir();
	f32 GetMaxHeight();
	f32 GetWidth();
	f32 GetEdge(bool landing);
	f32 GetGain();
	f32 GetLoss();
	f32 GetStrafeEfficiency();
	f32 GetAirPath();
	f32 GetDuckTime(bool endOnly);
};

class CKZPlayerManager : public CMovementPlayerManager
{
public:
	CKZPlayerManager()
	{
		for (int i = 0; i < MAXPLAYERS + 1; i++)
		{
			delete players[i];
			players[i] = new KZPlayer(i);
		}
	}
public:
	KZPlayer *ToPlayer(CCSPlayer_MovementServices *ms);
	KZPlayer *ToPlayer(CCSPlayerController *controller);
	KZPlayer *ToPlayer(CBasePlayerPawn *pawn);
	KZPlayer *ToPlayer(CPlayerSlot slot);
	KZPlayer *ToPlayer(CEntityIndex entIndex);
	KZPlayer *ToPlayer(CPlayerUserId userID);

	KZPlayer *ToKZPlayer(MovementPlayer *player) { return static_cast<KZPlayer *>(player); }
};

namespace KZ
{
	CKZPlayerManager *GetKZPlayerManager();
	namespace HUD
	{
		void OnProcessUsercmds_Post(CPlayerSlot &slot, bf_read *buf, int numcmds, bool ignore, bool paused);
		void DrawSpeedPanel(KZPlayer *player);
	}
	namespace misc
	{
		void RegisterCommands();
		void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);
		void OnClientPutInServer(CPlayerSlot slot);
	}
};