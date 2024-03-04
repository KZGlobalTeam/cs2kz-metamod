#include "kz_spec.h"

#include "utils/simplecmds.h"

internal KZSpecServiceTimerEventListener timerEventListener;

void KZSpecService::Reset()
{
	this->ResetSavedPosition();
}

void KZSpecService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
}

bool KZSpecService::HasSavedPosition()
{
	return false;
}

void KZSpecService::SavePosition()
{
	this->player->GetOrigin(&this->savedOrigin);
	this->player->GetAngles(&this->savedAngles);
	this->savedOnLadder = this->player->GetMoveType() == MOVETYPE_LADDER;
	this->savedPosition = true;
}

void KZSpecService::LoadPosition()
{
	if (!this->HasSavedPosition())
	{
		return;
	}
	this->player->GetPawn()->Teleport(&this->savedOrigin, &this->savedAngles, nullptr);
	if (this->savedOnLadder)
	{
		this->player->GetPawn()->SetMoveType(MOVETYPE_LADDER);
	}
}

void KZSpecService::ResetSavedPosition()
{
	this->savedOrigin = vec3_origin;
	this->savedAngles = vec3_angle;
	this->savedOnLadder = false;
	this->savedPosition = false;
}

bool KZSpecService::IsSpectating(KZPlayer *target)
{
	return false;
}

bool KZSpecService::SpectatePlayer(const char *playerName)
{
	return false;
}

bool KZSpecService::CanSpectate()
{
	return false;
}

KZPlayer *KZSpecService::GetSpectatingPlayer()
{
	return nullptr;
}

void KZSpecServiceTimerEventListener::OnTimerStartPost(KZPlayer *player, const char *courseName)
{
	player->specService->Reset();
}

internal SCMD_CALLBACK(Command_KzSpec)
{
	return MRES_HANDLED;
}

void KZSpecService::RegisterCommands()
{
	scmd::RegisterCmd("kz_spec", Command_KzSpec, "TODO: Spectate a player.");
}
