#include "kz_spec.h"
#include "../timer/kz_timer.h"
#include "kz/language/kz_language.h"
#include "utils/simplecmds.h"

static_global class KZTimerServiceEventListener_Spec : public KZTimerServiceEventListener
{
	virtual void OnTimerStartPost(KZPlayer *player, u32 courseGUID) override;
} timerEventListener;

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
	return this->savedPosition;
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
	CCSPlayerController *controller = this->player->GetController();
	KZPlayer *targetPlayer = nullptr;
	if (KZ_STREQI(playerName, "@me"))
	{
		if (!this->player->IsAlive())
		{
			this->player->languageService->PrintChat(true, false, "Spectate Failure (Dead)");
			return false;
		}
		targetPlayer = this->player;
	}
	else
	{
		// Prefer exact matches over partial matches.
		for (i32 i = 0; i <= MAXPLAYERS; i++)
		{
			CBasePlayerController *controller = g_pKZPlayerManager->players[i]->GetController();
			KZPlayer *otherPlayer = g_pKZPlayerManager->ToPlayer(i);

			if (!controller || this->player == otherPlayer)
			{
				continue;
			}

			if (KZ_STREQI(otherPlayer->GetName(), playerName))
			{
				if (otherPlayer->GetController()->GetTeam() == CS_TEAM_SPECTATOR)
				{
					continue;
				}
				targetPlayer = otherPlayer;
				break;
			}
		}
		// If no exact match was found, try partial matches.
		if (!targetPlayer)
		{
			for (i32 i = 0; i <= MAXPLAYERS; i++)
			{
				CBasePlayerController *controller = g_pKZPlayerManager->players[i]->GetController();
				KZPlayer *otherPlayer = g_pKZPlayerManager->ToPlayer(i);

				if (!controller || this->player == otherPlayer)
				{
					continue;
				}

				if (V_strstr(V_strlower((char *)otherPlayer->GetName()), V_strlower((char *)playerName)))
				{
					if (otherPlayer->GetController()->GetTeam() == CS_TEAM_SPECTATOR)
					{
						player->languageService->PrintChat(true, false, "Spectate Failure (Dead)");
						return MRES_SUPERCEDE;
					}
					targetPlayer = otherPlayer;
					break;
				}
			}
		}
	}

	if (!targetPlayer)
	{
		player->languageService->PrintChat(true, false, "Spectate Failure (Player Not Found)", playerName);
		return MRES_SUPERCEDE;
	}

	this->SpectatePlayer(targetPlayer);
	return true;
}

bool KZSpecService::SpectatePlayer(KZPlayer *target)
{
	if (!target || !this->CanSpectate())
	{
		return false;
	}
	// Join spectator team if not already in it.
	CCSPlayerController *controller = this->player->GetController();
	if (controller->GetTeam() != CS_TEAM_SPECTATOR)
	{
		KZ::misc::JoinTeam(this->player, CS_TEAM_SPECTATOR, true);
	}

	CPlayer_ObserverServices *obsService = player->GetController()->m_hObserverPawn()->m_pObserverServices;
	if (!obsService)
	{
		player->languageService->PrintChat(true, false, "Spectate Failure (Generic)");
		return false;
	}
	// This needs to be set if the player spectates themself, so that the camera position is correct.
	controller->m_DesiredObserverMode(OBS_MODE_IN_EYE);
	controller->m_hDesiredObserverTarget(target->GetPlayerPawn());

	obsService->m_iObserverMode(OBS_MODE_IN_EYE);
	obsService->m_iObserverLastMode(OBS_MODE_NONE);
	obsService->m_hObserverTarget(target->GetPlayerPawn());
	return true;
}

bool KZSpecService::CanSpectate()
{
	return !this->player->IsAlive() || this->player->timerService->GetPaused() || this->player->timerService->CanPause();
}

void KZSpecService::GetSpectatorList(CUtlVector<CUtlString> &spectatorList)
{
	KZPlayer *spectator = this->player->specService->GetNextSpectator(nullptr);
	while (spectator)
	{
		spectatorList.AddToTail(spectator->GetName());
		spectator = this->player->specService->GetNextSpectator(spectator);
	}
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

void KZTimerServiceEventListener_Spec::OnTimerStartPost(KZPlayer *player, u32 courseGUID)
{
	player->specService->Reset();
}

SCMD(kz_spec, SCFL_SPEC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player->specService->CanSpectate())
	{
		player->languageService->PrintChat(true, false, "Spectate Failure (Generic)");
		return MRES_SUPERCEDE;
	}
	u32 numAlivePlayers = 0;
	for (i32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *otherPlayer = g_pKZPlayerManager->ToPlayer(i);
		if (otherPlayer && otherPlayer->IsAlive() && otherPlayer != player)
		{
			numAlivePlayers++;
		}
	}
	if (numAlivePlayers == 0 && args->ArgC() == 1)
	{
		player->specService->SpectatePlayer("@me");
		return MRES_SUPERCEDE;
	}
	if (args->ArgC() < 2)
	{
		player->languageService->PrintChat(true, false, "Spec Command Usage", args->ArgS());
		return MRES_SUPERCEDE;
	}

	player->specService->SpectatePlayer(args->Arg(1));

	return MRES_SUPERCEDE;
}

SCMD(kz_specs, SCFL_SPEC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	KZPlayer *targetPlayer = player->IsAlive() ? player : player->specService->GetSpectatedPlayer();

	if (!targetPlayer)
	{
		player->languageService->PrintChat(true, false, "Spectator List (None)");
		return MRES_SUPERCEDE;
	}
	CUtlVector<CUtlString> spectatorList;
	targetPlayer->specService->GetSpectatorList(spectatorList);
	if (spectatorList.Count() == 0)
	{
		if (targetPlayer == player)
		{
			player->languageService->PrintChat(true, false, "Spectator List (None)");
		}
		else
		{
			player->languageService->PrintChat(true, false, "Target Spectator List (None)", targetPlayer->GetName());
		}
	}
	else
	{
		CUtlString spectatorListString;
		for (i32 i = 0; i < spectatorList.Count(); i++)
		{
			spectatorListString += spectatorList[i];
			if (i != spectatorList.Count() - 1)
			{
				spectatorListString += ", ";
			}
		}
		if (targetPlayer == player)
		{
			player->languageService->PrintChat(true, false, "Spectator List", spectatorList.Count(), spectatorListString.Get());
		}
		else
		{
			player->languageService->PrintChat(true, false, "Target Spectator List", targetPlayer->GetName(), spectatorList.Count(),
											   spectatorListString.Get());
		}
	}
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_speclist, kz_specs);
