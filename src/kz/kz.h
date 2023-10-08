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
	}
	virtual void OnProcessMovement() override;
		
private:
	bool inNoclip;
public:
	void DisableNoclip();
	void ToggleNoclip();
	void EnableGodMode();
	void HandleMoveCollision();
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
	KZPlayer *ToPlayer(CCSPlayerPawn *pawn);
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
		META_RES OnClientCommand(CPlayerSlot &slot, const CCommand &args);
	}
};