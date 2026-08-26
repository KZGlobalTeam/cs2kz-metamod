// The HUD's pages in the options menu.
#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/kz_menu.h"
#include "kz/option/menu/pickers.h"
#include "kz/option/menu/tables.h"
#include "kz/language/kz_language.h"

#include "tier0/memdbgon.h"

// Element order matches MHUDElement. "Speed" is an existing phrase with 15 translations already.
static_global constexpr const char *ELEMENT_PHRASE[(i32)MHUDElement::Count] = {"Menu - Timer", "Speed", "Menu - Prespeed", "Menu - Keys",
																			   "Menu - Checkpoint"};

enum
{
	HUD_ACT_STYLE,
	HUD_ACT_PANEL,
	HUD_ACT_COMPACT,
	HUD_ACT_RESET_ALL,
};

enum
{
	ELEM_ACT_ENABLED,
	ELEM_ACT_OUTLINE,
	ELEM_ACT_DETAIL,
	ELEM_ACT_OVERLAP,
	ELEM_ACT_UNPRESSED,
	ELEM_ACT_RESET,
};

static_function const MHUDColorPrefDef *ColorPref(KZMenuContext context)
{
	i32 count = 0;
	const MHUDColorPrefDef *prefs = MHUDElementColorPrefs((MHUDElement)context.index, count);
	return context.sub < count ? &prefs[context.sub] : NULL;
}

// === Leaf pages =====================================================================

static_global class ElementFontPage : public KZMenuFontPage
{
	virtual const char *GetCurrentSlug(KZPlayer *player, KZMenuContext context) override
	{
		return PanoramaResolveFontSlug(player->optionService->GetPreferenceStr(MHUD_ELEMENTS[context.index].fontKey, MHUD_DEFAULT_FONT),
									   MHUD_DEFAULT_FONT);
	}

	virtual void OnFontPicked(KZPlayer *player, KZMenuContext context, const char *slug) override
	{
		player->optionService->SetPreferenceStr(MHUD_ELEMENTS[context.index].fontKey, slug);
	}
} g_fontPage;

static_global class ElementColorPage : public KZMenuColorPage
{
	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override
	{
		const MHUDColorPrefDef *pref = ColorPref(context);
		return pref ? KZMenuPhrase(player, pref->phraseKey) : KZMenuPhrase(player, "Menu - Color");
	}

	virtual Color GetCurrentColor(KZPlayer *player, KZMenuContext context) override
	{
		const MHUDColorPrefDef *pref = ColorPref(context);
		return pref ? player->optionService->GetPreferenceColor(pref->prefKey, Color(pref->r, pref->g, pref->b, 255)) : MHUD_DEF_BASE_COLOR;
	}

	virtual void OnColorPicked(KZPlayer *player, KZMenuContext context, const Color &color) override
	{
		if (const MHUDColorPrefDef *pref = ColorPref(context))
		{
			player->optionService->SetPreferenceColor(pref->prefKey, color);
		}
	}
} g_colorPage;

// Both steppers force their element on screen so the player can see what they are moving.
class StepperPage : public KZMenuPage
{
public:
	virtual bool IsStepper() const override
	{
		return true;
	}

	virtual void OnEnter(KZPlayer *player, KZMenuContext context) override
	{
		player->hudService->SetMHUDForcedElement((MHUDElement)context.index, true);
	}

	virtual void OnLeave(KZPlayer *player, KZMenuContext context) override
	{
		player->hudService->SetMHUDForcedElement((MHUDElement)context.index, false);
	}
};

static_global class PositionPage : public StepperPage
{
	virtual bool HasVerticalStep() const override
	{
		return true;
	}

	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override
	{
		return KZMenuPhrase(player, ELEMENT_PHRASE[context.index]) + " - " + KZMenuPhrase(player, "Menu - Position");
	}

	virtual std::string GetStepReadout(KZPlayer *player, KZMenuContext context) override
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[context.index];
		char value[32];
		V_snprintf(value, sizeof(value), "%i%%, %i%%", (i32)player->optionService->GetPreferenceFloat(def.xKey, def.xDefault),
				   (i32)player->optionService->GetPreferenceFloat(def.yKey, def.yDefault));
		return value;
	}

	virtual void OnStep(KZPlayer *player, KZMenuContext context, bool vertical, i32 delta) override
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[context.index];
		const char *key = vertical ? def.yKey : def.xKey;
		i32 value = (i32)player->optionService->GetPreferenceFloat(key, vertical ? def.yDefault : def.xDefault) + delta;
		player->optionService->SetPreferenceFloat(key, PanoramaSnapToStep(value, -100, 100));
	}
} g_positionPage;

