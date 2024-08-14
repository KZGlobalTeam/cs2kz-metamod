#include "../kz.h"
#include "kz_checkpoint.h"
#include "../timer/kz_timer.h"
#include "../noclip/kz_noclip.h"
#include "../language/kz_language.h"
#include "utils/utils.h"

// TODO: replace printchat with HUD service's printchat

static_global const Vector NULL_VECTOR = Vector(0, 0, 0);

void KZCheckpointService::Reset()
{
	this->ResetCheckpoints();
	this->hasCustomStartPosition = false;
}

void KZCheckpointService::ResetCheckpoints()
{
	this->undoTeleportData = {};
	this->currentCpIndex = 0;
	this->tpCount = 0;
	this->holdingStill = false;
	this->teleportTime = 0.0f;
	this->checkpoints.Purge();
}

void KZCheckpointService::SetCheckpoint()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	u32 flags = pawn->m_fFlags();
	if (!(flags & FL_ONGROUND) && !(pawn->m_MoveType() == MOVETYPE_LADDER))
	{
		this->player->languageService->PrintChat(true, false, "Can't Checkpoint (Midair)");
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
	this->player->languageService->PrintChat(true, false, "Make Checkpoint", this->GetCheckpointCount());
	this->PlayCheckpointSound();
}

void KZCheckpointService::UndoTeleport()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn || !pawn->IsAlive())
	{
		return;
	}

	if (this->checkpoints.Count() <= 0 || this->undoTeleportData.origin.IsZero() || this->tpCount <= 0)
	{
		this->player->languageService->PrintChat(true, false, "Can't Undo (No Teleports)");
		return;
	}
	if (!this->undoTeleportData.teleportOnGround)
	{
		this->player->languageService->PrintChat(true, false, "Can't Undo (TP Was Midair)");
		return;
	}
	if (this->undoTeleportData.teleportInAntiCpTrigger)
	{
		this->player->languageService->PrintChat(true, false, "Can't Undo (AntiCp)");
		return;
	}

	this->DoTeleport(this->undoTeleportData);
}

void KZCheckpointService::DoTeleport(i32 index)
{
	if (this->checkpoints.Count() <= 0)
	{
		this->player->languageService->PrintChat(true, false, "Can't Teleport (No Checkpoints)");
		return;
	}
	this->DoTeleport(this->checkpoints[this->currentCpIndex]);
}

