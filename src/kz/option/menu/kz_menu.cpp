#include "cs2kz.h"
#include "kz/option/menu/kz_menu.h"
#include "kz/option/menu/pickers.h"
#include "kz/option/menu/tables.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"

#include <vendor/mm-cs2menus/src/public/ics2menus.h>
extern ICS2Menus *g_pMenus;

#include "tier0/memdbgon.h"

// Its own entity, separate from the movement HUD's, so a server where nobody opens the menu never
// spawns it.
#define KZ_MENU_LAYOUT       "panorama/layout/custom_game/menu.xml"
#define KZ_MENU_DEFAULT_FONT "stratum2-medium-tf"

static_global CHandle<CBaseEntity> g_hMenuLayout;
static_global const Color KZ_MENU_DEFAULT_COLOR(255, 255, 255, 255);

// === Small helpers ==================================================================

// One at a time: these share nothing but each returns its own static buffer.
static_function const char *RowPanel(i32 i)
{
	static_persist char buf[16];
	V_snprintf(buf, sizeof(buf), "m_row%i", i);
	return buf;
}

static_function const char *TextPanel(i32 i)
{
	static_persist char buf[16];
	V_snprintf(buf, sizeof(buf), "m_txt%i", i);
	return buf;
}

static_function const char *RowVar(i32 i)
{
	static_persist char buf[16];
	V_snprintf(buf, sizeof(buf), "row%i", i);
	return buf;
}

static_function void SetClass(CCSCustomHudLayout *layout, CPlayerSlot slot, const char *panelId, const char *className, bool on)
{
	layout->SetHasClassForPlayer(slot, panelId, className, on ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
}

// m = menu, l = label, h = horizontal, v = vertical, p = plus, n = minus.
static_global const char *MENU_FONT_PANELS[] = {
	"menu_root", "menu_title", "m_step_readout", "m_l_v_n5", "m_l_v_n1", "m_l_v_p1",  "m_l_v_p5", "m_l_h_n5",
	"m_l_h_n1",  "m_l_h_p1",   "m_l_h_p5",       "m_l_prev", "m_l_back", "m_l_close", "m_l_next",
};

static_function CCSCustomHudLayout *MenuLayout()
{
	return KZHUDService::GetLayoutEntity(KZ_MENU_LAYOUT, g_hMenuLayout);
}

std::string KZMenuPhrase(KZPlayer *player, const char *key)
{
	return player->languageService->PrepareMessage(key);
}

std::string KZMenuToggleLabel(KZPlayer *player, const char *phraseKey, bool on)
{
	return KZMenuPhrase(player, phraseKey) + ": " + KZMenuPhrase(player, on ? "Menu - On" : "Menu - Off");
}

std::string KZMenuValueLabel(KZPlayer *player, const char *phraseKey, const char *value)
{
	return KZMenuPhrase(player, phraseKey) + ": " + value;
}

// === Categories and the root page ===================================================

struct MenuCategory
{
	KZMenuPage *page;
	KZMenuContext context;
};

static_function std::vector<MenuCategory> &Categories()
{
	static_persist std::vector<MenuCategory> categories;
	return categories;
}

static_global class RootPage : public KZMenuPage
{
	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override
	{
		return KZMenuPhrase(player, "Menu - Title Options");
	}

	virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) override
	{
		for (const MenuCategory &category : Categories())
		{
			KZMenuRow row;
			row.label = category.page->GetTitle(player, category.context) + " >";
			row.submenu = category.page;
			row.param = category.context;
			rows.push_back(row);
		}
	}
} g_rootPage;

// === The menu's own appearance ======================================================

static_global class MenuFontPage : public KZMenuFontPage
{
	virtual const char *GetCurrentSlug(KZPlayer *player, KZMenuContext context) override
	{
		return PanoramaResolveFontSlug(player->optionService->GetPreferenceStr("menuFont", KZ_MENU_DEFAULT_FONT), KZ_MENU_DEFAULT_FONT);
	}

	virtual void OnFontPicked(KZPlayer *player, KZMenuContext context, const char *slug) override
	{
		player->optionService->SetPreferenceStr("menuFont", slug);
	}
} g_menuFontPage;

static_global class MenuColorPage : public KZMenuColorPage
{
	virtual Color GetCurrentColor(KZPlayer *player, KZMenuContext context) override
	{
		return player->optionService->GetPreferenceColor("menuColor", KZ_MENU_DEFAULT_COLOR);
	}

	virtual void OnColorPicked(KZPlayer *player, KZMenuContext context, const Color &color) override
	{
		player->optionService->SetPreferenceColor("menuColor", color);
	}
} g_menuColorPage;