static_global class SizePage : public StepperPage
{
	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override
	{
		return KZMenuPhrase(player, ELEMENT_PHRASE[context.index]) + " - " + KZMenuPhrase(player, "Menu - Size");
	}

	virtual std::string GetStepReadout(KZPlayer *player, KZMenuContext context) override
	{
		char value[32];
		V_snprintf(value, sizeof(value), "%ipx",
				   (i32)player->optionService->GetPreferenceFloat(MHUD_ELEMENTS[context.index].sizeKey, MHUD_ELEMENTS[context.index].sizeDefault));
		return value;
	}

	virtual void OnStep(KZPlayer *player, KZMenuContext context, bool vertical, i32 delta) override
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[context.index];
		i32 size = (i32)player->optionService->GetPreferenceFloat(def.sizeKey, def.sizeDefault) + delta;
		player->optionService->SetPreferenceFloat(def.sizeKey, PanoramaSnapToStep(size, MHUD_SIZE_MIN, MHUD_SIZE_MAX));
	}
} g_sizePage;

// === One element ====================================================================

static_global class ElementPage : public KZMenuPage
{
	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override
	{
		return KZMenuPhrase(player, ELEMENT_PHRASE[context.index]);
	}

	virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) override
	{
		const MHUDElement element = (MHUDElement)context.index;
		const MHUDElementDef &def = MHUD_ELEMENTS[context.index];
		auto *opts = player->optionService;
		KZMenuRow row;

		row.label = KZMenuToggleLabel(player, "Menu - Enabled", opts->GetPreferenceBool(def.enabledKey, true));
		row.param = {ELEM_ACT_ENABLED};
		rows.push_back(row);

		char value[32];
		V_snprintf(value, sizeof(value), "%i%%, %i%%", (i32)opts->GetPreferenceFloat(def.xKey, def.xDefault),
				   (i32)opts->GetPreferenceFloat(def.yKey, def.yDefault));
		row.label = KZMenuValueLabel(player, "Menu - Position", value) + " >";
		row.submenu = &g_positionPage;
		row.param = context;
		rows.push_back(row);

		V_snprintf(value, sizeof(value), "%ipx", (i32)opts->GetPreferenceFloat(def.sizeKey, def.sizeDefault));
		row.label = KZMenuValueLabel(player, "Menu - Size", value) + " >";
		row.submenu = &g_sizePage;
		row.param = context;
		rows.push_back(row);

		row.label = KZMenuValueLabel(player, "Menu - Font",
									 PanoramaFontDisplayName(opts->GetPreferenceStr(def.fontKey, MHUD_DEFAULT_FONT), MHUD_DEFAULT_FONT))
					+ " >";
		row.submenu = &g_fontPage;
		row.param = context;
		rows.push_back(row);

		row.submenu = NULL;
		row.label = KZMenuToggleLabel(player, "Menu - Outline", opts->GetPreferenceBool(def.outlineKey, true));
		row.param = {ELEM_ACT_OUTLINE};
		rows.push_back(row);

		if (element == MHUDElement::Timer)
		{
			row.label = KZMenuToggleLabel(player, "Menu - Timer Detail", player->hudService->IsMHUDTimerDetailed());
			row.param = {ELEM_ACT_DETAIL};
			rows.push_back(row);
		}
		else if (element == MHUDElement::Keys)
		{
			row.label = KZMenuToggleLabel(player, "Menu - Keys Overlap", player->hudService->IsMHUDKeysOverlapEnabled());
			row.param = {ELEM_ACT_OVERLAP};
			rows.push_back(row);

			row.label = KZMenuToggleLabel(player, "Menu - Keys Unpressed", player->hudService->IsMHUDKeysHidingUnpressed());
			row.param = {ELEM_ACT_UNPRESSED};
			rows.push_back(row);
		}

		i32 count = 0;
		const MHUDColorPrefDef *colors = MHUDElementColorPrefs(element, count);
		for (i32 i = 0; i < count; i++)
		{
			row.label = KZMenuPhrase(player, colors[i].phraseKey) + " >";
			row.submenu = &g_colorPage;
			row.param = {(i32)element, i};
			row.colorClass =
				PanoramaNearestColorClass(opts->GetPreferenceColor(colors[i].prefKey, Color(colors[i].r, colors[i].g, colors[i].b, 255)));
			rows.push_back(row);
		}

		row.submenu = NULL;
		row.colorClass = NULL;
		row.label = KZMenuPhrase(player, "Menu - Reset");
		row.param = {ELEM_ACT_RESET};
		rows.push_back(row);
	}

	virtual void OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param) override
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[context.index];
		auto *opts = player->optionService;
		switch (param.index)
		{
			case ELEM_ACT_ENABLED:
				opts->SetPreferenceBool(def.enabledKey, !opts->GetPreferenceBool(def.enabledKey, true));
				break;
			case ELEM_ACT_OUTLINE:
				opts->SetPreferenceBool(def.outlineKey, !opts->GetPreferenceBool(def.outlineKey, true));
				break;
			case ELEM_ACT_DETAIL:
				opts->SetPreferenceBool("mhudTimerDetailed", !player->hudService->IsMHUDTimerDetailed());
				break;
			case ELEM_ACT_OVERLAP:
				opts->SetPreferenceBool("mhudKeysOverlap", !player->hudService->IsMHUDKeysOverlapEnabled());
				break;
			case ELEM_ACT_UNPRESSED:
				opts->SetPreferenceBool("mhudKeysHideUnpressed", !player->hudService->IsMHUDKeysHidingUnpressed());
				break;
			case ELEM_ACT_RESET:
				MHUDResetElementPrefs(player, (MHUDElement)context.index);
				break;
		}
	}
} g_elementPage;