void KZCheckpointService::DoTeleport(const Checkpoint cp)
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn || !pawn->IsAlive())
	{
		return;
	}

	Vector currentOrigin;
	this->player->GetOrigin(&currentOrigin);

	// Update data for undoing teleports
	u32 flags = pawn->m_fFlags();
	this->undoTeleportData.teleportOnGround = ((flags & FL_ONGROUND) || (pawn->m_MoveType() == MOVETYPE_LADDER));
	this->undoTeleportData.origin = currentOrigin;
	this->player->GetAngles(&this->undoTeleportData.angles);
	this->undoTeleportData.slopeDropHeight = pawn->m_flSlopeDropHeight();
	this->undoTeleportData.slopeDropOffset = pawn->m_flSlopeDropOffset();
	if (this->player->GetMoveServices())
	{
		this->undoTeleportData.ladderNormal = this->player->GetMoveServices()->m_vecLadderNormal();
		this->undoTeleportData.onLadder = pawn->m_MoveType() == MOVETYPE_LADDER;
	}
	this->undoTeleportData.groundEnt = pawn->m_hGroundEntity();

	this->player->noclipService->DisableNoclip();

	// If we teleport the player to the same origin,
	// the player ends just a slightly bit off from where they are supposed to be...
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

	CBaseEntity *groundEntity = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(cp.groundEnt));
	// Don't attach the player onto moving platform (because they might not be there anymore). World doesn't move
	// though.
	if (groundEntity
		&& (groundEntity->entindex() == 0
			|| (groundEntity->m_vecBaseVelocity().Length() == 0.0f && groundEntity->m_vecAbsVelocity().Length() == 0.0f)))
	{
		pawn->m_hGroundEntity(cp.groundEnt);
	}

	CCSPlayer_MovementServices *ms = this->player->GetMoveServices();
	if (cp.onLadder)
	{
		ms->m_vecLadderNormal(cp.ladderNormal);
		if (!this->player->timerService->GetPaused())
		{
			this->player->SetMoveType(MOVETYPE_LADDER);
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
	this->PlayTeleportSound();
	this->lastTeleportedCheckpoint = cp;
}

void KZCheckpointService::TpToCheckpoint()
{
	DoTeleport(this->currentCpIndex);
}

void KZCheckpointService::TpToPrevCp()
{
	if (this->checkpoints.Count() <= 0)
	{
		this->player->languageService->PrintChat(true, false, "Can't Teleport (No Checkpoints)");
		return;
	}
	this->currentCpIndex = MAX(0, this->currentCpIndex - 1);
	DoTeleport(this->currentCpIndex);
}

void KZCheckpointService::TpToNextCp()
{
	if (this->checkpoints.Count() <= 0)
	{
		this->player->languageService->PrintChat(true, false, "Can't Teleport (No Checkpoints)");
		return;
	}
	this->currentCpIndex = MIN(this->currentCpIndex + 1, this->checkpoints.Count() - 1);
	DoTeleport(this->currentCpIndex);
}

void KZCheckpointService::TpHoldPlayerStill()
{
	bool isAlive = this->player->IsAlive();
	bool justTeleported = g_pKZUtils->GetServerGlobals()->curtime - this->teleportTime > 0.04;

	if (!isAlive || justTeleported)
	{
		return;
	}

	Vector currentOrigin;
	this->player->GetOrigin(&currentOrigin);

	// If we teleport the player to this origin every tick, they will end up NOT on this origin in the end somehow.
	if (currentOrigin != this->lastTeleportedCheckpoint.origin)
	{
		this->player->SetOrigin(this->lastTeleportedCheckpoint.origin);
		if (!utils::IsSpawnValid(this->lastTeleportedCheckpoint.origin))
		{
			this->player->GetMoveServices()->m_bDucked(true);
			this->player->GetMoveServices()->m_flDuckAmount(1.0f);
		}
	}
	this->player->SetVelocity(Vector(0, 0, 0));
	CCSPlayer_MovementServices *ms = this->player->GetMoveServices();
	if (this->lastTeleportedCheckpoint.onLadder && this->player->GetPlayerPawn()->m_MoveType() != MOVETYPE_NONE)
	{
		ms->m_vecLadderNormal(this->lastTeleportedCheckpoint.ladderNormal);
		this->player->SetMoveType(MOVETYPE_LADDER);
	}
	else
	{
		ms->m_vecLadderNormal(vec3_origin);
	}
	if (this->lastTeleportedCheckpoint.groundEnt)
	{
		this->player->GetPlayerPawn()->m_fFlags(this->player->GetPlayerPawn()->m_fFlags | FL_ONGROUND);
	}
	CBaseEntity *groundEntity = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(this->lastTeleportedCheckpoint.groundEnt));

	if (!groundEntity)
	{
		return;
	}

	bool isWorldEntity = groundEntity->entindex() == 0;
	bool isStaticGround = groundEntity->m_vecBaseVelocity().Length() == 0.0f && groundEntity->m_vecAbsVelocity().Length() == 0.0f;

	if (isWorldEntity || isStaticGround)
	{
		this->player->GetPlayerPawn()->m_hGroundEntity(this->lastTeleportedCheckpoint.groundEnt);
	}
}

void KZCheckpointService::SetStartPosition()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn)
	{
		this->player->languageService->PrintChat(true, false, "Can't Set Custom Start Position (Generic)");
		return;
	}
	this->hasCustomStartPosition = true;
	this->player->GetOrigin(&this->customStartPosition.origin);
	this->player->GetAngles(&this->customStartPosition.angles);
	this->customStartPosition.slopeDropHeight = pawn->m_flSlopeDropHeight();
	this->customStartPosition.slopeDropOffset = pawn->m_flSlopeDropOffset();
	this->customStartPosition.groundEnt = pawn->m_hGroundEntity();
	this->player->languageService->PrintChat(true, false, "Set Custom Start Position");
}

void KZCheckpointService::ClearStartPosition()
{
	this->hasCustomStartPosition = false;
	this->player->languageService->PrintChat(true, false, "Cleared Custom Start Position");
}

void KZCheckpointService::TpToStartPosition()
{
	this->DoTeleport(this->customStartPosition);
}

void KZCheckpointService::PlayCheckpointSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_SND_SET_CP);
}

void KZCheckpointService::PlayTeleportSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_SND_DO_TP);
}
