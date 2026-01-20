#include "cstrike15_usermessages.pb.h"
#include "usermessages.pb.h"
#include "gameevents.pb.h"
#include "cs_gameevents.pb.h"

#include "sdk/entity/cparticlesystem.h"
#include "sdk/services.h"

#include "kz_quiet.h"
#include "kz/beam/kz_beam.h"
#include "kz/measure/kz_measure.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"

#include "utils/utils.h"
#include "utils/simplecmds.h"

static_global class KZOptionServiceEventListener_Quiet : public KZOptionServiceEventListener
{
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player)
	{
		player->quietService->OnPlayerPreferencesLoaded();
	}
} optionEventListener;

void KZ::quiet::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
{
	for (int i = 0; i < infoCount; i++)
	{
		// Cast it to our own TransmitInfo struct because CCheckTransmitInfo isn't correct.
		TransmitInfo *pTransmitInfo = reinterpret_cast<TransmitInfo *>(pInfo[i]);

		// Find out who this info will be sent to.
		uintptr_t targetAddr = reinterpret_cast<uintptr_t>(pTransmitInfo) + g_pGameConfig->GetOffset("QuietPlayerSlot");
		CPlayerSlot targetSlot = CPlayerSlot(*reinterpret_cast<int *>(targetAddr));
		KZPlayer *targetPlayer = g_pKZPlayerManager->ToPlayer(targetSlot);
		// Make sure the target isn't CSTV.
		CCSPlayerController *targetController = targetPlayer->GetController();
		if (!targetController || targetController->m_bIsHLTV)
		{
			continue;
		}
		targetPlayer->quietService->UpdateHideState();
		CCSPlayerPawn *targetPlayerPawn = targetPlayer->GetPlayerPawn();

		EntityInstanceByClassIter_t iterParticleSystem(NULL, "info_particle_system");

		for (CParticleSystem *particleSystem = static_cast<CParticleSystem *>(iterParticleSystem.First()); particleSystem;
			 particleSystem = static_cast<CParticleSystem *>(iterParticleSystem.Next()))
		{
			if (particleSystem->m_iTeamNum() != CUSTOM_PARTICLE_SYSTEM_TEAM)
			{
				continue; // Only hide custom particle systems created by the plugin.
			}
			if (targetPlayer->beamService->playerBeam == particleSystem->GetRefEHandle()
				|| targetPlayer->beamService->playerBeamNew == particleSystem->GetRefEHandle())
			{
				// Don't hide the beam for the owner.
				continue;
			}
			if (targetPlayer->measureService->measurerHandle == particleSystem->GetRefEHandle())
			{
				// Don't hide the measure beam for the owner.
				continue;
			}
			pTransmitInfo->m_pTransmitEdict->Clear(particleSystem->GetEntityIndex().Get());
		}

		EntityInstanceByClassIter_t iter(NULL, "player");
		// clang-format off
		for (CCSPlayerPawn *pawn = static_cast<CCSPlayerPawn *>(iter.First());
			 pawn != NULL;
			 pawn = pawn->m_pEntity->m_pNextByClass ? static_cast<CCSPlayerPawn *>(pawn->m_pEntity->m_pNextByClass->m_pInstance) : nullptr)
		// clang-format on
		{
			if (targetPlayerPawn == pawn && targetPlayer->quietService->ShouldHideWeapon())
			{
				auto pVecWeapons = pawn->m_pWeaponServices->m_hMyWeapons();

				FOR_EACH_VEC(*pVecWeapons, i)
				{
					auto pWeapon = (*pVecWeapons)[i].Get();

					if (pWeapon)
					{
						pTransmitInfo->m_pTransmitEdict->Clear(pWeapon->entindex());
					}
				}
			}
			// Bit is not even set, don't bother.
			if (!pTransmitInfo->m_pTransmitEdict->IsBitSet(pawn->entindex()))
			{
				continue;
			}
			// Do not transmit a pawn without any controller to prevent crashes.
			if (!pawn->m_hController().IsValid())
			{
				pTransmitInfo->m_pTransmitEdict->Clear(pawn->entindex());
				continue;
			}
			// Respawn must be enabled or !hide will cause client crash.
#if 0
			// Never send dead players to prevent crashes.
			if (pawn->m_lifeState() != LIFE_ALIVE)
			{
				pTransmitInfo->m_pTransmitEdict->Clear(pawn->entindex());
				continue;
			}
#endif
			// Finally check if player is using !hide.
			if (!targetPlayer->quietService->ShouldHide())
			{
				continue;
			}
			u32 index = g_pKZPlayerManager->ToPlayer(pawn)->index;
			if (targetPlayer->quietService->ShouldHideIndex(index))
			{
				pTransmitInfo->m_pTransmitEdict->Clear(pawn->entindex());
			}
		}
	}
}

