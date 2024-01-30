#include "cstrike15_usermessages.pb.h"
#include "gameevents.pb.h"
#include "cs_gameevents.pb.h"

#include "sdk/services.h"

#include "kz_quiet.h"

#include "utils/utils.h"

void KZ::quiet::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
{
	for (int i = 0; i < infoCount; i++)
	{
		// Cast it to our own TransmitInfo struct because CCheckTransmitInfo isn't correct.
		TransmitInfo *pTransmitInfo = reinterpret_cast<TransmitInfo *>(pInfo[i]);

		// Find out who this info will be sent to.
		CPlayerSlot targetSlot = pTransmitInfo->m_nClientEntityIndex;
		KZPlayer *targetPlayer = g_pKZPlayerManager->ToPlayer(targetSlot);
		targetPlayer->quietService->UpdateHideState();
		CCSPlayerPawn *targetPlayerPawn = targetPlayer->GetPawn();

		EntityInstanceByClassIter_t iter("player");
		for (CCSPlayerPawn *pawn = static_cast<CCSPlayerPawn *>(iter.First());
			pawn != NULL; 
			pawn = pawn->m_pEntity->m_pNextByClass ? static_cast<CCSPlayerPawn *>(pawn->m_pEntity->m_pNextByClass->m_pInstance) : nullptr)
		{
			if (targetPlayerPawn == pawn)
			{
				// Hide weapon stuff.
				if (targetPlayer->quietService->ShouldHideWeapon())
				{
					for (u32 j = 0; j < 3; j++)
					{
						if (!pawn->m_pViewModelServices->m_hViewModel[j].IsValid())
						{
							continue;
						}
						pTransmitInfo->m_pTransmitEdict->Clear(pawn->m_pViewModelServices->m_hViewModel[j].GetEntryIndex());
					}
				}
				continue;
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
			// Never send dead players to prevent crashes.
			if (pawn->m_lifeState() != LIFE_ALIVE)
			{
				pTransmitInfo->m_pTransmitEdict->Clear(pawn->entindex());
				continue;
			}
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

void KZ::quiet::OnPostEvent(INetworkSerializable *pEvent, const void *pData, const uint64 *clients)
{
	NetMessageInfo_t *info = pEvent->GetNetMessageInfo();
	u32 entIndex, playerIndex;

	switch (info->m_MessageId)
	{
		// Hide bullet decals, and sound.
		case GE_FireBulletsId:
		{
			CMsgTEFireBullets *msg = (CMsgTEFireBullets *)pData;
			entIndex = msg->player() & 0x3FFF;
			break;
		}
		// Hide reload sounds.
		case CS_UM_WeaponSound:
		{
			CCSUsrMsg_WeaponSound *msg = (CCSUsrMsg_WeaponSound *)pData;
			entIndex = msg->entidx();
			break;
		}
		// Hide other sounds from player (eg. armor equipping)
		case GE_SosStartSoundEvent:
		{
			CMsgSosStartSoundEvent *msg = (CMsgSosStartSoundEvent *)pData;
			entIndex = msg->source_entity_index();
			break;
		}
		default:
		{
			return;
		}
	}
	CBaseEntity2 *ent = static_cast<CBaseEntity2 *>(g_pEntitySystem->GetBaseEntity(CEntityIndex(entIndex)));
	if (!ent)
	{
		return;
	}

	// Convert this entindex into the index in the player controller.
	if (ent->IsPawn())
	{
		CBasePlayerPawn *pawn = static_cast<CBasePlayerPawn *>(ent);
		playerIndex = g_pKZPlayerManager->ToPlayer(utils::GetController(pawn))->index;
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (g_pKZPlayerManager->ToPlayer(i)->quietService->ShouldHide()
				&& g_pKZPlayerManager->ToPlayer(i)->quietService->ShouldHideIndex(playerIndex))
			{
				*(uint64 *)clients &= ~i;
			}
		}
	}
	// Special case for the armor sound upon spawning/respawning.
	else if (V_strcmp(ent->GetClassname(), "item_assaultsuit") == 0 || V_strstr(ent->GetClassname(), "weapon_"))
	{
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (g_pKZPlayerManager->ToPlayer(i)->quietService->ShouldHide())
			{
				*(uint64 *)clients &= ~i;
			}
		}
	}
}

void KZQuietService::Reset()
{
	this->hideOtherPlayers = false;
}

void KZQuietService::SendFullUpdate()
{
	auto slots = *(void ***)((char *)g_pNetworkServerService->GetIGameServer() + offsets::ClientOffset);
	*(uint32_t *)((char *)slots[this->player->GetPlayerSlot().Get()] + offsets::ACKOffset) = -1;
}

bool KZQuietService::ShouldHide()
{
	if (!this->hideOtherPlayers)
	{
		return false;
	}

	// If the player is not alive, don't hide other players.
	if (this->player->GetController()->m_hPawn()->m_lifeState() != LIFE_ALIVE)
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

	return true;
}

void KZQuietService::ToggleHide()
{
	this->hideOtherPlayers = !this->hideOtherPlayers;
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
	if (obsServices->m_iObserverMode() != this->lastObserverMode || obsServices->m_hObserverTarget() != this->lastObserverTarget)
	{
		// Nuclear option, uncomment this if things crash still!
		// this->SendFullUpdate();
	}
	this->lastObserverMode = obsServices->m_iObserverMode();
	this->lastObserverTarget = obsServices->m_hObserverTarget();
}

void KZQuietService::ToggleHideWeapon()
{
	this->hideWeapon = !this->hideWeapon;
}
