#include "kz/pistol/kz_pistol.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"

#include "utils/simplecmds.h"
#include "icvar.h"
#include "sdk/cskeletoninstance.h"

IMPLEMENT_CLASS_EVENT_LISTENER(KZPistolService, KZPistolServiceEventListener);

static_global class : public KZOptionServiceEventListener
{
	void OnPlayerPreferencesLoaded(KZPlayer *player) override
	{
		player->pistolService->preferredPistol =
			KZPistolService::GetPistolIndexByName(player->optionService->GetPreferenceStr("preferredPistol", "weapon_usp_silencer"));
		player->pistolService->UpdatePistol();
	}
} optionEventListener;

void KZPistolService::Init()
{
	KZOptionService::RegisterEventListener(&optionEventListener);
}

SCMD(kz_pistol, SCFL_PREFERENCE | SCFL_MISC | SCFL_PLAYER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (args->ArgC() < 2)
	{
		player->languageService->PrintChat(true, false, "Pistol Command Usage");
		return MRES_SUPERCEDE;
	}
	const char *weapon = args->ArgS();
	i16 pistolIndex = KZPistolService::GetPistolIndexByName(weapon);
	if (pistolIndex == -1)
	{
		player->languageService->PrintChat(true, false, "Pistol Unknown", weapon);
		return MRES_SUPERCEDE;
	}
	player->pistolService->preferredPistol = pistolIndex;
	player->pistolService->UpdatePistol();
	player->optionService->SetPreferenceStr("preferredPistol", KZPistolService::pistols[pistolIndex].className);
	if (pistolIndex == 0)
	{
		player->languageService->PrintChat(true, false, "Pistol Disabled");
		return MRES_SUPERCEDE;
	}
	player->languageService->PrintChat(true, false, "Pistol Changed", KZPistolService::pistols[pistolIndex].name);
	return MRES_SUPERCEDE;
}

void KZPistolService::UpdatePistol()
{
	if (!player->IsAlive() || !player->IsInGame())
	{
		return;
	}
	if (preferredPistol == 0)
	{
		if (this->NeedWeaponStripping())
		{
			this->player->GetPlayerPawn()->m_pItemServices()->StripPlayerWeapons(false);
			auto weapon = this->player->GetPlayerPawn()->m_pItemServices()->GiveNamedItem(
				this->player->GetController()->m_iTeamNum() == CS_TEAM_CT ? "weapon_knife" : "weapon_knife_t");
			CALL_FORWARD(eventListeners, OnWeaponGiven, this->player, weapon);
		}
		return;
	}
	if (this->HasCorrectPistolEquipped())
	{
		return;
	}
	const PistolInfo_t &pistol = pistols[preferredPistol];
	i32 originalTeam = player->GetController()->m_iTeamNum();
	i32 otherTeam = originalTeam == CS_TEAM_CT ? CS_TEAM_T : CS_TEAM_CT;
	bool switchTeam = false;
	if (pistol.team == CS_TEAM_CT && originalTeam == CS_TEAM_T)
	{
		switchTeam = true;
	}
	else if (pistol.team == CS_TEAM_T && originalTeam == CS_TEAM_CT)
	{
		switchTeam = true;
	}
	else if (pistol.team == CS_TEAM_NONE && !this->player->IsFakeClient())
	{
		// Check the player's inventory. If there's a skin on this current team, don't switch. Otherwise, switch team.
		bool checkOtherTeam = true;
		CCSPlayerInventory *inventory = this->player->GetController()->m_pInventoryServices()->GetInventory();
		// LOADOUT_POSITION_SECONDARY0 to LOADOUT_POSITION_SECONDARY5
		for (i32 i = 2; i <= 7; i++)
		{
			if (inventory->m_loadoutItems[originalTeam][i].definitionIndex == pistol.itemDef
				&& inventory->m_loadoutItems[originalTeam][i].itemID != 0)
			{
				checkOtherTeam = false;
				break;
			}
		}
		if (checkOtherTeam)
		{
			for (i32 i = 2; i <= 7; i++)
			{
				if (inventory->m_loadoutItems[otherTeam][i].definitionIndex == pistol.itemDef && inventory->m_loadoutItems[otherTeam][i].itemID != 0)
				{
					switchTeam = true;
					break;
				}
			}
		}
	}
	this->player->GetPlayerPawn()->m_pItemServices()->StripPlayerWeapons(false);
	auto knife = this->player->GetPlayerPawn()->m_pItemServices()->GiveNamedItem(
		this->player->GetController()->m_iTeamNum == CS_TEAM_CT ? "weapon_knife" : "weapon_knife_t");
	if (switchTeam)
	{
		player->GetPlayerPawn()->m_iTeamNum(otherTeam);
	}
	auto weapon = this->player->GetPlayerPawn()->m_pItemServices()->GiveNamedItem(pistol.className);
	CALL_FORWARD(eventListeners, OnWeaponGiven, this->player, weapon);

	if (switchTeam)
	{
		player->GetPlayerPawn()->m_iTeamNum(originalTeam);
	}
}

bool KZPistolService::HasCorrectPistolEquipped()
{
	if (!player->IsAlive() || !player->IsInGame())
	{
		return false;
	}

	auto weapons = player->GetPlayerPawn()->m_pWeaponServices()->m_hMyWeapons();
	FOR_EACH_VEC(*weapons, i)
	{
		CBaseModelEntity *weapon = (*weapons)[i].Get();
		if (KZ_STREQI(weapon->GetClassname(), pistols[preferredPistol].className))
		{
			return true;
		}
	}
	return false;
}

bool KZPistolService::NeedWeaponStripping()
{
	if (!player->IsAlive() || !player->IsInGame())
	{
		return false;
	}

	auto weapons = player->GetPlayerPawn()->m_pWeaponServices()->m_hMyWeapons();
	FOR_EACH_VEC(*weapons, i)
	{
		CBaseModelEntity *weapon = (*weapons)[i].Get();
		if (weapon && (KZ_STREQI(weapon->GetClassname(), "weapon_knife") || KZ_STREQI(weapon->GetClassname(), "weapon_knife_t")))
		{
			continue;
		}
		return true;
	}
	return false;
}
