#include "common.h"
#include "kz_goto.h"
#include "utils/simplecmds.h"
#include "utils/utils.h"

#include "../language/kz_language.h"
#include "../timer/kz_timer.h"
#include "kz/away/kz_away.h"

void KZGotoService::Init() {}

void KZGotoService::Reset() {}

// The first player other than the given one whose name matches. An away player's pawn stays in the world and can be gone to.
static_function KZPlayer *FindGotoTarget(KZPlayer *player, const char *namePart, bool exact, bool persistentPawn)
{
	for (KZPlayer *otherPlayer : g_pKZPlayerManager->GetAllSlots())
	{
		if (player == otherPlayer || !(otherPlayer->IsInGame() || otherPlayer->IsAway())
			|| otherPlayer->awayService->CanControlPawn() == persistentPawn)
		{
			continue;
		}
		if (exact ? !KZ_STREQI(otherPlayer->GetName(), namePart) : !V_stristr(otherPlayer->GetName(), namePart))
		{
			continue;
		}
		if (otherPlayer->GetController()->GetTeam() == CS_TEAM_SPECTATOR && !otherPlayer->awayService->IsObservingWithPersistentPawn())
		{
			continue;
		}
		return otherPlayer;
	}
	return nullptr;
}

bool KZGotoService::GotoPlayer(const char *playerNamePart)
{
	if (!playerNamePart || !V_stricmp("", playerNamePart))
	{
		player->languageService->PrintChat(true, false, "Goto - Command Usage");
		return false;
	}

	if (this->player->timerService->GetTimerRunning())
	{
		this->player->languageService->PrintChat(true, false, "Goto - Error Message (Timer Running)");
		return false;
	}

	// Prefer exact matches over partial matches, and players in control of their pawn over pawns that persist on their own.
	KZPlayer *targetPlayer = nullptr;
	for (i32 pass = 0; pass < 4 && !targetPlayer; pass++)
	{
		targetPlayer = FindGotoTarget(this->player, playerNamePart, pass % 2 == 0, pass >= 2);
	}

	if (!targetPlayer)
	{
		player->languageService->PrintChat(true, false, "Error Message (Player Not Found)", playerNamePart);
		return false;
	}

	this->player->awayService->RestorePersistentPawn();
	if (this->player->GetController()->GetTeam() == CS_TEAM_SPECTATOR)
	{
		this->player->GetController()->SwitchTeam(CS_TEAM_CT);
		this->player->GetController()->Respawn();
	}

	CCSPlayer_MovementServices *ms = this->player->GetMoveServices();

	if (targetPlayer->GetMoveType() == MOVETYPE_LADDER)
	{
		ms->m_vecLadderNormal(targetPlayer->GetMoveServices()->m_vecLadderNormal());
		this->player->SetMoveType(MOVETYPE_LADDER);
	}
	else
	{
		ms->m_vecLadderNormal(vec3_origin);
	}

	Vector origin;
	QAngle angles;
	targetPlayer->GetOrigin(&origin);
	targetPlayer->GetAngles(&angles);

	this->player->Teleport(&origin, &angles, &vec3_origin);
	this->player->languageService->PrintChat(true, false, "Goto - Teleported", targetPlayer->GetName());
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (pawn && pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
	{
		pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_STANDARD;
		pawn->CollisionRulesChanged();
	}
	return true;
}

SCMD(kz_goto, SCFL_PLAYER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	const char *targetNamePart = args->ArgS();
	player->gotoService->GotoPlayer(targetNamePart);
	return true;
}
