// The HUD's categories in the options menu: one subcategory per movement-HUD element, plus General.
#include "kz/hud/layout/layout.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/model.h"
#include "kz/option/menu/kz_menu.h"

#include "tier0/memdbgon.h"

static_global constexpr const char *ELEMENT_PHRASE[(i32)MHUDElement::Count] = {
	"Menu - Timer", "Speed", "Menu - Prespeed", "Menu - Keys", "Menu - Checkpoint", "Menu - Jumpstats", "Menu - Course Info", "Menu - W Release",
	// The indicators share one flattened page, so these only name them; they are not sub titles.
	"Menu - Ind Perf", "Menu - Ind CJ", "Menu - Ind JB"};

// Kept from registration so the reset buttons hand the nodes straight back to the model.
static_global KZOptNode *generalNode {};
static_global KZOptNode *gameHudNode {};
static_global KZOptNode *elementNodes[(i32)MHUDElement::Count] {};

// --- General page callbacks ---------------------------------------------------------

static_function void GetStyleChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	out.push_back({KZMenuService::GetPhrase(player, "Menu - Style Legacy"), 0, NULL});
	if (KZHUDService::IsLayoutHudAvailable())
	{
		out.push_back({KZMenuService::GetPhrase(player, "Menu - Style Layout"), 1, NULL});
	}
}

static_function i64 GetCurrentStyle(KZPlayer *player, i64)
{
	return player->hudService->IsUsingLayoutStyle() ? 1 : 0;
}

static_function void PickStyle(KZPlayer *player, i64, i64 id)
{
	if ((id == 1) != player->hudService->IsUsingLayoutStyle())
	{
		player->hudService->ToggleStyle();
	}
}

static_function i64 GetPanelState(KZPlayer *player, i64)
{
	return player->hudService->IsShowingPanel() ? 1 : 0;
}

static_function void TogglePanelState(KZPlayer *player, i64)
{
	player->hudService->TogglePanel();
}

struct ScreenAspectDef
{
	const char *label;
	i32 width; // layout units, 1080 * aspect
};

static_global const ScreenAspectDef SCREEN_ASPECTS[] = {
	{"5:4", 1350}, {"4:3", 1440}, {"3:2", 1620}, {"16:10", 1728}, {"16:9", 1920}, {"21:9", 2560}, {"32:9", 3840},
};

static_function void GetScreenAspectChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (const ScreenAspectDef &aspect : SCREEN_ASPECTS)
	{
		out.push_back({aspect.label, aspect.width, NULL});
	}
}

static_function i64 GetCurrentScreenAspect(KZPlayer *player, i64)
{
	return player->hudService->GetOwnPrefs().screenWidth;
}

static_function void PickScreenAspect(KZPlayer *player, i64, i64 id)
{
	player->optionService->SetPreferenceInt("mhudScreenWidth", id);
}

static_function void MoveElements(KZPlayer *player, i64)
{
	player->menuService->Close();
	player->hudService->StartHudEdit();
}

// tag is the element index, as on every other row of an element's page.
static_function void MoveElement(KZPlayer *player, i64 tag)
{
	player->menuService->Suspend();
	player->hudService->StartHudEdit((MHUDElement)tag);
}

static_function void ResetAll(KZPlayer *player, i64)
{
	KZ::menu::ResetNode(player, generalNode);
	KZ::menu::ResetNode(player, gameHudNode);
	for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
	{
		KZ::menu::ResetNode(player, elementNodes[i]);
	}
}

static_function void ResetElement(KZPlayer *player, i64 tag)
{
	KZ::menu::ResetNode(player, elementNodes[tag]);
}

static_function void ResetGameHud(KZPlayer *player, i64)
{
	KZ::menu::ResetNode(player, gameHudNode);
}

// Indexed by MHUDKeysIdle, so a picked row id is the stored value.
static_global const char *const KEYS_IDLE_LABELS[] = {"Menu - Keys Unpressed Show", "Menu - Keys Unpressed Hide", "Menu - Keys Unpressed Underscore"};

static_function void GetKeysIdleChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < KZ_ARRAYSIZE(KEYS_IDLE_LABELS); i++)
	{
		out.push_back({KZMenuService::GetPhrase(player, KEYS_IDLE_LABELS[i]), i, NULL});
	}
}

