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

// JSReportType
static_global const char *const REPORT_TYPE_LABELS[JSREPORTTYPE_COUNT] = {"Menu - JS Report Type HUD", "Menu - JS Report Type Chat",
																		  "Menu - JS Report Type Both"};

static_function void GetReportTypeChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < JSREPORTTYPE_COUNT; i++)
	{
		out.push_back({KZMenuService::GetPhrase(player, REPORT_TYPE_LABELS[i]), i, NULL});
	}
}

static_function i64 GetCurrentReportType(KZPlayer *player, i64)
{
	return Clamp(player->optionService->GetPreferenceInt("jsReportType", JSReportType_Hud), (i64)JSReportType_Hud, (i64)JSReportType_Both);
}

static_function void PickReportType(KZPlayer *player, i64, i64 id)
{
	player->optionService->SetPreferenceInt("jsReportType", Clamp(id, (i64)JSReportType_Hud, (i64)JSReportType_Both));
}

static_global KZOptNode *fieldsNode {};

static_function void ResetFields(KZPlayer *player, i64)
{
	KZ::menu::ResetNode(player, fieldsNode);
}

void KZJumpstatsService::RegisterMenu()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Jumpstats");
	KZOptNode *general = KZ::menu::AddSub(cat, "Menu - General");
	KZ::menu::AddToggle(general, "Menu - JS Reporting", "jsReporting", true);
	KZ::menu::AddChoice(general, "Menu - JS Report Type", GetReportTypeChoices, GetCurrentReportType, PickReportType);
	KZ::menu::SetItemPref(general, "jsReportType", KZOptStorage::Int, JSReportType_Hud);
	KZ::menu::SetItemSubtext(general, "Menu - JS Report Type Sub");
	KZ::menu::AddToggle(general, "Menu - JS Always", "jsAlways", false);
	KZ::menu::SetItemSubtext(general, "Menu - JS Always Sub");
	KZ::menu::AddToggle(general, "Menu - JS Extended Stats", "jsExtendedChatStats", false);
	KZ::menu::SetItemSubtext(general, "Menu - JS Extended Stats Sub");
	KZ::menu::AddToggle(general, "Menu - JS Failstats", "jsFailstats", true);
	KZ::menu::AddToggle(general, "Menu - JS Failstats Console", "jsFailstatsConsole", true);
	KZ::menu::AddSize(general, "Menu - JS Volume", "jsVolume", 75, 0, 200);
	KZ::menu::SetItemUnit(general, "%");
	KZ::menu::SetItemScale(general, 100);
	KZ::menu::SetItemSubtext(general, "Menu - JS Volume Sub");
	KZ::menu::SetItemDivider(general);

	for (i32 i = 0; i < KZ_ARRAYSIZE(TIER_PREFS); i++)
	{
		KZ::menu::AddChoice(general, TIER_PREFS[i].phraseKey, GetTierChoices, GetCurrentTier, PickTier, i);
		KZ::menu::SetItemPref(general, TIER_PREFS[i].prefKey, KZOptStorage::Int,
							  (i32)KZOptionService::GetOptionInt(TIER_PREFS[i].serverDefaultKey, TIER_PREFS[i].fallback));
	}

	// Shared by the chat report and the HUD panel.
	fieldsNode = KZ::menu::AddSub(cat, "Menu - JS Fields");
	for (const JSFieldDef &field : JS_FIELDS)
	{
		KZ::menu::AddToggle(fieldsNode, field.menuPhrase, field.prefKey, true);
		if (field.extended)
		{
			KZ::menu::SetItemSubtext(fieldsNode, "Menu - JS Field Extended Sub");
		}
	}
	KZ::menu::SetItemDivider(fieldsNode);
	KZ::menu::AddButton(fieldsNode, "Menu - Reset", ResetFields);
}