static void FilterQuietClients(const uint64 *clients, u32 emitterPlayerIndex = 0)
{
	for (i32 recipientPlayerIndex = 1; recipientPlayerIndex < MAXPLAYERS + 1; recipientPlayerIndex++)
	{
		KZPlayer *recipient = g_pKZPlayerManager->ToPlayer(recipientPlayerIndex);
		if (recipient->quietService->ShouldHide() && (emitterPlayerIndex == 0 || recipient->quietService->ShouldHideIndex(emitterPlayerIndex)))
		{
			*(uint64 *)clients &= ~(1ull << (recipientPlayerIndex - 1));
		}
	}
}

void KZ::quiet::OnPostEvent(INetworkMessageInternal *pEvent, const CNetMessage *pData, const uint64 *clients)
{
	NetMessageInfo_t *info = pEvent->GetNetMessageInfo();

	u32 emitterEntIndex = 0;
	switch (info->m_MessageId)
	{
		// Hide bullet decals, and sound.
		case GE_FireBulletsId:
		{
			auto msg = const_cast<CNetMessage *>(pData)->ToPB<CMsgTEFireBullets>();
			emitterEntIndex = msg->player() & 0x3FFF;
			break;
		}
		// Hide reload sounds.
		case CS_UM_WeaponSound:
		{
			auto msg = const_cast<CNetMessage *>(pData)->ToPB<CCSUsrMsg_WeaponSound>();
			emitterEntIndex = msg->entidx();
			break;
		}
		// Hide other sounds from player (eg. armor equipping)
		case GE_SosStartSoundEvent:
		{
			auto msg = const_cast<CNetMessage *>(pData)->ToPB<CMsgSosStartSoundEvent>();
			emitterEntIndex = msg->source_entity_index();
			break;
		}
		// Used by kz_misc to block valve's player say messages.
		case CS_UM_SayText:
		case UM_SayText:
		{
			if (!KZOptionService::GetOptionInt("overridePlayerChat", true))
			{
				return;
			}
			auto msg = const_cast<CNetMessage *>(pData)->ToPB<CUserMessageSayText>();
			i32 index = msg->playerindex();
			if (index == -1)
			{
				return;
			}
			{
				*(uint64 *)clients = 0;
			}
			return;
		}
		case CS_UM_SayText2:
		case UM_SayText2:
		{
			if (!KZOptionService::GetOptionInt("overridePlayerChat", true))
			{
				return;
			}
			auto msg = const_cast<CNetMessage *>(pData)->ToPB<CUserMessageSayText2>();
			i32 index = msg->entityindex();
			if (index == -1)
			{
				return;
			}
			if (!msg->mutable_param1()->empty() || !msg->mutable_param2()->empty())
			{
				*(uint64 *)clients = 0;
			}
			return;
		}
		default:
		{
			return;
		}
	}
	CBaseEntity *emitterEnt = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(CEntityIndex(emitterEntIndex)));
	if (emitterEntIndex == 0 || !emitterEnt)
	{
		return;
	}

	// Convert this entindex into the index in the player controller.
	if (emitterEnt->IsPawn())
	{
		CBasePlayerPawn *pawn = static_cast<CBasePlayerPawn *>(emitterEnt);
		u32 emitterPlayerIndex = g_pKZPlayerManager->ToPlayer(utils::GetController(pawn))->index;
		FilterQuietClients(clients, emitterPlayerIndex);
	}
	else if (V_strstr(emitterEnt->GetClassname(), "weapon_"))
	{
		// Find the owner of the weapon if possible.
		if (emitterEnt->m_hOwnerEntity().IsValid() && emitterEnt->m_hOwnerEntity.Get()->IsPawn())
		{
			CBasePlayerPawn *ownerPawn = static_cast<CBasePlayerPawn *>(emitterEnt->m_hOwnerEntity().Get());
			u32 emitterPlayerIndex = g_pKZPlayerManager->ToPlayer(ownerPawn)->index;
			FilterQuietClients(clients, emitterPlayerIndex);
		}
		// Otherwise just hide from everyone having !hide enabled.
		else
		{
			FilterQuietClients(clients);
		}
	}
	// Special case for the armor sound upon spawning/respawning, because the emitter player is not yet known.
	else if (V_strcmp(emitterEnt->GetClassname(), "item_assaultsuit") == 0)
	{
		FilterQuietClients(clients);
	}
}