static_global class MenuChromePage : public KZMenuPage
{
	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override
	{
		return KZMenuPhrase(player, "Menu - Menu");
	}

	virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) override
	{
		auto *opts = player->optionService;
		KZMenuRow row;

		row.label = KZMenuValueLabel(player, "Menu - Font",
									 PanoramaFontDisplayName(opts->GetPreferenceStr("menuFont", KZ_MENU_DEFAULT_FONT), KZ_MENU_DEFAULT_FONT))
					+ " >";
		row.submenu = &g_menuFontPage;
		rows.push_back(row);

		const char *colorClass = PanoramaNearestColorClass(opts->GetPreferenceColor("menuColor", KZ_MENU_DEFAULT_COLOR));
		row.label = KZMenuValueLabel(player, "Menu - Color", colorClass + PANORAMA_COLOR_NAME_OFFSET) + " >";
		row.submenu = &g_menuColorPage;
		row.colorClass = colorClass;
		rows.push_back(row);
	}
} g_menuChromePage;

static_global class MiscPage : public KZMenuPage
{
	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override
	{
		return KZMenuPhrase(player, "Menu - Misc");
	}

	virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) override
	{
		KZMenuRow row;
		row.label = KZMenuPhrase(player, "Menu - Menu") + " >";
		row.submenu = &g_menuChromePage;
		rows.push_back(row);
	}
} g_miscPage;

void KZMenuService::Init()
{
	KZMenuService::RegisterCategory(&g_miscPage);
}

void KZMenuService::RegisterCategory(KZMenuPage *page, KZMenuContext context)
{
	Categories().push_back({page, context});
}

// === Navigation =====================================================================

void KZMenuService::OpenPage(KZMenuPage *page, KZMenuContext context)
{
	this->stack.push_back({page, context, 0});
	this->navSerial++;
	page->OnEnter(this->player, context);
	this->Render();
}

void KZMenuService::GoBack()
{
	if (this->stack.size() <= 1)
	{
		this->Close();
		return;
	}
	Frame frame = this->stack.back();
	this->stack.pop_back();
	this->navSerial++;
	frame.page->OnLeave(this->player, frame.context);
	this->Render();
}

void KZMenuService::Refresh()
{
	this->Render();
}

void KZMenuService::PickRow(i32 row)
{
	if (row < 0 || row >= KZ_MENU_ROWS || !this->rowUsed[row] || this->applied.rowDisabled[row] || this->stack.empty())
	{
		return;
	}
	if (this->rowPage[row])
	{
		this->OpenPage(this->rowPage[row], this->rowParam[row]);
		return;
	}
	const Frame frame = this->stack.back();
	const i32 serial = this->navSerial;
	frame.page->OnRowPicked(this->player, frame.context, this->rowParam[row]);
	// A row that navigated has already rendered its new page.
	if (this->navSerial == serial)
	{
		this->Render();
	}
}

void KZMenuService::ApplyStep(bool vertical, i32 delta)
{
	if (this->stack.empty() || !this->stack.back().page->IsStepper())
	{
		return;
	}
	const Frame frame = this->stack.back();
	frame.page->OnStep(this->player, frame.context, vertical, delta);
	this->Render();
}

// === Rendering ======================================================================