// === The HUD category ===============================================================

static_global class HudPage : public KZMenuPage
{
	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override
	{
		return KZMenuPhrase(player, "Menu - HUD");
	}

	virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) override
	{
		KZMenuRow row;
		const bool layoutStyle = player->hudService->IsUsingLayoutStyle();

		// Both of these stay on screen when they cannot be used: a row that vanishes reads as a bug,
		// a greyed one says why it is there.
		row.label =
			KZMenuValueLabel(player, "Menu - Style", KZMenuPhrase(player, layoutStyle ? "Menu - Style Layout" : "Menu - Style Legacy").c_str());
		row.param = {HUD_ACT_STYLE};
		row.disabled = !KZHUDService::IsLayoutHudAvailable();
		rows.push_back(row);

		row.disabled = false;
		row.label = KZMenuToggleLabel(player, "Menu - Panel", player->hudService->IsShowingPanel());
		row.param = {HUD_ACT_PANEL};
		rows.push_back(row);

		// Compact only ever applied to the legacy channels.
		row.label = KZMenuToggleLabel(player, "Menu - Compact", player->hudService->IsCompactPanel());
		row.param = {HUD_ACT_COMPACT};
		row.disabled = layoutStyle;
		rows.push_back(row);
		row.disabled = false;

		for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
		{
			row.label = KZMenuPhrase(player, ELEMENT_PHRASE[i]) + " >";
			row.submenu = &g_elementPage;
			row.param = {i};
			rows.push_back(row);
		}

		row.submenu = NULL;
		row.label = KZMenuPhrase(player, "Menu - Reset All");
		row.param = {HUD_ACT_RESET_ALL};
		rows.push_back(row);
	}

	virtual void OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param) override
	{
		switch (param.index)
		{
			case HUD_ACT_STYLE:
				player->hudService->ToggleStyle();
				break;
			case HUD_ACT_PANEL:
				player->hudService->TogglePanel();
				break;
			case HUD_ACT_COMPACT:
				player->hudService->ToggleCompactPanel();
				break;
			case HUD_ACT_RESET_ALL:
				for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
				{
					MHUDResetElementPrefs(player, (MHUDElement)i);
				}
				break;
		}
	}
} g_hudPage;

void MHUDRegisterMenu()
{
	KZMenuService::RegisterCategory(&g_hudPage);
}
