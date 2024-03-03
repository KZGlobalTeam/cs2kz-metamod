#include "../kz.h"
#include "kz_checkpoint.h"
#include "../timer/kz_timer.h"
#include "utils/utils.h"

// TODO: replace printchat with HUD service's printchat

internal const Vector NULL_VECTOR = Vector(0, 0, 0);

void KZCheckpointService::Reset()
{
	this->ResetCheckpoints();
	this->hasCustomStartPosition = false;
}

void KZCheckpointService::ResetCheckpoints()
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
	if (!pawn)
	{
		return;
	}
	u32 flags = pawn->m_fFlags();
	if (!(flags & FL_ONGROUND) && !(pawn->m_MoveType() == MOVETYPE_LADDER))
	{
		utils::CPrintChat(this->player->GetController(), "%s {grey}Checkpoint unavailable in the air.", KZ_CHAT_PREFIX);
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
	utils::CPrintChat(this->player->GetPawn(), "%s {grey}Checkpoint ({default}#%i{grey})", KZ_CHAT_PREFIX, this->currentCpIndex);
	this->player->PlayCheckpointSound();
}

void KZCheckpointService::DoTeleport(i32 index)
{
	if (this->checkpoints.Count() <= 0)
	{
		utils::CPrintChat(this->player->GetController(), "%s {grey}No checkpoints available.", KZ_CHAT_PREFIX);
		return;
	}
	this->DoTeleport(this->checkpoints[this->currentCpIndex]);
}

void KZCheckpointService::DoTeleport(const Checkpoint &cp)
{
	CCSPlayerPawn *pawn = this->player->GetPawn();
	if (!pawn || !pawn->IsAlive())
	{
		return;
	}
	// If we teleport the player to the same origin, the player ends just a slightly bit off from where they are supposed to be...
	Vector currentOrigin;
	this->player->GetOrigin(&currentOrigin);
	// If we teleport the player to this origin every tick, they will end up NOT on this origin in the end somehow.
	// So we only set the player origin if it doesn't match.
	if (currentOrigin != cp.origin)
	{
		this->player->Teleport(&cp.origin, &cp.angles, &NULL_VECTOR);
		// Check if player might get stuck and attempt to put the player in duck.
		if (!utils::IsSpawnValid(cp.origin))
		{
			this->player->GetMoveServices()->m_bDucked(true);
			this->player->GetMoveServices()->m_flDuckAmount(1.0f);
		}
	}
	else
	{
		this->player->Teleport(NULL, &cp.angles, &NULL_VECTOR);
	}
	pawn->m_flSlopeDropHeight(cp.slopeDropHeight);
	pawn->m_flSlopeDropOffset(cp.slopeDropOffset);

	CBaseEntity2 *groundEntity = static_cast<CBaseEntity2 *>(GameEntitySystem()->GetBaseEntity(cp.groundEnt));
	// Don't attach the player onto moving platform (because they might not be there anymore). World doesn't move though.
	if (groundEntity && (groundEntity->entindex() == 0 || (groundEntity->m_vecBaseVelocity().Length() == 0.0f && groundEntity->m_vecAbsVelocity().Length() == 0.0f)))
	{
		pawn->m_hGroundEntity(cp.groundEnt);
	}

	CCSPlayer_MovementServices *ms = this->player->GetMoveServices();
	if (cp.onLadder)
	{
		ms->m_vecLadderNormal(cp.ladderNormal);
		if (!this->player->timerService->GetPaused())
		{
			this->player->GetPawn()->SetMoveType(MOVETYPE_LADDER);
		}
		else
		{
			this->player->timerService->SetPausedOnLadder(true);
		}
	}
	else
	{
		ms->m_vecLadderNormal(vec3_origin);
		if (this->player->timerService->GetPaused())
		{
			this->player->timerService->SetPausedOnLadder(false);
		}
	}

	this->tpCount++;
	this->teleportTime = g_pKZUtils->GetServerGlobals()->curtime;
	this->player->PlayTeleportSound();
	this->lastTeleportedCheckpoint = &cp;
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
	if (!this->lastTeleportedCheckpoint
		|| !this->player->IsAlive()
		|| g_pKZUtils->GetServerGlobals()->curtime - this->teleportTime > 0.04) 
	{
		return;
	}
	Vector currentOrigin;
	this->player->GetOrigin(&currentOrigin);

	// If we teleport the player to this origin every tick, they will end up NOT on this origin in the end somehow.
	if (currentOrigin != this->lastTeleportedCheckpoint->origin)
	{
		this->player->SetOrigin(this->lastTeleportedCheckpoint->origin);
		if (!utils::IsSpawnValid(this->lastTeleportedCheckpoint->origin))
		{
			this->player->GetMoveServices()->m_bDucked(true);
			this->player->GetMoveServices()->m_flDuckAmount(1.0f);
		}
	}
	this->player->SetVelocity(Vector(0, 0, 0));
	CCSPlayer_MovementServices *ms = this->player->GetMoveServices();
	if (this->lastTeleportedCheckpoint->onLadder && this->player->GetPawn()->m_MoveType() != MOVETYPE_NONE)
	{
		ms->m_vecLadderNormal(this->lastTeleportedCheckpoint->ladderNormal);
		this->player->GetPawn()->SetMoveType(MOVETYPE_LADDER);
	}
	else
	{
		ms->m_vecLadderNormal(vec3_origin);
	}
	if (this->lastTeleportedCheckpoint->groundEnt)
	{
		this->player->GetPawn()->m_fFlags(this->player->GetPawn()->m_fFlags | FL_ONGROUND);
	}
	CBaseEntity2 *groundEntity = static_cast<CBaseEntity2 *>(GameEntitySystem()->GetBaseEntity(this->lastTeleportedCheckpoint->groundEnt));
	if (groundEntity && (groundEntity->entindex() == 0 || (groundEntity->m_vecBaseVelocity().Length() == 0.0f && groundEntity->m_vecAbsVelocity().Length() == 0.0f)))
	{
		this->player->GetPawn()->m_hGroundEntity(this->lastTeleportedCheckpoint->groundEnt);
	}
}

void KZCheckpointService::SetStartPosition()
{
	CCSPlayerPawn *pawn = this->player->GetPawn();
	if (!pawn)
	{
		utils::CPrintChat(this->player->GetController(), "%s {grey} Failed to set your custom start position!", KZ_CHAT_PREFIX);
		return;
	}
	this->hasCustomStartPosition = true;
	this->player->GetOrigin(&this->customStartPosition.origin);
	this->player->GetAngles(&this->customStartPosition.angles);
	this->customStartPosition.slopeDropHeight = pawn->m_flSlopeDropHeight();
	this->customStartPosition.slopeDropOffset = pawn->m_flSlopeDropOffset();
	this->customStartPosition.groundEnt = pawn->m_hGroundEntity();
	utils::CPrintChat(this->player->GetController(), "%s {grey}You have set your custom start position.", KZ_CHAT_PREFIX);
}

void KZCheckpointService::ClearStartPosition()
{
	this->hasCustomStartPosition = false;
	utils::CPrintChat(this->player->GetController(), "%s {grey}You have cleared your custom start position.", KZ_CHAT_PREFIX);
}

void KZCheckpointService::TpToStartPosition()
{
	this->DoTeleport(this->customStartPosition);
}