void KZMenuService::Render()
{
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	CCSCustomHudLayout *layout = MenuLayout();
	if (!layout || !layout->GetPlayerLayoutState(slot))
	{
		return;
	}

	// Dialog variables are said again from scratch every pass; classes are not, see AppliedClasses.
	this->written = WrittenState();
	if (this->layoutEntity.Get() != (CBaseEntity *)layout)
	{
		this->layoutEntity = layout->GetRefEHandle();
		this->applied = AppliedClasses();
	}

	if (this->applied.rootHidden != !this->open)
	{
		this->applied.rootHidden = !this->open;
		SetClass(layout, slot, "menu_root", "hidden", this->applied.rootHidden);
	}
	if (!this->open || this->stack.empty())
	{
		return;
	}

	Frame &frame = this->stack.back();
	std::vector<KZMenuRow> rows;
	std::string title = frame.page->GetTitle(this->player, frame.context);
	const bool stepPage = frame.page->IsStepper();
	std::string step;
	if (stepPage)
	{
		step = frame.page->GetStepReadout(this->player, frame.context);
	}
	else
	{
		frame.page->BuildRows(this->player, frame.context, rows);
	}

	const char *fontClass = PanoramaFontClass(this->player->optionService->GetPreferenceStr("menuFont", KZ_MENU_DEFAULT_FONT), KZ_MENU_DEFAULT_FONT);
	if (this->applied.fontClass != fontClass)
	{
		for (const char *panel : MENU_FONT_PANELS)
		{
			if (this->applied.fontClass)
			{
				SetClass(layout, slot, panel, this->applied.fontClass, false);
			}
			SetClass(layout, slot, panel, fontClass, true);
		}
		for (i32 i = 0; i < KZ_MENU_ROWS; i++)
		{
			if (this->applied.fontClass)
			{
				SetClass(layout, slot, TextPanel(i), this->applied.fontClass, false);
			}
			SetClass(layout, slot, TextPanel(i), fontClass, true);
		}
		this->applied.fontClass = fontClass;
	}
	const char *menuColor = PanoramaNearestColorClass(this->player->optionService->GetPreferenceColor("menuColor", KZ_MENU_DEFAULT_COLOR));

	if (this->written.title != title)
	{
		this->written.title = title;
		layout->SetDialogVariableStringForPlayer(slot, "menu_title", "title", title.c_str());
	}

	// A step page has no rows, so the whole row list collapses instead.
	i32 scroll = 0;
	i32 visible = 0;
	if (!stepPage)
	{
		if (frame.scroll >= (i32)rows.size())
		{
			frame.scroll = 0;
		}
		scroll = frame.scroll;
		visible = MIN(KZ_MENU_ROWS, (i32)rows.size() - scroll);
	}

	for (i32 i = 0; i < KZ_MENU_ROWS; i++)
	{
		const bool used = i < visible;
		this->rowUsed[i] = used;
		if (used)
		{
			const KZMenuRow &row = rows[scroll + i];
			const char *colorClass = row.colorClass ? row.colorClass : menuColor;
			if (this->written.rowText[i] != row.label)
			{
				this->written.rowText[i] = row.label;
				layout->SetDialogVariableStringForPlayer(slot, TextPanel(i), RowVar(i), row.label.c_str());
			}
			if (this->applied.rowColor[i] != colorClass)
			{
				if (this->applied.rowColor[i])
				{
					SetClass(layout, slot, TextPanel(i), this->applied.rowColor[i], false);
				}
				SetClass(layout, slot, TextPanel(i), colorClass, true);
				this->applied.rowColor[i] = colorClass;
			}
			this->rowPage[i] = row.submenu;
			this->rowParam[i] = row.param;
		}

		// applied.rowDisabled doubles as what PickRow refuses to route: it is rewritten to the
		// current row every pass, so it never lags what is on screen.
		const bool disabled = used && rows[scroll + i].disabled;
		if (this->applied.rowDisabled[i] != disabled)
		{
			this->applied.rowDisabled[i] = disabled;
			SetClass(layout, slot, RowPanel(i), "disabled", disabled);
		}

		if (this->applied.rowHidden[i] != !used)
		{
			this->applied.rowHidden[i] = !used;
			SetClass(layout, slot, RowPanel(i), "hidden", this->applied.rowHidden[i]);
		}
	}

	if (this->applied.rowsHidden != stepPage)
	{
		this->applied.rowsHidden = stepPage;
		SetClass(layout, slot, "menu_rows", "hidden", stepPage);
	}
	if (this->applied.stepHidden != !stepPage)
	{
		this->applied.stepHidden = !stepPage;
		SetClass(layout, slot, "m_step", "hidden", this->applied.stepHidden);
	}
	const bool vStepHidden = !stepPage || !frame.page->HasVerticalStep();
	if (this->applied.vStepHidden != vStepHidden)
	{
		this->applied.vStepHidden = vStepHidden;
		SetClass(layout, slot, "m_step_up", "hidden", vStepHidden);
		SetClass(layout, slot, "m_step_down", "hidden", vStepHidden);
	}
	if (stepPage && this->written.step != step)
	{
		this->written.step = step;
		layout->SetDialogVariableStringForPlayer(slot, "m_step_readout", "step", step.c_str());
	}

	const bool prevHidden = stepPage || scroll <= 0;
	const bool nextHidden = stepPage || scroll + visible >= (i32)rows.size();
	if (this->applied.prevHidden != prevHidden)
	{
		this->applied.prevHidden = prevHidden;
		SetClass(layout, slot, "m_prev", "hidden", prevHidden);
	}
	if (this->applied.nextHidden != nextHidden)
	{
		this->applied.nextHidden = nextHidden;
		SetClass(layout, slot, "m_next", "hidden", nextHidden);
	}
}

