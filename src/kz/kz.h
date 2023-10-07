#pragma once

#include "common.h"
#include "movement/movement.h"
#include "utils/datatypes.h"

extern CMovementPlayerManager *g_pPlayerManager;

class KZPlayer : public MovementPlayer
{

};

class CKZPlayerManager : public CMovementPlayerManager
{

};

namespace KZ
{
	KZPlayer *ToKZPlayer(MovementPlayer *player);
	
	namespace HUD
	{
		void OnProcessUsercmds_Post(CPlayerSlot &slot, bf_read *buf, int numcmds, bool ignore, bool paused);
	}
	namespace misc
	{
		void EnableGodMode(CPlayerSlot slot);
		void InitPlayerManager();
	}
};