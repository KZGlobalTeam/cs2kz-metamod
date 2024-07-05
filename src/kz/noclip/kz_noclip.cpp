#include "kz_noclip.h"

#include "../timer/kz_timer.h"
#include "../language/kz_language.h"

#include "utils/utils.h"
#include "utils/simplecmds.h"

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
			pawn->m_fFlags.Set(pawn->m_fFlags() | FL_NOCLIP);
		}
		if (pawn->m_MoveType() != MOVETYPE_NOCLIP)
		{
			this->player->SetMoveType(MOVETYPE_NOCLIP);
			this->player->timerService->TimerStop();
		}
		if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_NOTRIGGER)
		{
			pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_NOTRIGGER;
			pawn->CollisionRulesChanged();
		}
		this->lastNoclipTime = g_pKZUtils->GetServerGlobals()->curtime;
		this->player->timerService->TimerStop();
	}
	else
	{
		if ((pawn->m_fFlags() & FL_NOCLIP) != 0)
		{
			pawn->m_fFlags.Set(pawn->m_fFlags() & ~FL_NOCLIP);
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

static_function SCMD_CALLBACK(Command_KzNoclip)
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

void KZNoclipService::RegisterCommands()
{
	scmd::RegisterCmd("kz_noclip", Command_KzNoclip);
	scmd::RegisterCmd("kz_nc", Command_KzNoclip);
	scmd::RegisterCmd("noclip", Command_KzNoclip);
}

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
