// The HUD's categories in the options menu, registered with the option-menu model. One subcategory
// per movement-HUD element plus a General page.
#include "kz/hud/layout/layout.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/model.h"
#include "kz/option/menu/kz_menu.h"

#include "tier0/memdbgon.h"

static_global constexpr const char *ELEMENT_PHRASE[(i32)MHUDElement::Count] = {"Menu - Timer", "Speed", "Menu - Prespeed", "Menu - Keys",
																			   "Menu - Checkpoint"};

// Kept from registration so the reset buttons can hand the nodes straight back to the model, rather
// than repeating every default in a list that has to be updated whenever an option is added.
static_global KZOptNode *generalNode {};
static_global KZOptNode *elementNodes[(i32)MHUDElement::Count] {};

// While any popup for an element's position/size/font/color is open, force that element on screen so
// the player can see what they are editing.
static_function void HudEdit(KZPlayer *player, i64 tag, bool begin)
{
	player->hudService->SetMHUDForcedElement((MHUDElement)tag, begin);
}

// --- General page callbacks ---------------------------------------------------------

static_function void StyleChoices(KZPlayer *player, i64, std::vector<KZChoice> &out)
{
	out.push_back({KZMenuPhrase(player, "Menu - Style Legacy"), 0, NULL});
	if (KZHUDService::IsLayoutHudAvailable())
	{
		out.push_back({KZMenuPhrase(player, "Menu - Style Layout"), 1, NULL});
	}
}

static_function i64 StyleCurrent(KZPlayer *player, i64)
{
	return player->hudService->IsUsingLayoutStyle() ? 1 : 0;
}

static_function void StylePick(KZPlayer *player, i64, i64 id)
{
	if ((id == 1) != player->hudService->IsUsingLayoutStyle())
	{
		player->hudService->ToggleStyle();
	}
}

static_function i64 PanelCurrent(KZPlayer *player, i64)
{
	return player->hudService->IsShowingPanel() ? 1 : 0;
}

static_function void PanelToggle(KZPlayer *player, i64)
{
	player->hudService->TogglePanel();
}

// The crosshair mirrors the player's own cl_crosshair* settings, which the server can only read by
// asking for them: re-ask whenever the option is switched on, so changes made since the player
// connected show up.
static_function i64 CrosshairCurrent(KZPlayer *player, i64)
{
	return player->optionService->GetPreferenceBool("mhudCrosshair", false) ? 1 : 0;
}

static_function void CrosshairToggle(KZPlayer *player, i64)
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
	KZMenu::ResetNode(player, generalNode);
	for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
	{
		KZMenu::ResetNode(player, elementNodes[i]);
	}
}

static_function void ResetElement(KZPlayer *player, i64 tag)
{
	KZMenu::ResetNode(player, elementNodes[tag]);
}

// --- Registration -------------------------------------------------------------------

void MHUDRegisterMenu()
{
	KZOptNode *hud = KZMenu::AddCategory("Menu - HUD");

	KZOptNode *general = KZMenu::AddSub(hud, "Menu - General");
	generalNode = general;
	KZMenu::AddChoice(general, "Menu - Style", StyleChoices, StyleCurrent, StylePick);
	KZMenu::AddActionToggle(general, "Menu - Panel", PanelCurrent, PanelToggle);
	KZMenu::AddActionToggle(general, "Menu - Crosshair", CrosshairCurrent, CrosshairToggle);
	KZMenu::AddToggle(general, "Menu - Compact", "compactPanel", false);
	KZMenu::SetItemSubtext(general, "Menu - Compact Sub");
	KZMenu::SetItemDivider(general);
	KZMenu::AddButton(general, "Menu - Reset All", ResetAll);

	for (i32 e = 0; e < (i32)MHUDElement::Count; e++)
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[e];
		KZOptNode *sub = KZMenu::AddSub(hud, ELEMENT_PHRASE[e]);
		elementNodes[e] = sub;

		KZMenu::AddToggle(sub, "Menu - Enabled", def.enabledKey, true);
		KZMenu::AddPosition(sub, "Menu - Position", def.xKey, def.yKey, def.xDefault, def.yDefault, e, HudEdit);
		KZMenu::AddSize(sub, "Menu - Size", def.sizeKey, def.sizeDefault, MHUD_SIZE_MIN, MHUD_SIZE_MAX, e, HudEdit);
		KZMenu::AddFont(sub, "Menu - Font", def.fontKey, MHUD_DEFAULT_FONT, e, HudEdit);
		KZMenu::AddToggle(sub, "Menu - Outline", def.outlineKey, true);

		if (e == (i32)MHUDElement::Timer)
		{
			KZMenu::AddToggle(sub, "Menu - Timer Detail", "mhudTimerDetailed", true);
		}
		else if (e == (i32)MHUDElement::Keys)
		{
			KZMenu::AddToggle(sub, "Menu - Keys Overlap", "mhudKeysOverlap", true);
			KZMenu::AddToggle(sub, "Menu - Keys Unpressed", "mhudKeysHideUnpressed", false);
			KZMenu::AddToggle(sub, "Menu - Keys Letters", "mhudKeysLetters", false);
			KZMenu::AddToggle(sub, "Menu - Keys Square", "mhudKeysSquare", false);
		}

		KZMenu::SetItemDivider(sub); // rule between the layout controls and the colors

		i32 count = 0;
		const MHUDColorPrefDef *colors = MHUDElementColorPrefs((MHUDElement)e, count);
		for (i32 i = 0; i < count; i++)
		{
			KZMenu::AddColor(sub, colors[i].phraseKey, colors[i].prefKey, Color(colors[i].r, colors[i].g, colors[i].b, 255), e, HudEdit);
		}

		KZMenu::AddButton(sub, "Menu - Reset", ResetElement, e);
	}
}
