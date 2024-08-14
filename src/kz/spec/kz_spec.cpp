#include "kz_spec.h"

#include "utils/simplecmds.h"

static_global KZTimerServiceEventListener_Spec timerEventListener;

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
	this->player->GetPlayerPawn()->Teleport(&this->savedOrigin, &this->savedAngles, nullptr);
	if (this->savedOnLadder)
	{
		this->player->SetMoveType(MOVETYPE_LADDER);
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
	return this->GetSpectatedPlayer() == target;
}

bool KZSpecService::SpectatePlayer(const char *playerName)
{
	return false;
}

bool KZSpecService::SpectatePlayer(KZPlayer *playerToSpec) 
{
	// Some of the time, this works all of the time.
	this->player->GetController()->SwitchTeam(CS_TEAM_SPECTATOR);

	CPlayer_ObserverServices *obsService = this->player->GetController()->m_hObserverPawn()->m_pObserverServices;
	obsService->m_hObserverTarget.Set(playerToSpec->GetController()->m_hPlayerPawn.Get());

	return true;
}

bool KZSpecService::CanSpectate()
{
	return false;
}

KZPlayer *KZSpecService::GetSpectatedPlayer()
{
	if (!player || player->IsAlive())
	{
		return NULL;
	}
	if (!player->GetController() || !player->GetController()->m_hObserverPawn())
	{
		return NULL;
	}
	CPlayer_ObserverServices *obsService = player->GetController()->m_hObserverPawn()->m_pObserverServices;
	if (!obsService)
	{
		return NULL;
	}
	if (!obsService->m_hObserverTarget().IsValid())
	{
		return NULL;
	}
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	CBasePlayerPawn *target = (CBasePlayerPawn *)obsService->m_hObserverTarget().Get();
	// If the player is spectating their own corpse, consider that as not spectating anyone.
	return target == pawn ? nullptr : g_pKZPlayerManager->ToPlayer(target);
}

KZPlayer *KZSpecService::GetNextSpectator(KZPlayer *current)
{
	for (int i = current ? current->index + 1 : 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (player->specService->IsSpectating(this->player))
		{
			return player;
		}
	}
	return nullptr;
}

void KZTimerServiceEventListener_Spec::OnTimerStartPost(KZPlayer *player, const char *courseName)
{
	player->specService->Reset();
}

static_function SCMD_CALLBACK(Command_KzSpec)
{
	return MRES_HANDLED;
}

void KZSpecService::RegisterCommands()
{
	scmd::RegisterCmd("kz_spec", Command_KzSpec);
}
