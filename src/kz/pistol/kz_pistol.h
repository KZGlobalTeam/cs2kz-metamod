#pragma once
#include "../kz.h"
#include "utils/eventlisteners.h"

struct PistolInfo_t
{
	i16 itemDef;
	i8 team;
	const char *name;
	const char *className;
	std::vector<const char *> aliases;
};

class KZPistolServiceEventListener
{
public:
	virtual void OnWeaponGiven(KZPlayer *player, CBasePlayerWeapon *weapon) {}
};

class KZPistolService : public KZBaseService
{
public:
	// clang-format off
	static inline const std::vector<PistolInfo_t> pistols = {
		{0, CS_TEAM_NONE, "Knife", "weapon_knife", {"knife", "disable", "disabled", "none", "off", "0"}},
		{1, CS_TEAM_NONE, "Desert Eagle", "weapon_deagle", {"deagle", "deag", "desert_eagle", "deserteagle", "desert-eagle", "de"}},
		{2, CS_TEAM_NONE, "Dual Berettas", "weapon_elite", {"elite", "dualies", "dual_elites", "dual_berettas", "dual-elites", "dual-berettas", "dualelites", "dualberettas", "berettas", "dual berettas"}},
		{3, CS_TEAM_CT, "Five-SeveN", "weapon_fiveseven", {"fiveseven", "five_seven", "five7", "five_7", "five-7", "5seven", "5_seven", "5-seven", "57", "5-7", "5_7"}},
		{4, CS_TEAM_T, "Glock-18", "weapon_glock", {"glock", "glock18", "glock_18", "glock-18"}},
		{30, CS_TEAM_T, "Tec-9", "weapon_tec9", {"tec9", "tec-9", "tec_9"}},
		{32, CS_TEAM_CT, "P2000", "weapon_hkp2000", {"hkp2000", "p2k", "p2000"}},
		{36, CS_TEAM_NONE, "P250", "weapon_p250", {"p250"}},
		{61, CS_TEAM_CT, "USP-S", "weapon_usp_silencer", {"usps", "usp-s", "usp"}},
		{63, CS_TEAM_NONE, "CZ75-Auto", "weapon_cz75a", {"cz75-auto", "cz75a", "cz75", "cz-75", "cz_75", "cz", "cz75_auto", "cz-75_auto", "cz_75_auto"}},
		{64, CS_TEAM_NONE, "R8 Revolver", "weapon_revolver", {"r8 revolver", "r8revolver", "revolver", "r8", "r8_revolver"}}};
	// clang-format on

	static int GetPistolIndexByName(const char *name)
	{
		if (!name)
		{
			return 0;
		}
		for (i16 i = 0; i < pistols.size(); i++)
		{
			if (KZ_STREQI(pistols[i].name, name) || KZ_STREQI(pistols[i].className, name))
			{
				return i;
			}
			for (const char *alias : pistols[i].aliases)
			{
				if (KZ_STREQI(alias, name))
				{
					return i;
				}
			}
		}
		return 0;
	}

	static int GetPistolIndexByItemDef(i16 itemDef)
	{
		for (i16 i = 0; i < pistols.size(); i++)
		{
			if (pistols[i].itemDef == itemDef)
			{
				return i;
			}
		}
		return 0;
	}

	using KZBaseService::KZBaseService;

	DECLARE_CLASS_EVENT_LISTENER(KZPistolServiceEventListener);

	static void Init();

	virtual void Reset() override
	{
		this->preferredPistol = 8; // Default to USP-S
	}

	void OnPlayerJoinTeam()
	{
		this->UpdatePistol();
	}

	void UpdatePistol();
	bool HasCorrectPistolEquipped();
	// Return true if the player has a weapon that isn't a knife.
	bool NeedWeaponStripping();
	i16 preferredPistol = 8; // Default to USP-S
};
