#include "../kz.h"
#include "kz_checkpoint.h"
#include "../option/kz_option.h"
#include "../timer/kz_timer.h"
#include "../noclip/kz_noclip.h"
#include "../language/kz_language.h"
#include "kz/trigger/kz_trigger.h"
#include "kz/racing/kz_racing.h"
#include "utils/utils.h"

static_global class KZOptionServiceEventListener_Checkpoint : public KZOptionServiceEventListener
{
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player)
	{
		player->checkpointService->OnPlayerPreferencesLoaded();
	}
} optionEventListener;

static_global const Vector NULL_VECTOR = Vector(0, 0, 0);

void KZCheckpointService::Init()
{
	KZOptionService::RegisterEventListener(&optionEventListener);
}

void KZCheckpointService::Reset()
{
	this->ResetCheckpoints();
	this->hasCustomStartPosition = false;
}

void KZCheckpointService::OnPlayerPreferencesLoaded()
{
	KeyValues3 ssps;
	player->optionService->GetPreferenceTable("startPositions", ssps);

	bool hasMapName = false;
	CUtlString currentMap = g_pKZUtils->GetCurrentMapName(&hasMapName);
	if (!hasMapName)
	{
		return;
	}
	if (KeyValues3 *startPos = ssps.FindMember(currentMap.Get()))
	{
		if (!startPos || !startPos->FindMember("origin") || !startPos->FindMember("angles") || !startPos->FindMember("ladderNormal")
			|| !startPos->FindMember("onLadder") || !startPos->FindMember("groundEnt") || !startPos->FindMember("slopeDropOffset")
			|| !startPos->FindMember("slopeDropHeight"))
		{
			return;
		}
		this->customStartPosition.origin = startPos->FindMember("origin")->GetVector();
		this->customStartPosition.angles = startPos->FindMember("angles")->GetQAngle();
		this->customStartPosition.ladderNormal = startPos->FindMember("ladderNormal")->GetVector();
		this->customStartPosition.onLadder = startPos->FindMember("onLadder")->GetBool();
		this->customStartPosition.groundEnt = CEntityHandle(startPos->FindMember("groundEnt")->GetUInt());
		this->customStartPosition.slopeDropOffset = startPos->FindMember("slopeDropOffset")->GetFloat();
		this->customStartPosition.slopeDropHeight = startPos->FindMember("slopeDropHeight")->GetFloat();
		this->hasCustomStartPosition = true;
	}

	player->checkpointService->checkpointSound = player->optionService->GetPreferenceBool("checkpointSound", true);
	player->checkpointService->teleportSound = player->optionService->GetPreferenceBool("teleportSound", true);
}

void KZCheckpointService::ResetCheckpoints(bool playSound, bool resetTeleports)
{
	if (playSound && this->GetCheckpointCount())
	{
		this->PlayCheckpointResetSound();
	}
	this->undoTeleportData = {};
	this->currentCpIndex = 0;
	if (resetTeleports)
	{
		this->tpCount = 0;
		this->holdingStill = false;
		this->teleportTime = 0.0f;
	}
	this->checkpoints.Purge();
}

void KZCheckpointService::SetCheckpoint()
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}

	if (this->player->triggerService->InAntiCpArea())
	{
		this->player->languageService->PrintChat(true, false, "Can't Checkpoint (Anti Checkpoint Area)");
		this->player->PlayErrorSound();
		return;
	}

	if (this->player->triggerService->InBhopTriggers())
	{
		this->player->languageService->PrintChat(true, false, "Can't Checkpoint (Just Landed)");
		this->player->PlayErrorSound();
		return;
	}

	u32 flags = pawn->m_fFlags();
	if (!(flags & FL_ONGROUND) && !(pawn->m_MoveType() == MOVETYPE_LADDER))
	{
		this->player->languageService->PrintChat(true, false, "Can't Checkpoint (Midair)");
		this->player->PlayErrorSound();
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
	if (player->optionService->GetPreferenceBool("checkpointMessage", true))
	{
		this->player->languageService->PrintChat(true, false, "Make Checkpoint", this->GetCheckpointCount());
	}
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
		this->player->PlayErrorSound();
		return;
	}
	if (!this->undoTeleportData.teleportOnGround)
	{
		this->player->languageService->PrintChat(true, false, "Can't Undo (TP Was Midair)");
		this->player->PlayErrorSound();
		return;
	}
	if (this->undoTeleportData.teleportInAntiCpTrigger)
	{
		this->player->languageService->PrintChat(true, false, "Can't Undo (AntiCp)");
		this->player->PlayErrorSound();
		return;
	}
	if (this->undoTeleportData.teleportInBhopTrigger)
	{
		this->player->languageService->PrintChat(true, false, "Can't Undo (Just Landed)");
		this->player->PlayErrorSound();
		return;
	}

	this->DoTeleport(this->undoTeleportData);
}

