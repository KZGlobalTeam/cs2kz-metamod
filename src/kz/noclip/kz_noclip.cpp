#include "kz_noclip.h"

#include "../timer/kz_timer.h"
#include "../language/kz_language.h"

#include "utils/utils.h"
#include "utils/simplecmds.h"

#define FL_NOCLIP (1 << 3)

void KZNoclipService::Reset()
{
	this->lastNoclipTime = {};
	this->inNoclip = {};
}

void KZNoclipService::HandleNoclip()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (this->inNoclip)
	{
		if ((pawn->m_fFlags() & FL_NOCLIP) == 0)
		{
			pawn->m_fFlags(pawn->m_fFlags() | FL_NOCLIP);
		}
		if (pawn->m_MoveType() != MOVETYPE_NOCLIP)
		{
			this->player->SetMoveType(MOVETYPE_NOCLIP);
			this->player->timerService->TimerStop();
		}
		// if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_NOTRIGGER)
		// {
		// 	pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_NOTRIGGER;
		// 	pawn->CollisionRulesChanged();
		// }
		this->lastNoclipTime = g_pKZUtils->GetServerGlobals()->curtime;
		this->player->timerService->TimerStop();
	}
	else
	{
		if ((pawn->m_fFlags() & FL_NOCLIP) != 0)
		{
			pawn->m_fFlags(pawn->m_fFlags() & ~FL_NOCLIP);
		}
		if (pawn->m_nActualMoveType() == MOVETYPE_NOCLIP)
		{
			this->player->SetMoveType(MOVETYPE_WALK);
			this->player->timerService->TimerStop();
		}
		if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
		{
			pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_STANDARD;
			pawn->CollisionRulesChanged();
		}
	}
}

// Commands

SCMD(kz_noclip, SCFL_PLAYER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->noclipService->ToggleNoclip();
	if (player->noclipService->IsNoclipping())
	{
		player->languageService->PrintChat(true, false, "Noclip - Enable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Noclip - Disable");
	}
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_nc, kz_noclip);
SCMD_LINK(noclip, kz_noclip);

void KZNoclipService::HandleMoveCollision()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	if (pawn->m_lifeState() != LIFE_ALIVE)
	{
		this->DisableNoclip();
		return;
	}
	this->HandleNoclip();
}
