// The general category in the options menu.
#include "kz/kz.h"
#include "kz/beam/kz_beam.h"
#include "kz/fov/kz_fov.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/quiet/kz_quiet.h"
#include "kz/style/kz_style.h"
#include "kz/pistol/kz_pistol.h"
#include "kz/option/menu/model.h"
#include "kz/option/menu/kz_menu.h"

#include "tier0/memdbgon.h"

// Indexed by KZBeamService::BeamType, so a picked row id is the stored value.
static_global const char *const BEAM_LABELS[] = {"Menu - Beam None", "Menu - Beam Ground", "Menu - Beam Feet"};

static_function void GetBeamChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < KZ_ARRAYSIZE(BEAM_LABELS); i++)
	{
		out.push_back({KZMenuService::GetPhrase(player, BEAM_LABELS[i]), i, NULL});
	}
}

static_function i64 GetCurrentBeam(KZPlayer *player, i64)
{
	return player->optionService->GetPreferenceInt("desiredBeamType", KZBeamService::BEAM_NONE);
}

static_function void PickBeam(KZPlayer *player, i64, i64 id)
{
	player->beamService->SetBeamType((u8)id);
	player->optionService->SetPreferenceInt("desiredBeamType", id);
}

// The pistol preference stores a class name; the row id is the index into the static table.
static_function void GetPistolChoices(KZPlayer *, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < (i32)KZPistolService::pistols.size(); i++)
	{
		out.push_back({KZPistolService::pistols[i].name, i, NULL});
	}
}

static_function i64 GetCurrentPistol(KZPlayer *player, i64)
{
	// The pistol in hand, for the same reason the mode row reads the live mode.
	return player->pistolService->preferredPistol;
}

static_function void PickPistol(KZPlayer *player, i64, i64 id)
{
	if (id < 0 || id >= (i64)KZPistolService::pistols.size())
	{
		return;
	}
	player->pistolService->preferredPistol = (i16)id;
	player->pistolService->UpdatePistol();
	player->optionService->SetPreferenceStr("preferredPistol", KZPistolService::pistols[id].className);
}

// Only the modes registered on this server are offered, so the row id is an index into that list
// and the current mode is matched by name.
static_function void GetModeChoices(KZPlayer *, i64, std::vector<KZChoice> &out)
{
	const CUtlVector<KZModeManager::ModePluginInfo> &modes = KZModeManager::GetModes();
	FOR_EACH_VEC(modes, i)
	{
		out.push_back({modes[i].longModeName.Get(), i, NULL});
	}
}

static_function i64 GetCurrentMode(KZPlayer *player, i64)
{
	// The live mode, not the stored preference: someone who never picked one is on the server default.
	const char *current = player->modeService->GetModeName();
	const CUtlVector<KZModeManager::ModePluginInfo> &modes = KZModeManager::GetModes();
	FOR_EACH_VEC(modes, i)
	{
		if (KZ_STREQI(modes[i].longModeName.Get(), current) || KZ_STREQI(modes[i].shortModeName.Get(), current))
		{
			return i;
		}
	}
	return -1;
}

static_function void PickMode(KZPlayer *player, i64, i64 id)
{
	const CUtlVector<KZModeManager::ModePluginInfo> &modes = KZModeManager::GetModes();
	if (id >= 0 && id < modes.Count())
	{
		g_pKZModeManager->SwitchToMode(player, modes[id].longModeName.Get());
	}
}

// Styles stack, so picking one toggles it rather than replacing the selection.
static_function void GetStyleChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	const CUtlString current = g_pKZStyleManager->GetStylesString(player);
	const CUtlVector<KZStyleManager::StylePluginInfo> &styles = KZStyleManager::GetStyles();
	FOR_EACH_VEC(styles, i)
	{
		KZChoice choice {styles[i].longName, i, NULL};
		choice.selected = V_stristr(current.Get(), styles[i].longName) != NULL;
		out.push_back(choice);
	}
}

static_function void PickStyle(KZPlayer *player, i64, i64 id)
{
	const CUtlVector<KZStyleManager::StylePluginInfo> &styles = KZStyleManager::GetStyles();
	if (id >= 0 && id < styles.Count())
	{
		g_pKZStyleManager->ToggleStyle(player, styles[id].longName);
	}
}

