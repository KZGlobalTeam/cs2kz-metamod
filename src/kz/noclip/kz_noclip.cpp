#include "kz_noclip.h"

#include "../timer/kz_timer.h"

#include "utils/utils.h"
#include "utils/simplecmds.h"

void KZNoclipService::Reset()
{
	this->lastNoclipTime = {};
	this->inNoclip = {};
}

void KZNoclipService::HandleNoclip()
{
	CCSPlayerPawn *pawn = this->player->GetPawn();
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

internal SCMD_CALLBACK(Command_KzNoclip)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->noclipService->ToggleNoclip();
	player->PrintChat(true, false, "{grey}Noclip %s.", player->noclipService->IsNoclipping() ? "enabled" : "disabled");
	return MRES_SUPERCEDE;
}

void KZNoclipService::RegisterCommands()
{
	scmd::RegisterCmd("kz_noclip", Command_KzNoclip, "Toggle noclip.");
	scmd::RegisterCmd("kz_nc", Command_KzNoclip, "Toggle noclip.");
	scmd::RegisterCmd("noclip", Command_KzNoclip, "Toggle noclip.");
}

void KZNoclipService::HandleMoveCollision()
{
	CCSPlayerPawn *pawn = this->player->GetPawn();
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