// === Click routing ==================================================================

void KZMenuService::OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId)
{
	// Two layouts exist; only clicks on ours are ours.
	if ((CBaseEntity *)layout != g_hMenuLayout.Get())
	{
		return;
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (!player || !player->menuService->IsOpen())
	{
		return;
	}
	KZMenuService *menu = player->menuService;

	if (V_strcmp(buttonId, "m_close") == 0)
	{
		menu->Close();
	}
	else if (V_strcmp(buttonId, "m_back") == 0)
	{
		menu->GoBack();
	}
	else if (menu->stack.empty())
	{
		return;
	}
	else if (V_strcmp(buttonId, "m_prev") == 0)
	{
		menu->stack.back().scroll = MAX(0, menu->stack.back().scroll - KZ_MENU_ROWS);
		menu->Render();
	}
	else if (V_strcmp(buttonId, "m_next") == 0)
	{
		menu->stack.back().scroll += KZ_MENU_ROWS;
		menu->Render();
	}
	else if (V_strncmp(buttonId, "m_row", 5) == 0)
	{
		menu->PickRow(atoi(buttonId + 5));
	}
	else if (V_strcmp(buttonId, "m_v_n5") == 0)
	{
		menu->ApplyStep(true, -5);
	}
	else if (V_strcmp(buttonId, "m_v_n1") == 0)
	{
		menu->ApplyStep(true, -1);
	}
	else if (V_strcmp(buttonId, "m_v_p1") == 0)
	{
		menu->ApplyStep(true, 1);
	}
	else if (V_strcmp(buttonId, "m_v_p5") == 0)
	{
		menu->ApplyStep(true, 5);
	}
	else if (V_strcmp(buttonId, "m_h_n5") == 0)
	{
		menu->ApplyStep(false, -5);
	}
	else if (V_strcmp(buttonId, "m_h_n1") == 0)
	{
		menu->ApplyStep(false, -1);
	}
	else if (V_strcmp(buttonId, "m_h_p1") == 0)
	{
		menu->ApplyStep(false, 1);
	}
	else if (V_strcmp(buttonId, "m_h_p5") == 0)
	{
		menu->ApplyStep(false, 5);
	}
}

// === Public API =====================================================================

void KZMenuService::DropCapture()
{
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	while (!this->stack.empty())
	{
		Frame frame = this->stack.back();
		this->stack.pop_back();
		frame.page->OnLeave(this->player, frame.context);
	}
	this->navSerial++;
	if (CCSCustomHudLayout *layout = MenuLayout())
	{
		SetClass(layout, slot, "menu_root", "hidden", true);
		this->applied.rootHidden = true;
		layout->SetInputCaptureEnabled(slot, false);
	}
	if (g_pMenus)
	{
		g_pMenus->SetExternalBusy(slot.Get(), false);
	}
}

void KZMenuService::Close()
{
	if (!this->open)
	{
		return;
	}
	this->open = false;
	this->DropCapture();
}

void KZMenuService::Toggle()
{
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	if (this->open)
	{
		this->Close();
		return;
	}

	CCSCustomHudLayout *layout = MenuLayout();
	if (!layout || !layout->GetPlayerLayoutState(slot))
	{
		this->player->languageService->PrintChat(true, false, "Menu - Unavailable");
		return;
	}

	if (g_pMenus)
	{
		// Take the slot off cs2menus so the two never fight over the player's input.
		g_pMenus->CancelMenu(slot.Get());
		g_pMenus->SetExternalBusy(slot.Get(), true);
	}

	this->open = true;
	this->stack.clear();
	this->stack.push_back({&g_rootPage, {}, 0});
	layout->SetInputCaptureEnabled(slot, true);
	this->Render();
}

void KZMenuService::Reset()
{
	if (this->open)
	{
		this->open = false;
		this->DropCapture();
	}
	this->stack.clear();
	this->written = WrittenState();
	this->applied = AppliedClasses();
}

void KZMenuService::Cleanup()
{
	for (i32 i = 0; i < MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(i));
		if (player && player->menuService && player->menuService->IsOpen())
		{
			player->menuService->Reset();
		}
	}
}

SCMD(kz_options, SCFL_PREFERENCE)
{
	g_pKZPlayerManager->ToPlayer(controller)->menuService->Toggle();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_o, kz_options);
