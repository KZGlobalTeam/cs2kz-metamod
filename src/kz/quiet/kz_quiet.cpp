#include "cstrike15_usermessages.pb.h"
#include "usermessages.pb.h"
#include "gameevents.pb.h"
#include "cs_gameevents.pb.h"

#include "kz_quiet.h"

#include "utils/utils.h"
void KZ::quiet::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
{
	for (int i = 0; i < infoCount; i++)
	{
		// Cast it to our own TransmitInfo struct because CCheckTransmitInfo isn't correct.
		TransmitInfo *pTransmitInfo = reinterpret_cast<TransmitInfo*>(pInfo[i]);
		
		// Find out who this info will be sent to.
		CPlayerSlot targetSlot = pTransmitInfo->m_nClientEntityIndex;
		KZPlayer *targetPlayer = g_pKZPlayerManager->ToPlayer(targetSlot);

		CCSPlayerPawn *targetPlayerPawn = targetPlayer->GetPawn();
		CCSPlayerPawn *pawn = nullptr;

		while (nullptr != (pawn = (CCSPlayerPawn *)utils::FindEntityByClassname(pawn, "player")))
		{
			// If it's our pawn, don't hide them.
			if (targetPlayerPawn == pawn)
			{
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
	if (!ent) return;

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

bool KZQuietService::ShouldHide()
{
	if (!this->hideOtherPlayers) return false;

	// If the player is not alive and is in third person/free roam, don't hide other players.
	if (this->player->GetPawn()->m_lifeState() != LIFE_ALIVE 
		&& this->player->GetPawn()->m_pObserverServices() 
		&& this->player->GetPawn()->m_pObserverServices()->m_iObserverMode() != OBS_MODE_IN_EYE) return false;

	return true;
}

bool KZQuietService::ShouldHideIndex(u32 targetIndex)
{
	// Don't self-hide.
	if (this->player->index == targetIndex) return false;

	// Don't hide the spectated player.
	if (this->player->GetPawn()->m_pObserverServices()
		&& this->player->GetPawn()->m_pObserverServices()->m_hObserverTarget().GetEntryIndex() - 1 == targetIndex)
	{
		return false;
	}
	return true;
}