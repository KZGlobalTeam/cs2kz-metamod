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

// Force an element on screen while any popup for it is open.
static_function void OnHudEdit(KZPlayer *player, i64 tag, bool begin)
{
	player->hudService->SetMHUDForcedElement((MHUDElement)tag, begin);
}

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
	KZ::menu::AddActionToggle(general, "Menu - Crosshair", GetCrosshairState, ToggleCrosshairState);
	KZ::menu::AddToggle(general, "Menu - Compact", "compactPanel", false);
	KZ::menu::SetItemSubtext(general, "Menu - Compact Sub");
	KZ::menu::SetItemDivider(general);
	KZ::menu::AddButton(general, "Menu - Reset All", ResetAll);

	for (i32 e = 0; e < (i32)MHUDElement::Count; e++)
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[e];
		KZOptNode *sub = KZ::menu::AddSub(hud, ELEMENT_PHRASE[e]);
		elementNodes[e] = sub;

		KZ::menu::AddToggle(sub, "Menu - Enabled", def.enabledKey, true);
		KZ::menu::AddPosition(sub, "Menu - Position", def.xKey, def.yKey, def.xDefault, def.yDefault, e, OnHudEdit);
		KZ::menu::AddSize(sub, "Menu - Size", def.sizeKey, def.sizeDefault, MHUD_SIZE_MIN, MHUD_SIZE_MAX, e, OnHudEdit);
		KZ::menu::AddFont(sub, "Menu - Font", def.fontKey, MHUD_DEFAULT_FONT, e, OnHudEdit);
		KZ::menu::AddToggle(sub, "Menu - Outline", def.outlineKey, true);

		if (e == (i32)MHUDElement::Timer)
		{
			KZ::menu::AddToggle(sub, "Menu - Timer Detail", "mhudTimerDetailed", true);
		}
		else if (e == (i32)MHUDElement::Keys)
		{
			KZ::menu::AddToggle(sub, "Menu - Keys Overlap", "mhudKeysOverlap", true);
			KZ::menu::AddToggle(sub, "Menu - Keys Unpressed", "mhudKeysHideUnpressed", false);
			KZ::menu::AddToggle(sub, "Menu - Keys Letters", "mhudKeysLetters", false);
			KZ::menu::AddToggle(sub, "Menu - Keys Square", "mhudKeysSquare", false);
		}

		KZ::menu::SetItemDivider(sub); // rule between the layout controls and the colors

		i32 count = 0;
		const MHUDColorPrefDef *colors = KZHUDService::GetMHUDElementColorPrefs((MHUDElement)e, count);
		for (i32 i = 0; i < count; i++)
		{
			KZ::menu::AddColor(sub, colors[i].phraseKey, colors[i].prefKey, Color(colors[i].r, colors[i].g, colors[i].b, 255), e, OnHudEdit);
		}

		KZ::menu::AddButton(sub, "Menu - Reset", ResetElement, e);
	}
}
