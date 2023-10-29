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
	bool jsAlways;

	// misc
	bool hideOtherPlayers;
};

class AACall
{
public:
	f32 externalSpeedDiff{};
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
	CUtlVector<AACall> aaCalls;
	TurnState turnstate;

private:
	f32 duration;

	f32 badAngles;
	f32 overlap;
	f32 deadAir;

	f32 syncDuration;
	f32 width;

	f32 gain;
	f32 maxGain;
	f32 loss;
	f32 externalGain;
	f32 externalLoss;
public:
	void End();
	f32 GetStrafeDuration() { return this->duration; };

	f32 GetMaxGain() { return this->maxGain; };
	f32 GetGain(bool external = false) { return external ? this->externalGain : this->gain; };
	f32 GetLoss(bool external = false) { return external ? this->externalLoss : this->loss; };
	f32 GetWidth() { return this->width; };

	// BA/OL/DA
	f32 GetBadAngleDuration() { return this->badAngles; };
	f32 GetOverlapDuration() { return this->overlap; };
	f32 GetDeadAirDuration() { return this->deadAir; };

	f32 GetSync() { return this->syncDuration / this->duration; };
	f32 GetSyncDuration() { return this->syncDuration; };
	// Calculate the ratio for each strafe.
	// The ratio is 0 if the angle is perfect, closer to -100 if it's too slow
	// Closer to 100 if it passes the optimal value.
	// Note: if the player jumps in place, no velocity and no attempt to move at all, any angle will be "perfect".
	// Returns false if there is no available stats.
	internal int SortFloat(const f32 *a, const f32 *b) { return &a > &b; };
	struct AngleRatioStats
	{
		bool available;
		f32 max;
		f32 median;
		f32 average;
	};
	AngleRatioStats arStats;
	
	bool CalcAngleRatioStats();
};

class Jump
{
private:
	KZPlayer *player;

	Vector takeoffOrigin;
	Vector adjustedTakeoffOrigin;

	Vector landingOrigin;
	Vector adjustedLandingOrigin;

	// Required for airpath stat.
	f32 totalDistance{};
	f32 currentMaxSpeed{};
	f32 currentMaxHeight = -16384.0f;
	f32 airtime{};

	f32 deadAir{};
	f32 overlap{};
	f32 badAngles{};
	f32 sync{};
	f32 duckDuration{};
	f32 duckEndDuration{};
	f32 width{};
	f32 gainEff{};
	bool validJump;
public:
	CUtlVector<Strafe> strafes;

public:
	Jump() {}
	Jump(KZPlayer *player) : player(player)
	{
		this->takeoffOrigin = player->takeoffOrigin;
		this->adjustedTakeoffOrigin = player->takeoffGroundOrigin;
	}
	void UpdateAACallPost(Vector wishdir, f32 wishspeed, f32 accel);
	void Update();
	void End();
	void Invalidate();
	u8 DetermineJumpType();

	Strafe *GetCurrentStrafe();

	f32 GetOffset() { return adjustedLandingOrigin.z - adjustedTakeoffOrigin.z; };
	f32 GetDistance();
	f32 GetMaxSpeed() { return this->currentMaxSpeed; };
	f32 GetSync() { return this->sync; };
	f32 GetBadAngles() { return this->badAngles; };
	f32 GetOverlap() { return this->overlap; };
	f32 GetDeadAir() { return this->deadAir; };
	f32 GetMaxHeight() { return this->currentMaxHeight; };
	f32 GetWidth() { return this->width; };
	f32 GetEdge(bool landing);
	f32 GetGainEfficiency() { return this->gainEff; };
	f32 GetAirPath();
	f32 GetDuckTime(bool endOnly = true) { return endOnly ? this->duckEndDuration : this->duckDuration; };
	f32 GetDeviation();
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