void KZCheckpointService::DoTeleport(i32 index)
{
	if (this->checkpoints.Count() <= 0)
	{
		this->player->languageService->PrintChat(true, false, "Can't Teleport (No Checkpoints)");
		this->player->PlayErrorSound();
		return;
	}
	if (!this->player->racingService->CanTeleport())
	{
		this->player->languageService->PrintChat(true, false, "Can't Teleport (Limit Reached)");
		this->player->PlayErrorSound();
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

	if (!this->player->triggerService->CanTeleportToCheckpoints())
	{
		this->player->languageService->PrintChat(true, false, "Can't Teleport (Map)");
		this->player->PlayErrorSound();
		return;
	}

	Vector currentOrigin;
	this->player->GetOrigin(&currentOrigin);

	// Update data for undoing teleports
	u32 flags = pawn->m_fFlags();
	this->undoTeleportData.teleportOnGround = ((flags & FL_ONGROUND) || (pawn->m_MoveType() == MOVETYPE_LADDER));
	this->undoTeleportData.teleportInAntiCpTrigger = this->player->triggerService->InAntiCpArea();
	this->undoTeleportData.teleportInBhopTrigger = this->player->triggerService->InBhopTriggers();
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
		this->player->PlayErrorSound();
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
		this->player->PlayErrorSound();
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
		this->player->PlayErrorSound();
		return;
	}
	this->hasCustomStartPosition = true;
	this->player->GetOrigin(&this->customStartPosition.origin);
	this->player->GetAngles(&this->customStartPosition.angles);
	this->customStartPosition.slopeDropHeight = pawn->m_flSlopeDropHeight();
	this->customStartPosition.slopeDropOffset = pawn->m_flSlopeDropOffset();
	this->customStartPosition.groundEnt = pawn->m_hGroundEntity();
	bool hasMapName = false;
	CUtlString currentMap = g_pKZUtils->GetCurrentMapName(&hasMapName);
	if (hasMapName)
	{
		KeyValues3 ssps;
		player->optionService->GetPreferenceTable("startPositions", ssps);
		KeyValues3 *startPos = ssps.FindOrCreateMember(currentMap.Get());

		startPos->FindOrCreateMember("origin")->SetVector(this->customStartPosition.origin);
		startPos->FindOrCreateMember("angles")->SetQAngle(this->customStartPosition.angles);
		startPos->FindOrCreateMember("ladderNormal")->SetVector(this->customStartPosition.ladderNormal);
		startPos->FindOrCreateMember("onLadder")->SetBool(this->customStartPosition.onLadder);
		startPos->FindOrCreateMember("groundEnt")->SetUInt(this->customStartPosition.groundEnt.ToInt());
		startPos->FindOrCreateMember("slopeDropOffset")->SetFloat(this->customStartPosition.slopeDropOffset);
		startPos->FindOrCreateMember("slopeDropHeight")->SetFloat(this->customStartPosition.slopeDropHeight);

		player->optionService->SetPreferenceTable("startPositions", ssps);
	}
	this->player->languageService->PrintChat(true, false, "Set Custom Start Position");
}

void KZCheckpointService::ClearStartPosition()
{
	this->hasCustomStartPosition = false;

	bool hasMapName = false;
	CUtlString currentMap = g_pKZUtils->GetCurrentMapName(&hasMapName);
	if (hasMapName)
	{
		KeyValues3 ssps;
		player->optionService->GetPreferenceTable("startPositions", ssps);
		ssps.RemoveMember(currentMap.Get());
		player->optionService->SetPreferenceTable("startPositions", ssps);
	}

	this->player->languageService->PrintChat(true, false, "Cleared Custom Start Position");
}

void KZCheckpointService::TpToStartPosition()
{
	this->DoTeleport(this->customStartPosition);
}

void KZCheckpointService::PlayCheckpointSound()
{
	if (this->checkpointSound)
	{
		utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_SND_SET_CP);
	}
}

void KZCheckpointService::PlayTeleportSound()
{
	if (this->teleportSound)
	{
		utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_SND_DO_TP);
	}
}

void KZCheckpointService::PlayCheckpointResetSound()
{
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_SND_RESET_CPS);
}