static_function i64 GetCurrentKeysIdle(KZPlayer *player, i64)
{
	return (i64)player->hudService->GetOwnPrefs().keysIdle;
}

static_function void PickKeysIdle(KZPlayer *player, i64, i64 id)
{
	player->optionService->SetPreferenceInt("mhudKeysIdle", Clamp(id, (i64)MHUDKeysIdle::Show, (i64)MHUDKeysIdle::Underscore));
}

// Indexed by MHUDPrespeedShow, so a picked row id is the stored value.
static_global const char *const PRESPEED_SHOW_LABELS[] = {"Menu - Prespeed Show Brief", "Menu - Prespeed Show Jump Only",
														  "Menu - Prespeed Show Always"};

static_function void GetPrespeedShowChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < KZ_ARRAYSIZE(PRESPEED_SHOW_LABELS); i++)
	{
		out.push_back({KZMenuService::GetPhrase(player, PRESPEED_SHOW_LABELS[i]), i, NULL});
	}
}

static_function i64 GetCurrentPrespeedShow(KZPlayer *player, i64)
{
	return (i64)player->hudService->GetOwnPrefs().prespeedShow;
}

static_function void PickPrespeedShow(KZPlayer *player, i64, i64 id)
{
	player->optionService->SetPreferenceInt("mhudPrespeedShow", Clamp(id, (i64)MHUDPrespeedShow::Brief, (i64)MHUDPrespeedShow::Always));
}

static_function void GetBorderChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < (i32)MHUDBorder::Count; i++)
	{
		const char *label = MHUD_BORDERS[i].label;
		out.push_back({label ? std::string(label) : KZMenuService::GetPhrase(player, "Menu - Border None"), i, NULL});
	}
}

// tag is the element index, the same one every other row on the page carries.
static_function i64 GetCurrentBorder(KZPlayer *player, i64 tag)
{
	return (i64)player->hudService->GetOwnPrefs().elements[tag].border;
}

static_function void PickBorder(KZPlayer *player, i64 tag, i64 id)
{
	player->optionService->SetPreferenceInt(MHUD_ELEMENTS[tag].borderKey, Clamp(id, (i64)MHUDBorder::None, (i64)MHUDBorder::Count - 1));
}

// Indexed by MHUDAlign, so a picked row id is the stored value.
static_global const char *const ALIGN_LABELS[] = {"Menu - Align Left", "Menu - Align Center", "Menu - Align Right"};

static_function void GetAlignChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	for (i32 i = 0; i < KZ_ARRAYSIZE(ALIGN_LABELS); i++)
	{
		out.push_back({KZMenuService::GetPhrase(player, ALIGN_LABELS[i]), i, NULL});
	}
}

static_function i64 GetCurrentAlign(KZPlayer *player, i64 tag)
{
	return (i64)player->hudService->GetOwnPrefs().elements[tag].align;
}

static_function void PickAlign(KZPlayer *player, i64 tag, i64 id)
{
	player->optionService->SetPreferenceInt(MHUD_ELEMENTS[tag].alignKey, Clamp(id, (i64)MHUDAlign::Left, (i64)MHUDAlign::Right));
}

