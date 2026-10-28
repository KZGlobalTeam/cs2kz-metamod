#include "kz_spec.h"
#include "../timer/kz_timer.h"
#include "kz/language/kz_language.h"
#include "utils/simplecmds.h"
#include "utils/ctimer.h"

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
	this->player->Teleport(&this->savedOrigin, &this->savedAngles, nullptr);
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
		for (KZPlayer *otherPlayer : g_pKZPlayerManager->GetInGamePlayers())
		{
			if (this->player == otherPlayer)
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
			for (KZPlayer *otherPlayer : g_pKZPlayerManager->GetInGamePlayers())
			{
				if (this->player == otherPlayer)
				{
					continue;
				}

				if (V_stristr(otherPlayer->GetName(), playerName))
				{
					if (otherPlayer->GetController()->GetTeam() == CS_TEAM_SPECTATOR)
					{
						player->languageService->PrintChat(true, false, "Spectate Failure (Dead)");
						return false;
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
		return true;
	}

	this->SpectatePlayer(targetPlayer);
	return true;
}

static_function f64 TeleportObserver(CPlayerUserId userID, Vector origin, QAngle angles)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	if (player && player->GetObserverPawn())
	{
		player->GetObserverPawn()->Teleport(&origin, &angles, nullptr);
	}
	return 0.0f;
};

bool KZSpecService::SpectatePlayer(KZPlayer *target, bool force)
{
	if (!target || (!force && !this->CanSpectate()))
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

	obsService->m_iObserverMode(OBS_MODE_IN_EYE);
	obsService->m_iObserverLastMode(OBS_MODE_NONE);
	obsService->m_hObserverTarget(target->GetPlayerPawn());
	controller->m_DesiredObserverMode(OBS_MODE_IN_EYE);
	controller->m_hDesiredObserverTarget(target->GetPlayerPawn());

	if (target == this->player)
	{
		controller->m_DesiredObserverMode(OBS_MODE_ROAMING);
		obsService->m_iObserverMode(OBS_MODE_ROAMING);
		obsService->m_hObserverTarget(target->GetPlayerPawn());
		Vector origin;
		QAngle angles;
		this->player->GetEyeOrigin(&origin);
		this->player->GetAngles(&angles);
		StartTimer<CPlayerUserId, Vector, QAngle>(TeleportObserver, player->GetClient()->GetUserID(), std::move(origin), std::move(angles), 0.0f,
												  false, false);
	}
	return true;
}

bool KZSpecService::CanSpectate()
{
	return !this->player->IsAlive() || this->player->timerService->GetPaused() || this->player->timerService->CanPause();
}

void KZSpecService::GetSpectatorList(std::vector<std::string> &spectatorList)
{
	KZPlayer *spectator = this->player->specService->GetNextSpectator(nullptr);
	while (spectator)
	{
		spectatorList.push_back(spectator->GetName());
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
	for (i32 i = current ? current->GetPlayerSlot().Get() + 1 : 0; i < MAX_PLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(i));
		if (player->IsInGame() && player->specService->IsSpectating(this->player))
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
		return true;
	}

	// Handle explicit target, this always takes priority over automatic spectating.
	if (args->ArgC() >= 2)
	{
		player->specService->SpectatePlayer(args->Arg(1));
		return true;
	}

	// Find the first alive player other than the player themselves.
	KZPlayer *firstAlivePlayer = nullptr;
	for (KZPlayer *otherPlayer : g_pKZPlayerManager->GetInGamePlayers())
	{
		if (otherPlayer->IsAlive() && otherPlayer != player)
		{
			firstAlivePlayer = otherPlayer;
			break;
		}
	}

	// If no other player is alive, spectate themselves instead.
	if (!firstAlivePlayer)
	{
		player->specService->SpectatePlayer("@me");
		return true;
	}

	// If no target is provided, default to the first alive player.
	player->specService->SpectatePlayer(firstAlivePlayer);
	return true;
}

SCMD(kz_specs, SCFL_SPEC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	KZPlayer *targetPlayer = player->IsAlive() ? player : player->specService->GetSpectatedPlayer();

	if (!targetPlayer)
	{
		player->languageService->PrintChat(true, false, "Spectator List (None)");
		return true;
	}
	std::vector<std::string> spectatorList;
	targetPlayer->specService->GetSpectatorList(spectatorList);
	if (spectatorList.empty())
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
		std::string spectatorListString;
		for (size_t i = 0; i < spectatorList.size(); i++)
		{
			spectatorListString += spectatorList[i];
			if (i != spectatorList.size() - 1)
			{
				spectatorListString += ", ";
			}
		}
		if (targetPlayer == player)
		{
			player->languageService->PrintChat(true, false, "Spectator List", spectatorList.size(), spectatorListString.c_str());
		}
		else
		{
			player->languageService->PrintChat(true, false, "Target Spectator List", targetPlayer->GetName(), spectatorList.size(),
											   spectatorListString.c_str());
		}
	}
	return true;
}

SCMD_LINK(kz_speclist, kz_specs);
