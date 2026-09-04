// The jumpstats category in the options menu.
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/model.h"
#include "kz/option/menu/kz_menu.h"

#include "tier0/memdbgon.h"

// Indexed by DistanceTier, so a picked row id is the stored value.
static_global const char *const TIER_LABELS[DISTANCETIER_COUNT] = {"Menu - Tier None",    "Menu - Tier Meh",     "Menu - Tier Impressive",
																   "Menu - Tier Perfect", "Menu - Tier Godlike", "Menu - Tier Ownage",
																   "Menu - Tier Wrecker"};

// The tier preferences all behave identically; tag is the index into this table.
struct TierPref
{
	const char *prefKey;
	const char *serverDefaultKey;
	i32 fallback;
	const char *phraseKey;
};

// clang-format off
static_global const TierPref TIER_PREFS[] = {
	{"jsMinTier",                "defaultJSMinTier",                DistanceTier_Impressive, "Menu - JS Min Tier"},
	{"jsMinTierConsole",         "defaultJSMinTierConsole",         DistanceTier_Impressive, "Menu - JS Min Tier Console"},
	{"jsSoundMinTier",           "defaultJSSoundMinTier",           DistanceTier_Impressive, "Menu - JS Sound Min Tier"},
	{"jsBroadcastMinTier",       "defaultJSBroadcastMinTier",       DistanceTier_Ownage,     "Menu - JS Broadcast Min Tier"},
	{"jsBroadcastMinTierConsole","defaultJSBroadcastMinTierConsole",DistanceTier_Ownage,     "Menu - JS Broadcast Min Tier Console"},
	{"jsBroadcastSoundMinTier",  "defaultJSBroadcastSoundMinTier",  DistanceTier_Ownage,     "Menu - JS Broadcast Sound Min Tier"},
};
// clang-format on

static_function void GetTierChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < DISTANCETIER_COUNT; i++)
	{
		out.push_back({KZMenuService::GetPhrase(player, TIER_LABELS[i]), i, NULL});
	}
}

static_function i64 GetCurrentTier(KZPlayer *player, i64 tag)
{
	const TierPref &pref = TIER_PREFS[tag];
	return player->optionService->GetPreferenceInt(pref.prefKey, KZOptionService::GetOptionInt(pref.serverDefaultKey, pref.fallback));
}

static_function void PickTier(KZPlayer *player, i64 tag, i64 id)
{
	player->optionService->SetPreferenceInt(TIER_PREFS[tag].prefKey, id);
}

void KZJumpstatsService::RegisterMenu()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Jumpstats");
	KZ::menu::AddToggle(cat, "Menu - JS Reporting", "jsReporting", true);
	KZ::menu::AddToggle(cat, "Menu - JS Always", "jsAlways", false);
	KZ::menu::AddToggle(cat, "Menu - JS Extended Stats", "jsExtendedChatStats", false);
	KZ::menu::AddToggle(cat, "Menu - JS Failstats", "jsFailstats", true);
	KZ::menu::AddToggle(cat, "Menu - JS Failstats Console", "jsFailstatsConsole", true);
	KZ::menu::AddSize(cat, "Menu - JS Volume", "jsVolume", 75, 0, 200);
	KZ::menu::SetItemUnit(cat, "%");
	KZ::menu::SetItemScale(cat, 100);
	KZ::menu::SetItemSubtext(cat, "Menu - JS Volume Sub");
	KZ::menu::SetItemDivider(cat);

	for (i32 i = 0; i < KZ_ARRAYSIZE(TIER_PREFS); i++)
	{
		KZ::menu::AddChoice(cat, TIER_PREFS[i].phraseKey, GetTierChoices, GetCurrentTier, PickTier, i);
		KZ::menu::SetItemPref(cat, TIER_PREFS[i].prefKey, KZOptStorage::Int);
	}
}