// All three indicators share one page, so every row carries the indicator's own label.
static_function void RegisterIndicators(KZOptNode *hud)
{
	KZOptNode *sub = KZ::menu::AddSub(hud, "Menu - Indicators");
	// Only the first index points at the shared node, so ResetAll resets the page once, not three times.
	elementNodes[(i32)MHUDElement::Perf] = sub;

	for (i32 i = 0; i < MHUD_INDICATOR_COUNT; i++)
	{
		const MHUDIndicatorDef &indicator = MHUD_INDICATOR_DEFS[i];
		const i32 e = (i32)indicator.element;
		const MHUDElementDef &def = MHUD_ELEMENTS[e];
		const char *const *row = indicator.rowPhrase;

		KZ::menu::AddToggle(sub, row[(i32)MHUDIndicatorRow::Enabled], def.enabledKey, false);
		KZ::menu::AddPosition(sub, row[(i32)MHUDIndicatorRow::Position], def.xKey, def.yKey, def.xDefault, def.yDefault, e);
		KZ::menu::SetItemInteract(sub, MoveElement);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddChoice(sub, row[(i32)MHUDIndicatorRow::Align], GetAlignChoices, GetCurrentAlign, PickAlign, e);
		KZ::menu::SetItemPref(sub, def.alignKey, KZOptStorage::Int, (i32)MHUDAlign::Center);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddSize(sub, row[(i32)MHUDIndicatorRow::Size], def.sizeKey, def.sizeDefault, MHUD_SIZE_MIN, MHUD_SIZE_MAX, e);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddFont(sub, row[(i32)MHUDIndicatorRow::Font], def.fontKey, MHUD_DEFAULT_FONT, e);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddToggle(sub, row[(i32)MHUDIndicatorRow::Outline], def.outlineKey, true);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddSize(sub, row[(i32)MHUDIndicatorRow::Opacity], def.opacityKey, 100, 0, 100, e);
		KZ::menu::SetItemUnit(sub, "%");
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);

		i32 count = 0;
		const MHUDColorPrefDef *colors = KZHUDService::GetMHUDElementColorPrefs(indicator.element, count);
		for (i32 c = 0; c < count; c++)
		{
			KZ::menu::AddColor(sub, row[(i32)MHUDIndicatorRow::Color], colors[c].prefKey, Color(colors[c].r, colors[c].g, colors[c].b, 255), e);
			KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		}

		KZ::menu::AddToggle(sub, row[(i32)MHUDIndicatorRow::Acronym], indicator.acronymKey, false);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);

		KZ::menu::SetItemDivider(sub); // rule between this indicator's block and the next
	}

	KZ::menu::AddButton(sub, "Menu - Reset", ResetElement, (i64)MHUDElement::Perf);
}

// --- Registration -------------------------------------------------------------------

