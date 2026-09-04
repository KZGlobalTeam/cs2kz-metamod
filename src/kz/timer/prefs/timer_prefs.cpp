// The timer category in the options menu.
#include "kz/timer/kz_timer.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/model.h"
#include "kz/option/menu/kz_menu.h"

#include "tier0/memdbgon.h"

// Indexed by SafeguardOption and CompareType, so a picked row id is the stored value.
static_global const char *const SAFEGUARD_LABELS[] = {"Menu - Safeguard Off", "Menu - Safeguard Nub", "Menu - Safeguard Pro"};
static_global const char *const COMPARE_LABELS[] = {"Menu - Compare None", "Menu - Compare Local PB", "Menu - Compare Global PB",
													"Menu - Compare Server Record", "Menu - Compare World Record"};
// SetCompareTarget takes the same strings the kz_comparelevel command does.
static_global const char *const COMPARE_ARGS[] = {"none", "spb", "gpb", "sr", "wr"};

static_function void GetSafeguardChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < KZ_ARRAYSIZE(SAFEGUARD_LABELS); i++)
	{
		out.push_back({KZMenuService::GetPhrase(player, SAFEGUARD_LABELS[i]), i, NULL});
	}
}

static_function i64 GetCurrentSafeguard(KZPlayer *player, i64)
{
	return player->optionService->GetPreferenceInt("safeguard", SAFEGUARD_DISABLED);
}

static_function void PickSafeguard(KZPlayer *player, i64, i64 id)
{
	player->optionService->SetPreferenceInt("safeguard", id);
}

static_function void GetCompareChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < KZ_ARRAYSIZE(COMPARE_LABELS); i++)
	{
		out.push_back({KZMenuService::GetPhrase(player, COMPARE_LABELS[i]), i, NULL});
	}
}

static_function i64 GetCurrentCompare(KZPlayer *player, i64)
{
	return player->optionService->GetPreferenceInt("preferredCompareType", KZTimerService::COMPARE_GPB);
}

static_function void PickCompare(KZPlayer *player, i64, i64 id)
{
	if (id >= 0 && id < KZ_ARRAYSIZE(COMPARE_ARGS))
	{
		player->timerService->SetCompareTarget(COMPARE_ARGS[id]);
	}
}

static_function i64 GetStopSoundState(KZPlayer *player, i64)
{
	return player->optionService->GetPreferenceBool("timerStopSound", true) ? 1 : 0;
}

static_function void ToggleStopSound(KZPlayer *player, i64)
{
	player->timerService->ToggleTimerStopSound();
}

void KZTimerService::RegisterMenu()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Timer Category");
	KZ::menu::AddToggle(cat, "Menu - Map Overlay", "mapOverlay", false);
	KZ::menu::AddActionToggle(cat, "Menu - Timer Stop Sound", GetStopSoundState, ToggleStopSound);
	KZ::menu::SetItemPref(cat, "timerStopSound", KZOptStorage::Bool);
	KZ::menu::AddChoice(cat, "Menu - Safeguard", GetSafeguardChoices, GetCurrentSafeguard, PickSafeguard);
	KZ::menu::SetItemPref(cat, "safeguard", KZOptStorage::Int);
	KZ::menu::AddChoice(cat, "Menu - Compare", GetCompareChoices, GetCurrentCompare, PickCompare);
	KZ::menu::SetItemPref(cat, "preferredCompareType", KZOptStorage::Int);
	KZ::menu::AddSize(cat, "Menu - Record Volume", "recordVolume", 100, 0, 200);
	KZ::menu::SetItemUnit(cat, "%");
	KZ::menu::SetItemScale(cat, 100);
	KZ::menu::SetItemSubtext(cat, "Menu - Record Volume Sub");
}
