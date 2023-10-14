#pragma once

#include "common.h"
#include "movement/movement.h"
#include "utils/datatypes.h"

#define KZ_COLLISION_GROUP_STANDARD LAST_SHARED_COLLISION_GROUP
#define KZ_COLLISION_GROUP_NOTRIGGER COLLISION_GROUP_DEBRIS

extern CMovementPlayerManager *g_pPlayerManager;

class KZPlayer : public MovementPlayer
{
public:
	KZPlayer(int i) : MovementPlayer(i)
	{
		inNoclip = false;
		m_currentCpIndex = 0;
		m_checkpoints = CUtlVector<Checkpoint>(1, 0);
	}
	virtual void OnStartProcessMovement() override;
	virtual void OnStopProcessMovement() override;
	virtual void OnStartTouchGround() override;
		
private:
	bool inNoclip;
public:
	void DisableNoclip();
	void ToggleNoclip();
	void EnableGodMode();
	void HandleMoveCollision();
	void UpdatePlayerModelAlpha();
	void SetCheckpoint();
	void TpToCheckpoint();
	void TpToPrevCp();
	void TpToNextCp();
	
	struct Checkpoint
	{
		Vector origin;
		QAngle angles;
	};
	
	i32 m_currentCpIndex;
	CUtlVector<Checkpoint> m_checkpoints;
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

	KZPlayer *ToKZPlayer(MovementPlayer *player) { return static_cast<KZPlayer *>(player); }
};

namespace KZ
{
	CKZPlayerManager *GetKZPlayerManager();
	namespace HUD
	{
		void OnProcessUsercmds_Post(CPlayerSlot &slot, bf_read *buf, int numcmds, bool ignore, bool paused);
	}
	namespace misc
	{
		void RegisterCommands();
	}
};