void KZHUDService::RegisterMenu()
{
	KZOptNode *hud = KZ::menu::AddCategory("Menu - HUD");

	KZOptNode *general = KZ::menu::AddSub(hud, "Menu - General");
	generalNode = general;
	KZ::menu::AddChoice(general, "Menu - Style", GetStyleChoices, GetCurrentStyle, PickStyle);
	// Both run through callbacks, so naming the preference is what makes them transferable.
	KZ::menu::SetItemPref(general, "hudLegacyStyle", KZOptStorage::Bool);
	KZ::menu::AddActionToggle(general, "Menu - Panel", GetPanelState, TogglePanelState);
	KZ::menu::SetItemPref(general, "showPanel", KZOptStorage::Bool, 1);
	KZ::menu::SetItemSubtext(general, "Menu - Panel Sub");
	KZ::menu::AddToggle(general, "Menu - Compact", "compactPanel", false);
	KZ::menu::SetItemEnabledBy(general, "showPanel");
	KZ::menu::SetItemSubtext(general, "Menu - Compact Sub");
	KZ::menu::AddToggle(general, "Menu - Mimic Spec", "mhudMimicSpec", false);
	KZ::menu::SetItemSubtext(general, "Menu - Mimic Spec Sub");
	KZ::menu::AddButton(general, "Menu - Move Elements", MoveElements);
	KZ::menu::SetItemSubtext(general, "Menu - Move Elements Sub");
	KZ::menu::AddChoice(general, "Menu - Screen Aspect", GetScreenAspectChoices, GetCurrentScreenAspect, PickScreenAspect);
	KZ::menu::SetItemPref(general, "mhudScreenWidth", KZOptStorage::Int, MHUD_DEF_SCREEN_WIDTH);
	KZ::menu::SetItemSubtext(general, "Menu - Screen Aspect Sub");
	KZ::menu::SetItemDivider(general);
	KZ::menu::AddButton(general, "Menu - Reset All", ResetAll);

	gameHudNode = KZ::menu::AddSub(hud, "Menu - Game HUD");
	for (const GameHudPartDef &part : GAME_HUD_PARTS)
	{
		KZ::menu::AddToggle(gameHudNode, part.phraseKey, part.prefKey, false);
		if (part.bit == KZ_HIDEHUD_ALL)
		{
			KZ::menu::SetItemSubtext(gameHudNode, "Menu - Game HUD All Sub");
		}
	}
	KZ::menu::SetItemDivider(gameHudNode);
	KZ::menu::AddButton(gameHudNode, "Menu - Reset", ResetGameHud);

	for (i32 e = 0; e < (i32)MHUDElement::Count; e++)
	{
		if (IsMHUDIndicator((MHUDElement)e))
		{
			continue; // flattened onto the one Indicators page below
		}
		const MHUDElementDef &def = MHUD_ELEMENTS[e];
		KZOptNode *sub = KZ::menu::AddSub(hud, ELEMENT_PHRASE[e]);
		elementNodes[e] = sub;

		// Everything below the Enabled toggle only affects a visible element, so it greys out with it. The jumpstats panel
		// has no toggle of its own, since the report type under Jumpstats decides whether it shows.
		if (def.enabledKey)
		{
			KZ::menu::AddToggle(sub, "Menu - Enabled", def.enabledKey, IsMHUDElementOnByDefault((MHUDElement)e));
		}
		KZ::menu::AddPosition(sub, "Menu - Position", def.xKey, def.yKey, def.xDefault, def.yDefault, e);
		KZ::menu::SetItemInteract(sub, MoveElement);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		if (def.alignKey)
		{
			KZ::menu::AddChoice(sub, "Menu - Align", GetAlignChoices, GetCurrentAlign, PickAlign, e);
			KZ::menu::SetItemPref(sub, def.alignKey, KZOptStorage::Int, (i32)GetMHUDDefaultAlign((MHUDElement)e));
			KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		}
		KZ::menu::AddSize(sub, "Menu - Size", def.sizeKey, def.sizeDefault, def.sizeMin, def.sizeMax, e);
		if (e == (i32)MHUDElement::Jumpstats || e == (i32)MHUDElement::Course)
		{
			KZ::menu::SetItemUnit(sub, "%");
		}
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddFont(sub, "Menu - Font", def.fontKey, MHUD_DEFAULT_FONT, e);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		if (def.outlineKey)
		{
			KZ::menu::AddToggle(sub, "Menu - Outline", def.outlineKey, true);
			KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		}
		const char *bgOpacityKey = GetMHUDBackgroundOpacityKey((MHUDElement)e);
		KZ::menu::AddSize(sub, bgOpacityKey ? "Menu - Text Opacity" : "Menu - Opacity", def.opacityKey, 100, 0, 100, e);
		KZ::menu::SetItemUnit(sub, "%");
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		if (bgOpacityKey)
		{
			KZ::menu::AddSize(sub, "Menu - Background Opacity", bgOpacityKey, MHUD_DEF_BG_OPACITY, 0, 100, e);
			KZ::menu::SetItemUnit(sub, "%");
			KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		}

		switch ((MHUDElement)e)
		{
			case MHUDElement::Timer:
			{
				KZ::menu::AddToggle(sub, "Menu - Timer Detail", "mhudTimerDetailed", true);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Timer State", "mhudTimerShowState", true);
				KZ::menu::SetItemSubtext(sub, "Menu - Affect Legacy Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				break;
			}

			case MHUDElement::Speed:
			{
				KZ::menu::AddToggle(sub, "Menu - Decimal", "mhudSpeedPrecise", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddChoice(sub, "Menu - Border", GetBorderChoices, GetCurrentBorder, PickBorder, e);
				KZ::menu::SetItemPref(sub, def.borderKey, KZOptStorage::Int, (i32)MHUDBorder::None);
				KZ::menu::SetItemSubtext(sub, "Menu - Border Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				break;
			}

			case MHUDElement::Prespeed:
			{
				KZ::menu::AddToggle(sub, "Menu - Decimal", "mhudPrespeedPrecise", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddChoice(sub, "Menu - Border", GetBorderChoices, GetCurrentBorder, PickBorder, e);
				KZ::menu::SetItemPref(sub, def.borderKey, KZOptStorage::Int, (i32)MHUDBorder::None);
				KZ::menu::SetItemSubtext(sub, "Menu - Border Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddChoice(sub, "Menu - Prespeed Show", GetPrespeedShowChoices, GetCurrentPrespeedShow, PickPrespeedShow);
				KZ::menu::SetItemPref(sub, "mhudPrespeedShow", KZOptStorage::Int, (i32)MHUDPrespeedShow::Brief);
				KZ::menu::SetItemSubtext(sub, "Menu - Prespeed Show Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				break;
			}

			case MHUDElement::Keys:
			{
				KZ::menu::AddToggle(sub, "Menu - Keys Overlap", "mhudKeysOverlap", true);
				KZ::menu::SetItemSubtext(sub, "Menu - Keys Overlap Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Overlap Axis", "mhudKeysOverlapAxis", false);
				KZ::menu::SetItemSubtext(sub, "Menu - Keys Overlap Axis Sub");
				KZ::menu::SetItemEnabledBy(sub, "mhudKeysOverlap");
				KZ::menu::AddChoice(sub, "Menu - Keys Unpressed", GetKeysIdleChoices, GetCurrentKeysIdle, PickKeysIdle);
				KZ::menu::SetItemPref(sub, "mhudKeysIdle", KZOptStorage::Int, (i32)MHUDKeysIdle::Show);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Letters", "mhudKeysLetters", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Jump Count", "mhudKeysJumpCount", false);
				KZ::menu::SetItemSubtext(sub, "Menu - Keys Jump Count Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Square", "mhudKeysSquare", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Border", "mhudKeysBorder", true);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Glow", "mhudKeysGlow", true);
				KZ::menu::SetItemSubtext(sub, "Menu - Keys Glow Fill Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Fill", "mhudKeysFill", true);
				KZ::menu::SetItemSubtext(sub, "Menu - Keys Glow Fill Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				break;
			}

			case MHUDElement::Jumpstats:
			{
				KZ::menu::AddToggle(sub, "Menu - JS History", "mhudJsShowHistory", true);
				KZ::menu::SetItemSubtext(sub, "Menu - JS History Sub");
				KZ::menu::AddToggle(sub, "Menu - JS History Below", "mhudJsHistoryBelow", false);
				KZ::menu::SetItemSubtext(sub, "Menu - JS History Below Sub");
				KZ::menu::SetItemEnabledBy(sub, "mhudJsShowHistory");
				KZ::menu::AddSize(sub, "Menu - JS Duration", "mhudJsDuration", MHUD_JS_PANEL_TIME, MHUD_JS_PANEL_TIME_MIN, MHUD_JS_PANEL_TIME_MAX);
				KZ::menu::SetItemPref(sub, "mhudJsDuration", KZOptStorage::Int);
				KZ::menu::SetItemUnit(sub, "s");
				KZ::menu::SetItemSubtext(sub, "Menu - JS Duration Sub");
				break;
			}

			case MHUDElement::Course:
			{
				KZ::menu::AddToggle(sub, "Progress - Show", "showProgress", true);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Progress - Show Label", "showProgressLabel", true);
				KZ::menu::SetItemEnabledBy(sub, "showProgress");
				KZ::menu::AddToggle(sub, "Menu - Course Show Map", "mhudCourseShowMap", true);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Course Show Progress", "mhudCourseShowProgress", true);
				KZ::menu::SetItemSubtext(sub, "Menu - Course Show Progress Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Course Show Records", "mhudCourseShowRecords", true);
				KZ::menu::SetItemSubtext(sub, "Menu - Course Show Records Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Course Show Pro", "mhudCourseShowPro", true);
				KZ::menu::SetItemSubtext(sub, "Menu - Course Show Pro Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Course Show Splits", "mhudCourseShowSplits", true);
				KZ::menu::SetItemSubtext(sub, "Menu - Course Show Splits Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				break;
			}

			default:
				break;
		}

		KZ::menu::SetItemDivider(sub); // rule between the layout controls and the colors

		i32 count = 0;
		const MHUDColorPrefDef *colors = KZHUDService::GetMHUDElementColorPrefs((MHUDElement)e, count);
		for (i32 i = 0; i < count; i++)
		{
			KZ::menu::AddColor(sub, colors[i].phraseKey, colors[i].prefKey, Color(colors[i].r, colors[i].g, colors[i].b, 255), e);
			if (colors[i].solidOnly)
			{
				KZ::menu::SetItemSolidOnly(sub);
			}
			if (colors[i].affectLegacy)
			{
				KZ::menu::SetItemSubtext(sub, "Menu - Affect Legacy Sub");
			}
			KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
			if (colors[i].enabledBy)
			{
				KZ::menu::SetItemEnabledBy(sub, colors[i].enabledBy);
			}
		}

		KZ::menu::AddButton(sub, "Menu - Reset", ResetElement, e);
	}

	RegisterIndicators(hud);
}