static_function void GetLanguageChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	const auto &languages = KZLanguageService::GetAvailableLanguages();
	for (i32 i = 0; i < (i32)languages.size(); i++)
	{
		// Each language names itself. A missing phrase comes back as its own key, which is the cue to
		// fall back to the Steam name from config.txt.
		char key[64];
		V_snprintf(key, sizeof(key), "Menu - Lang %s", languages[i].code.Get());
		std::string name = KZMenuService::GetPhrase(player, key);
		if (name == key)
		{
			name = languages[i].name.Get();
			name[0] = toupper(name[0]);
		}
		char label[64];
		V_snprintf(label, sizeof(label), "%s (%i%%)", name.c_str(), languages[i].coverage);
		out.push_back({label, i, NULL});
	}
}

static_function i64 GetCurrentLanguage(KZPlayer *player, i64)
{
	const char *current = player->languageService->GetLanguage();
	const auto &languages = KZLanguageService::GetAvailableLanguages();
	for (i32 i = 0; i < (i32)languages.size(); i++)
	{
		if (V_stricmp(languages[i].code.Get(), current) == 0)
		{
			return i;
		}
	}
	return -1;
}

static_function i64 GetHideOtherPlayersState(KZPlayer *player, i64)
{
	return player->optionService->GetPreferenceBool("hideOtherPlayers", false) ? 1 : 0;
}

static_function void ToggleHideOtherPlayers(KZPlayer *player, i64)
{
	player->quietService->ToggleHide();
}

static_function i64 GetHideWeaponState(KZPlayer *player, i64)
{
	return player->optionService->GetPreferenceBool("hideWeapon", false) ? 1 : 0;
}

static_function void ToggleHideWeapon(KZPlayer *player, i64)
{
	player->quietService->ToggleHideWeapon();
}

static_function void PickLanguage(KZPlayer *player, i64, i64 id)
{
	const auto &languages = KZLanguageService::GetAvailableLanguages();
	if (id >= 0 && id < (i64)languages.size())
	{
		player->optionService->SetPreferenceStr("preferredLanguage", languages[id].code.Get());
		KZLanguageService::UpdateLanguage(player->GetSteamId64(false), languages[id].code.Get(),
										  KZLanguageService::LanguageInfo::CacheLevel::CACHE_OVERRIDE, true);
	}
}

void KZ::misc::RegisterMenu()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Misc");
	KZ::menu::AddChoice(cat, "Menu - Mode", GetModeChoices, GetCurrentMode, PickMode);
	KZ::menu::SetItemPref(cat, "preferredMode", KZOptStorage::Str);
	KZ::menu::AddChoice(cat, "Menu - Styles", GetStyleChoices, NULL, PickStyle);
	KZ::menu::SetItemPref(cat, "preferredStyles", KZOptStorage::Str);
	KZ::menu::SetItemDivider(cat);
	KZ::menu::AddChoice(cat, "Menu - Pistol", GetPistolChoices, GetCurrentPistol, PickPistol);
	KZ::menu::SetItemPref(cat, "preferredPistol", KZOptStorage::Str);
	KZ::menu::AddChoice(cat, "Menu - Beam", GetBeamChoices, GetCurrentBeam, PickBeam);
	KZ::menu::SetItemPref(cat, "desiredBeamType", KZOptStorage::Int);
	KZ::menu::AddVector(cat, "Menu - Beam Offset", "beamOffset", KZBeamService::defaultOffset, -64, 64);
	KZ::menu::SetItemSubtext(cat, "Menu - Beam Offset Sub");
	KZ::menu::AddChoice(cat, "Menu - Language", GetLanguageChoices, GetCurrentLanguage, PickLanguage);
	KZ::menu::SetItemPref(cat, "preferredLanguage", KZOptStorage::Str);
	KZ::menu::SetItemDivider(cat);
	KZ::menu::AddActionToggle(cat, "Menu - Hide Other Players", GetHideOtherPlayersState, ToggleHideOtherPlayers);
	KZ::menu::SetItemPref(cat, "hideOtherPlayers", KZOptStorage::Bool);
	KZ::menu::AddActionToggle(cat, "Menu - Hide Weapon", GetHideWeaponState, ToggleHideWeapon);
	KZ::menu::SetItemPref(cat, "hideWeapon", KZOptStorage::Bool);
	KZ::menu::AddToggle(cat, "Menu - Hide Legs", "hideLegs", false);
	KZ::menu::AddToggle(cat, "Menu - Show Tips", "showTips", true);
	// The bounds come from the server config, which is already loaded by the time services register.
	KZ::menu::AddSize(cat, "Menu - FOV", "fov", KZFOVService::GetDefaultFOV(), KZFOVService::GetMinFOV(), KZFOVService::GetMaxFOV());
	KZ::menu::SetItemPref(cat, "fov", KZOptStorage::Int);
	KZ::menu::SetItemUnit(cat, "");
}
