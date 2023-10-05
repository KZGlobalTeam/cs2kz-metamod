#pragma once
#include "common.h"
#include "movement.h"

class CPlayerManager
{
public:
	CPlayerManager()
	{
		
	}

public:
	MovementPlayer *ToPlayer(CCSPlayer_MovementServices *ms);
	MovementPlayer *ToPlayer(CCSPlayerController *controller);
	MovementPlayer *ToPlayer(CCSPlayerPawn *pawn);
	MovementPlayer *ToPlayer(CPlayerSlot slot);
	MovementPlayer *ToPlayer(CEntityIndex entIndex);
public:
	MovementPlayer players[MAXPLAYERS + 1];
};

extern CPlayerManager *g_pPlayerManager;