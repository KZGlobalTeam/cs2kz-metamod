// The HUD's categories in the options menu: one subcategory per movement-HUD element, plus General.
#include "kz/hud/layout/layout.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/model.h"
#include "kz/option/menu/kz_menu.h"

#include "tier0/memdbgon.h"

static_global constexpr const char *ELEMENT_PHRASE[(i32)MHUDElement::Count] = {"Menu - Timer", "Speed", "Menu - Prespeed", "Menu - Keys",
																			   "Menu - Checkpoint"};

// Kept from registration so the reset buttons hand the nodes straight back to the model.
static_global KZOptNode *generalNode {};
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

// Re-ask for the client's cl_crosshair* values whenever the option is switched on.
static_function i64 GetCrosshairState(KZPlayer *player, i64)
{
	return player->optionService->GetPreferenceBool("mhudCrosshair", false) ? 1 : 0;
}

static_function void ToggleCrosshairState(KZPlayer *player, i64)
{
	const bool enabled = !player->optionService->GetPreferenceBool("mhudCrosshair", false);
	player->optionService->SetPreferenceBool("mhudCrosshair", enabled);
	if (enabled)
	{
		player->hudService->QueryCrosshairCvars();
	}
}

static_function void ResetAll(KZPlayer *player, i64)
{
	KZ::menu::ResetNode(player, generalNode);
	for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
	{
		KZ::menu::ResetNode(player, elementNodes[i]);
	}
}

static_function void ResetElement(KZPlayer *player, i64 tag)
{
	KZ::menu::ResetNode(player, elementNodes[tag]);
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
	return (i64)player->hudService->GetPrefs().keysIdle;
}

static_function void PickKeysIdle(KZPlayer *player, i64, i64 id)
{
	player->optionService->SetPreferenceInt("mhudKeysIdle", Clamp(id, (i64)MHUDKeysIdle::Show, (i64)MHUDKeysIdle::Underscore));
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
	KZ::menu::SetItemPref(general, "showPanel", KZOptStorage::Bool);
	KZ::menu::SetItemSubtext(general, "Menu - Affect Legacy Sub");
	KZ::menu::AddActionToggle(general, "Menu - Crosshair", GetCrosshairState, ToggleCrosshairState);
	KZ::menu::AddToggle(general, "Menu - Compact", "compactPanel", false);
	KZ::menu::SetItemEnabledBy(general, "showPanel");
	KZ::menu::SetItemSubtext(general, "Menu - Compact Sub");
	KZ::menu::SetItemDivider(general);
	KZ::menu::AddButton(general, "Menu - Reset All", ResetAll);

	for (i32 e = 0; e < (i32)MHUDElement::Count; e++)
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[e];
		KZOptNode *sub = KZ::menu::AddSub(hud, ELEMENT_PHRASE[e]);
		elementNodes[e] = sub;

		// Everything below the Enabled toggle only affects a visible element, so it greys out with it.
		KZ::menu::AddToggle(sub, "Menu - Enabled", def.enabledKey, true);
		KZ::menu::AddPosition(sub, "Menu - Position", def.xKey, def.yKey, def.xDefault, def.yDefault, e);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddSize(sub, "Menu - Size", def.sizeKey, def.sizeDefault, MHUD_SIZE_MIN, MHUD_SIZE_MAX, e);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddFont(sub, "Menu - Font", def.fontKey, MHUD_DEFAULT_FONT, e);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
		KZ::menu::AddToggle(sub, "Menu - Outline", def.outlineKey, true);
		KZ::menu::SetItemEnabledBy(sub, def.enabledKey);

		switch ((MHUDElement)e)
		{
			case MHUDElement::Timer:
			{
				KZ::menu::AddToggle(sub, "Menu - Timer Detail", "mhudTimerDetailed", true);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				break;
			}

			case MHUDElement::Speed:
			{
				KZ::menu::AddToggle(sub, "Menu - Decimal", "mhudSpeedPrecise", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				break;
			}

			case MHUDElement::Prespeed:
			{
				KZ::menu::AddToggle(sub, "Menu - Decimal", "mhudPrespeedPrecise", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Prespeed Brackets", "mhudPrespeedBrackets", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Prespeed Hide Walk Off", "mhudPrespeedHideWalkOff", false);
				KZ::menu::SetItemSubtext(sub, "Menu - Prespeed Hide Walk Off Sub");
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				break;
			}

			case MHUDElement::Keys:
			{
				KZ::menu::AddToggle(sub, "Menu - Keys Overlap", "mhudKeysOverlap", true);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddChoice(sub, "Menu - Keys Unpressed", GetKeysIdleChoices, GetCurrentKeysIdle, PickKeysIdle);
				KZ::menu::SetItemPref(sub, "mhudKeysIdle", KZOptStorage::Int);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Letters", "mhudKeysLetters", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Square", "mhudKeysSquare", false);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Border", "mhudKeysBorder", true);
				KZ::menu::SetItemEnabledBy(sub, def.enabledKey);
				KZ::menu::AddToggle(sub, "Menu - Keys Glow", "mhudKeysGlow", true);
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
}