void KZQuietService::Init()
{
	KZOptionService::RegisterEventListener(&optionEventListener);
}

void KZQuietService::Reset()
{
	this->hideOtherPlayers = this->player->optionService->GetPreferenceBool("hideOtherPlayers", false);
	this->hideWeapon = this->player->optionService->GetPreferenceBool("hideWeapon", false);
}

void KZQuietService::SendFullUpdate()
{
	if (CServerSideClient *client = g_pKZUtils->GetClientBySlot(this->player->GetPlayerSlot()))
	{
		client->ForceFullUpdate();
	}
	// Keep the player's angles the same.
	QAngle angles;
	this->player->GetAngles(&angles);
	this->player->SetAngles(angles);
}

bool KZQuietService::ShouldHide()
{
	if (!this->hideOtherPlayers)
	{
		return false;
	}

	// If the player is not alive and not spectating another player, don't hide other players.
	if (!this->player->IsAlive() && !this->player->specService->GetSpectatedPlayer())
	{
		return false;
	}

	return true;
}

bool KZQuietService::ShouldHideIndex(u32 targetIndex)
{
	// Don't self-hide.
	if (this->player->index == targetIndex)
	{
		return false;
	}

	// Don't hide the player being spectated.
	if (this->player->specService->GetSpectatedPlayer() && this->player->specService->GetSpectatedPlayer()->index == targetIndex)
	{
		return false;
	}

	return true;
}

SCMD(kz_hideweapon, SCFL_PLAYER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->quietService->ToggleHideWeapon();
	return MRES_SUPERCEDE;
}

void KZQuietService::ToggleHideWeapon()
{
	this->hideWeapon = !this->hideWeapon;
	this->SendFullUpdate();
	this->player->optionService->SetPreferenceBool("hideWeapon", this->hideWeapon);
	this->player->languageService->PrintChat(true, false,
											 this->hideWeapon ? "Quiet Option - Show Weapon - Disable" : "Quiet Option - Show Weapon - Enable");
}

void KZQuietService::OnPhysicsSimulatePost() {}

void KZQuietService::OnPlayerPreferencesLoaded()
{
	this->hideWeapon = this->player->optionService->GetPreferenceBool("hideWeapon", false);
	if (this->hideWeapon)
	{
		this->SendFullUpdate();
	}
	bool newShouldHide = this->player->optionService->GetPreferenceBool("hideOtherPlayers", false);
	if (!newShouldHide && this->hideOtherPlayers && this->player->IsInGame())
	{
		this->SendFullUpdate();
	}
	this->hideOtherPlayers = newShouldHide;
}

void KZQuietService::ToggleHide()
{
	this->hideOtherPlayers = !this->hideOtherPlayers;
	this->player->optionService->SetPreferenceBool("hideOtherPlayers", this->hideOtherPlayers);
	if (!this->hideOtherPlayers)
	{
		this->SendFullUpdate();
	}
}

void KZQuietService::UpdateHideState()
{
	CPlayer_ObserverServices *obsServices = this->player->GetController()->m_hPawn()->m_pObserverServices;
	if (!obsServices)
	{
		this->lastObserverMode = OBS_MODE_NONE;
		this->lastObserverTarget.Term();
		return;
	}
	// Nuclear option, define this if things crash still!
#if 0
	if (obsServices->m_iObserverMode() != this->lastObserverMode || obsServices->m_hObserverTarget() != this->lastObserverTarget)
	{
		this->SendFullUpdate();
	}
#endif
	this->lastObserverMode = obsServices->m_iObserverMode();
	this->lastObserverTarget = obsServices->m_hObserverTarget();
}
