#include "../kz.h"
#include "kz_checkpoint.h"
#include "utils/utils.h"

// TODO: replace printchat with HUD service's printchat

internal const Vector NULL_VECTOR = Vector(0, 0, 0);

void KZCheckpointService::Reset()
{
	this->currentCpIndex = 0;
	this->tpCount = 0;
	this->holdingStill = false;
	this->teleportTime = 0.0f;
	this->checkpoints.Purge();
}

void KZCheckpointService::SetCheckpoint()
{
	CCSPlayerPawn *pawn = this->player->GetPawn();
	if (!pawn) return;
	u32 flags = pawn->m_fFlags();
	if (!(flags & FL_ONGROUND) && !(pawn->m_MoveType() == MOVETYPE_LADDER))
	{
		utils::PrintChat(this->player->GetController(), "Checkpoint unavailable in the air.");
		return;
	}
	
	Checkpoint cp = {};
	this->player->GetOrigin(&cp.origin);
	this->player->GetAngles(&cp.angles);
	cp.slopeDropHeight = pawn->m_flSlopeDropHeight();
	cp.slopeDropOffset = pawn->m_flSlopeDropOffset();
	if (this->player->GetMoveServices())
	{
		cp.ladderNormal = this->player->GetMoveServices()->m_vecLadderNormal();
		cp.onLadder = pawn->m_MoveType() == MOVETYPE_LADDER;
	}
	cp.groundEnt = pawn->m_hGroundEntity();
	this->checkpoints.AddToTail(cp);
	// newest checkpoints aren't deleted after using prev cp.
	this->currentCpIndex = this->checkpoints.Count() - 1;
	utils::PrintChat(this->player->GetPawn(), "Checkpoint (#%i)", this->currentCpIndex);
	this->player->PlayCheckpointSound();
}

void KZCheckpointService::DoTeleport(i32 index)
{
	CCSPlayerPawn *pawn = this->player->GetPawn();
	if (!pawn) return;
	if (checkpoints.Count() <= 0)
	{
		utils::PrintChat(this->player->GetController(), "No checkpoints available.");
		return;
	}
	const Checkpoint cp = this->checkpoints[this->currentCpIndex];

	// If we teleport the player to the same origin, the player ends just a slightly bit off from where they are supposed to be...
	Vector currentOrigin;
	this->player->GetOrigin(&currentOrigin);

	// If we teleport the player to this origin every tick, they will end up NOT on this origin in the end somehow.
	// So we only set the player origin if it doesn't match.
	if (currentOrigin != checkpoints[currentCpIndex].origin)
	{
		this->player->Teleport(&cp.origin, &cp.angles, &NULL_VECTOR);
	}
	else
	{
		this->player->Teleport(NULL, &cp.angles, &NULL_VECTOR);
	}
	pawn->m_flSlopeDropHeight(cp.slopeDropHeight);
	pawn->m_flSlopeDropOffset(cp.slopeDropOffset);

	pawn->m_hGroundEntity(cp.groundEnt);
	if (cp.onLadder)
	{
		CCSPlayer_MovementServices *ms = this->player->GetMoveServices();
		ms->m_vecLadderNormal(cp.ladderNormal);
		this->player->GetPawn()->m_MoveType(MOVETYPE_LADDER);
	}
	this->tpCount++;
	this->teleportTime = g_pKZUtils->GetServerGlobals()->curtime;
	this->player->PlayTeleportSound();
}

void KZCheckpointService::TpToCheckpoint()
{
	DoTeleport(this->currentCpIndex);
}

void KZCheckpointService::TpToPrevCp()
{
	this->currentCpIndex = MAX(0, this->currentCpIndex - 1);
	DoTeleport(this->currentCpIndex);
}

void KZCheckpointService::TpToNextCp()
{
	this->currentCpIndex = MIN(this->currentCpIndex + 1, this->checkpoints.Count() - 1);
	DoTeleport(this->currentCpIndex);
}

void KZCheckpointService::TpHoldPlayerStill()
{
	if (!checkpoints.IsValidIndex(currentCpIndex)) return;
	if (this->player->GetPawn()->m_lifeState() != LIFE_ALIVE) return;
	if (g_pKZUtils->GetServerGlobals()->curtime - this->teleportTime > 0.04) return;
	Vector currentOrigin;
	this->player->GetOrigin(&currentOrigin);

	// If we teleport the player to this origin every tick, they will end up NOT on this origin in the end somehow.
	if (currentOrigin != checkpoints[currentCpIndex].origin)
	{
		this->player->SetOrigin(checkpoints[currentCpIndex].origin);
	}
	this->player->SetVelocity(Vector(0,0,0));
	if (checkpoints[currentCpIndex].onLadder && this->player->GetPawn()->m_MoveType() != MOVETYPE_NONE)
	{
		this->player->GetPawn()->m_MoveType(MOVETYPE_LADDER);
	}
	if (checkpoints[currentCpIndex].groundEnt)
	{
		this->player->GetPawn()->m_fFlags(this->player->GetPawn()->m_fFlags | FL_ONGROUND);
	}
	this->player->GetPawn()->m_hGroundEntity(checkpoints[currentCpIndex].groundEnt);
}