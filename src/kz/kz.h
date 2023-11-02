#pragma once

#include "common.h"
#include "movement/movement.h"
#include "utils/datatypes.h"

#include "kz_jumpstats.h"

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
		this->checkpoints = CUtlVector<Checkpoint>(1, 0);
		this->jumps = CUtlVector<Jump>(1, 0);
	}
	virtual void Reset() override;
	virtual void OnStartProcessMovement() override;
	virtual void OnStopProcessMovement() override;

	virtual void OnStartTouchGround() override;
	virtual void OnStopTouchGround() override;
	virtual void OnChangeMoveType(MoveType_t oldMoveType) override;
	virtual void OnAirAcceleratePre(Vector &wishdir, f32 &wishspeed, f32 &accel) override;
	virtual void OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel) override;

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
	
	i32 currentCpIndex{};
	bool holdingStill{};
	f32 teleportTime{};

	CUtlVector<Checkpoint> checkpoints;

	void SetCheckpoint();
	void DoTeleport(i32 index);
	void TpHoldPlayerStill();
	void TpToCheckpoint();
	void TpToPrevCp();
	void TpToNextCp();

	// Jumpstats
	CUtlVector<Jump> jumps;
	bool jsAlways{};
	f32 lastJumpButtonTime{};
	f32 lastNoclipTime{};
	f32 lastDuckbugTime{};
	f32 lastGroundSpeedCappedTime{};
	f32 lastMovementProcessedTime{};
	
	void TrackJumpstatsVariables();
	// misc
	bool hideOtherPlayers{};
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
		void DrawSpeedPanel(KZPlayer *player);
	}
	namespace misc
	{
		void RegisterCommands();
		void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);
		void OnClientPutInServer(CPlayerSlot slot);
	}
};