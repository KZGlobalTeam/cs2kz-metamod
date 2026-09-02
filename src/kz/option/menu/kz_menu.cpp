#include "cs2kz.h"
#include "kz/option/menu/kz_menu.h"
#include "kz/option/menu/model.h"
#include "kz/option/menu/tables.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "sdk/datatypes.h"
#include "entitykeyvalues.h"
#include "checktransmitinfo.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"

#include <vendor/mm-cs2menus/src/public/ics2menus.h>
extern ICS2Menus *g_pMenus;

#include "tier0/memdbgon.h"

#define KZ_MENU_LAYOUT       "panorama/layout/custom_game/cs2kz/menu.xml"
#define KZ_MENU_DEFAULT_FONT "stratum2-medium-tf"

static_global const Color KZ_MENU_DEFAULT_COLOR(255, 255, 255, 255);

// Color pages derive from the picker entry count (solids + gradients) and the per-page swatch count.
static_function i32 GetColorPageCount()
{
	return MAX(1, (panorama::GetColorEntryCount() + KZ_MENU_SWATCH - 1) / KZ_MENU_SWATCH);
}

// The menu registers its own appearance category. Defined in prefs/menu_prefs.cpp.
void MenuRegisterChromePrefs();

// === Panel id / dialog var helpers (each its own static buffer) ======================

#define SLOT_ID(fn, fmt) \
	static_function const char *fn(i32 i) \
	{ \
		static_persist char buf[24]; \
		V_snprintf(buf, sizeof(buf), fmt, i); \
		return buf; \
	}

SLOT_ID(CatPanel, "cat%i")
SLOT_ID(CatLbl, "cat_lbl%i")
SLOT_ID(CatVar, "cl%i")
SLOT_ID(ItemPanel, "item%i")
SLOT_ID(ItemLbl, "item_lbl%i")
SLOT_ID(ItemLblVar, "il%i")
SLOT_ID(ItemSub, "item_sub%i")
SLOT_ID(ItemSubVar, "is%i")
SLOT_ID(ItemVal, "item_val%i")
SLOT_ID(ItemValVar, "iv%i")
SLOT_ID(ItemSw, "item_sw%i")
SLOT_ID(LiPanel, "li%i")
SLOT_ID(LiLbl, "li_lbl%i")
SLOT_ID(LiVar, "ll%i")
SLOT_ID(SwPanel, "sw%i")
#undef SLOT_ID

static_function const char *GetTypeClass(KZOptItemType type)
{
	switch (type)
	{
		case KZOptItemType::Toggle:
			return "type-toggle";
		case KZOptItemType::Color:
			return "type-color";
		case KZOptItemType::Font:
			return "type-font";
		case KZOptItemType::Position:
			return "type-position";
		case KZOptItemType::Size:
			return "type-size";
		case KZOptItemType::Button:
			return "type-button";
		case KZOptItemType::Choice:
			return "type-choice";
	}
	return "type-button";
}

std::string KZMenuPhrase(KZPlayer *player, const char *key)
{
	return player->languageService->PrepareMessage(key);
}

// === Owned entity (mirrors the HUD's pattern) ========================================

CCSCustomHudLayout *KZMenuService::EnsureMenuLayout(bool &created)
{
	created = false;
	if (g_KZPlugin.unloading || !KZHUDService::IsLayoutHudAvailable())
	{
		return NULL;
	}
	if (CBaseEntity *cached = this->layoutEntity.Get())
	{
		return (CCSCustomHudLayout *)cached;
	}
	CCSCustomHudLayout *layout = utils::CreateEntityByName<CCSCustomHudLayout>("custom_hud_layout");
	if (!layout)
	{
		return NULL;
	}
	CEntityKeyValues *kv = new CEntityKeyValues();
	kv->SetString("layout", KZ_MENU_LAYOUT);
	char name[32];
	V_snprintf(name, sizeof(name), "kzmenu%i", this->player->GetPlayerSlot().Get());
	kv->SetString("targetname", name);
	layout->DispatchSpawn(kv);
	this->layoutEntity = layout->GetRefEHandle();
	created = true;
	return layout;
}

CCSCustomHudLayout *KZMenuService::MenuLayout()
{
	bool created = false;
	return this->EnsureMenuLayout(created);
}

void KZMenuService::DestroyOwnedLayout()
{
	if (CBaseEntity *ent = this->layoutEntity.Get())
	{
		g_pKZUtils->RemoveEntity(ent);
	}
	this->layoutEntity = nullptr;
}

// === Registration ====================================================================

void KZMenuService::Init()
{
	MenuRegisterChromePrefs();
}

// === Write helpers ===================================================================

void KZMenuService::SetClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool on)
{
	layout->SetHasClass(panelId, className, on ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
}

void KZMenuService::SetBoolClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool &cache, bool want)
{
	if (cache != want)
	{
		cache = want;
		this->SetClass(layout, panelId, className, want);
	}
}

void KZMenuService::SetSwapClass(CCSCustomHudLayout *layout, const char *panelId, const char *&cache, const char *want)
{
	if (cache == want)
	{
		return;
	}
	if (cache)
	{
		this->SetClass(layout, panelId, cache, false);
	}
	if (want)
	{
		this->SetClass(layout, panelId, want, true);
	}
	cache = want;
}

void KZMenuService::SetVar(CCSCustomHudLayout *layout, const char *panelId, const char *var, const char *value)
{
	// Every SetDialogVariableString marks the whole entity for a full network resend, so writing an
	// unchanged value is far from free. Skip it when the value has not changed since the last render.
	std::string &cached = this->writtenVars[var];
	if (cached == value)
	{
		return;
	}
	cached = value;
	layout->SetDialogVariableString(panelId, var, value);
}

// === Model navigation ================================================================

KZOptNode *KZMenuService::ActiveNode()
{
	const std::vector<KZOptNode *> &tree = KZMenu::Tree();
	if (this->selectedCategory < 0 || this->selectedCategory >= (i32)tree.size())
	{
		return NULL;
	}
	KZOptNode *cat = tree[this->selectedCategory];
	if (cat->subs.empty())
	{
		return cat;
	}
	if (this->selectedSub >= 0 && this->selectedSub < (i32)cat->subs.size())
	{
		return cat->subs[this->selectedSub];
	}
	return NULL;
}

i32 KZMenuService::BuildLeft()
{
	const std::vector<KZOptNode *> &tree = KZMenu::Tree();
	i32 n = 0;
	for (i32 ci = 0; ci < (i32)tree.size() && n < KZ_MENU_CATS; ci++)
	{
		this->leftSlots[n++] = {tree[ci], false, ci, -1};
		if (ci == this->selectedCategory)
		{
			KZOptNode *cat = tree[ci];
			for (i32 si = 0; si < (i32)cat->subs.size() && n < KZ_MENU_CATS; si++)
			{
				this->leftSlots[n++] = {cat->subs[si], true, ci, si};
			}
		}
	}
	this->leftCount = n;
	return n;
}

const KZOptItem *KZMenuService::PopupItem()
{
	KZOptNode *node = this->ActiveNode();
	if (!node || this->popupItemIndex < 0 || this->popupItemIndex >= (i32)node->items.size())
	{
		return NULL;
	}
	// Only hand back an item whose type matches the open popup, so a stale index can never feed a
	// mismatched item (e.g. a NULL prefKey) into rendering.
	const KZOptItem &it = node->items[this->popupItemIndex];
	const bool ok = (this->popup == Popup::Color && it.type == KZOptItemType::Color)
					|| (this->popup == Popup::List && (it.type == KZOptItemType::Font || it.type == KZOptItemType::Choice))
					|| (this->popup == Popup::Step && (it.type == KZOptItemType::Position || it.type == KZOptItemType::Size));
	return ok ? &it : NULL;
}

// === Rendering =======================================================================

void KZMenuService::Render()
{
	bool created = false;
	CCSCustomHudLayout *layout = this->EnsureMenuLayout(created);
	if (!layout)
	{
		return;
	}
	if (created)
	{
		this->applied = Applied();
		this->writtenVars.clear();
	}

	this->SetBoolClass(layout, "menu_root", "hidden", this->applied.rootHidden, !this->open);
	if (!this->open)
	{
		return;
	}

	// The box stays put and centred; a popup is a third panel that appears to its right.
	this->SetBoolClass(layout, "color_popup", "hidden", this->applied.colorHidden, this->popup != Popup::Color);
	this->SetBoolClass(layout, "list_popup", "hidden", this->applied.listHidden, this->popup != Popup::List);
	this->SetBoolClass(layout, "step_popup", "hidden", this->applied.stepHidden, this->popup != Popup::Step);

	this->RenderChrome(layout);
	this->RenderLeft(layout);
	this->RenderItems(layout);

	if (this->popup == Popup::Color)
	{
		this->RenderColorPopup(layout);
	}
	else if (this->popup == Popup::List)
	{
		this->RenderListPopup(layout);
	}
	else if (this->popup == Popup::Step)
	{
		this->RenderStepPopup(layout);
	}
}

void KZMenuService::RenderChrome(CCSCustomHudLayout *layout)
{
	auto *opts = this->player->optionService;
	const char *font = panorama::ResolveFontClass(opts->GetPreferenceStr("menuFont", KZ_MENU_DEFAULT_FONT), KZ_MENU_DEFAULT_FONT);
	const char *color = panorama::ResolveColorClass(opts->GetPreferenceColor("menuColor", KZ_MENU_DEFAULT_COLOR));

	// Panorama only re-measures/re-tints a panel when a class lands on that panel itself; a font or
	// color class set on a parent is inherited for new children but does not re-lay-out or re-color
	// the ones already on screen. So the menu font and color cannot be set once on a container - they
	// are stamped on every text panel individually. When either changes we sweep the whole set,
	// removing the previously stamped class and adding the new one on each panel.
	if (this->applied.menuFont != font || this->applied.menuColor != color)
	{
		const bool fontChanged = this->applied.menuFont != font;
		const bool colorChanged = this->applied.menuColor != color;
		auto stamp = [&](const char *panel)
		{
			if (fontChanged)
			{
				if (this->applied.menuFont)
				{
					this->SetClass(layout, panel, this->applied.menuFont, false);
				}
				this->SetClass(layout, panel, font, true);
			}
			if (colorChanged)
			{
				if (this->applied.menuColor)
				{
					this->SetClass(layout, panel, this->applied.menuColor, false);
				}
				this->SetClass(layout, panel, color, true);
			}
		};
		stamp("menu_title");
		for (i32 i = 0; i < KZ_MENU_CATS; i++)
		{
			stamp(CatLbl(i));
		}
		for (i32 i = 0; i < KZ_MENU_ITEMS; i++)
		{
			stamp(ItemLbl(i));
			stamp(ItemSub(i));
			stamp(ItemVal(i));
		}
		this->applied.menuFont = font;
		this->applied.menuColor = color;
	}

	this->SetVar(layout, "menu_title", "title", KZMenuPhrase(this->player, "Menu - Title Options").c_str());
}

void KZMenuService::RenderLeft(CCSCustomHudLayout *layout)
{
	const i32 count = this->BuildLeft();
	KZOptNode *active = this->ActiveNode();
	for (i32 i = 0; i < KZ_MENU_CATS; i++)
	{
		const bool used = i < count;
		if (used)
		{
			const LeftEntry &e = this->leftSlots[i];
			// Every top-level category is styled as a header (with or without subs, so a leaf category
			// reads the same as one that owns subs). A category that owns subs is also inert once one of
			// its subs is active - the user switches by clicking another category.
			const bool isParent = !e.isSub;
			const bool disabled = isParent && !e.node->subs.empty() && e.categoryIndex == this->selectedCategory;
			this->SetVar(layout, CatLbl(i), CatVar(i), KZMenuPhrase(this->player, e.node->phraseKey).c_str());
			this->SetBoolClass(layout, CatPanel(i), "indent", this->applied.catIndent[i], e.isSub);
			this->SetBoolClass(layout, CatPanel(i), "cat-parent", this->applied.catParent[i], isParent);
			this->SetBoolClass(layout, CatPanel(i), "disabled", this->applied.catDisabled[i], disabled);
			this->SetBoolClass(layout, CatPanel(i), "selected", this->applied.catSel[i], e.node == active);
		}
		this->SetBoolClass(layout, CatPanel(i), "hidden", this->applied.catHidden[i], !used);
	}
}

void KZMenuService::RenderItems(CCSCustomHudLayout *layout)
{
	KZOptNode *node = this->ActiveNode();
	this->itemCount = node ? MIN((i32)node->items.size(), KZ_MENU_ITEMS) : 0;
	auto *opts = this->player->optionService;

	for (i32 i = 0; i < KZ_MENU_ITEMS; i++)
	{
		const bool used = i < this->itemCount;
		this->itemSlots[i] = used ? &node->items[i] : NULL;
		if (used)
		{
			const KZOptItem &it = node->items[i];
			this->SetVar(layout, ItemLbl(i), ItemLblVar(i), KZMenuPhrase(this->player, it.phraseKey).c_str());

			std::string value;
			const char *swatch = NULL;
			bool on = false;
			switch (it.type)
			{
				case KZOptItemType::Toggle:
					on = it.getCurrent ? it.getCurrent(this->player, it.tag) != 0 : opts->GetPreferenceBool(it.prefKey, it.idef != 0);
					value = KZMenuPhrase(this->player, on ? "Menu - On" : "Menu - Off");
					break;
				case KZOptItemType::Color:
					swatch = panorama::ResolveSwatchClass(opts->GetPreferenceColor(it.prefKey, it.cdef));
					break;
				case KZOptItemType::Font:
					value = panorama::GetFontDisplayName(opts->GetPreferenceStr(it.prefKey, it.sdef), it.sdef);
					break;
				case KZOptItemType::Position:
				{
					char buf[32];
					V_snprintf(buf, sizeof(buf), "%i%%, %i%%", (i32)opts->GetPreferenceFloat(it.prefKey, it.idef),
							   (i32)opts->GetPreferenceFloat(it.yKey, it.iydef));
					value = buf;
					break;
				}
				case KZOptItemType::Size:
				{
					char buf[16];
					V_snprintf(buf, sizeof(buf), "%ipx", (i32)opts->GetPreferenceFloat(it.prefKey, it.idef));
					value = buf;
					break;
				}
				case KZOptItemType::Button:
					break;
				case KZOptItemType::Choice:
				{
					if (it.getChoices && it.getCurrent)
					{
						std::vector<KZChoice> choices;
						it.getChoices(this->player, it.tag, choices);
						const i64 cur = it.getCurrent(this->player, it.tag);
						for (const KZChoice &c : choices)
						{
							if (c.id == cur)
							{
								value = c.label;
								break;
							}
						}
					}
					break;
				}
			}

			this->SetVar(layout, ItemVal(i), ItemValVar(i), value.c_str());
			this->SetVar(layout, ItemSub(i), ItemSubVar(i), it.subKey ? KZMenuPhrase(this->player, it.subKey).c_str() : "");
			this->SetSwapClass(layout, ItemSw(i), this->applied.itemSwatch[i], swatch);
			this->SetSwapClass(layout, ItemPanel(i), this->applied.itemType[i], GetTypeClass(it.type));
			this->SetBoolClass(layout, ItemPanel(i), "on", this->applied.itemOn[i], on);
			this->SetBoolClass(layout, ItemPanel(i), "has-sub", this->applied.itemSub[i], it.subKey != NULL);
			this->SetBoolClass(layout, ItemPanel(i), "divider", this->applied.itemDiv[i], it.dividerAfter);
		}
		this->SetBoolClass(layout, ItemPanel(i), "hidden", this->applied.itemHidden[i], !used);
	}
}

void KZMenuService::RenderColorPopup(CCSCustomHudLayout *layout)
{
	// Highlight the entry matching the item's current color, if it is on this page.
	i32 curIdx = -1;
	if (const KZOptItem *it = this->PopupItem())
	{
		curIdx = panorama::FindColorEntry(this->player->optionService->GetPreferenceColor(it->prefKey, it->cdef));
	}
	const i32 total = panorama::GetColorEntryCount();
	for (i32 i = 0; i < KZ_MENU_SWATCH; i++)
	{
		const i32 idx = this->popupPage * KZ_MENU_SWATCH + i;
		const bool used = idx < total;
		if (used)
		{
			this->SetSwapClass(layout, SwPanel(i), this->applied.swBg[i], panorama::GetColorEntryBgClass(idx));
			this->SetBoolClass(layout, SwPanel(i), "selected", this->applied.swSel[i], idx == curIdx);
		}
		this->SetBoolClass(layout, SwPanel(i), "hidden", this->applied.swHidden[i], !used);
	}
	char page[16];
	V_snprintf(page, sizeof(page), "%i/%i", this->popupPage + 1, GetColorPageCount());
	this->SetVar(layout, "cp_page", "cppage", page);
}

void KZMenuService::RenderListPopup(CCSCustomHudLayout *layout)
{
	const i32 n = (i32)this->listChoices.size();
	const i32 pages = MAX(1, (n + KZ_MENU_LIST - 1) / KZ_MENU_LIST);

	i64 curId = -1;
	const KZOptItem *it = this->PopupItem();
	if (this->popupFont && it)
	{
		const char *slug = panorama::ResolveFontSlug(this->player->optionService->GetPreferenceStr(it->prefKey, it->sdef), it->sdef);
		for (i32 f = 0; f < PANORAMA_FONT_COUNT; f++)
		{
			if (V_strcmp(slug, PANORAMA_FONTS[f].slug) == 0)
			{
				curId = f;
				break;
			}
		}
	}
	else if (it && it->getCurrent)
	{
		curId = it->getCurrent(this->player, it->tag);
	}

	const char *menuFont = panorama::ResolveFontClass(this->player->optionService->GetPreferenceStr("menuFont", KZ_MENU_DEFAULT_FONT), KZ_MENU_DEFAULT_FONT);

	for (i32 i = 0; i < KZ_MENU_LIST; i++)
	{
		const i32 idx = this->popupPage * KZ_MENU_LIST + i;
		const bool used = idx < n;
		if (used)
		{
			const KZChoice &c = this->listChoices[idx];
			this->SetVar(layout, LiLbl(i), LiVar(i), c.label.c_str());
			this->SetBoolClass(layout, LiPanel(i), "selected", this->applied.liSel[i], c.id == curId);
			// Font rows preview their own face; choice rows use the menu font.
			const char *face = this->popupFont ? PANORAMA_FONTS[c.id].className : menuFont;
			this->SetSwapClass(layout, LiLbl(i), this->applied.liFont[i], face);
		}
		this->SetBoolClass(layout, LiPanel(i), "hidden", this->applied.liHidden[i], !used);
	}
	char page[16];
	V_snprintf(page, sizeof(page), "%i/%i", this->popupPage + 1, pages);
	this->SetVar(layout, "lp_page", "lppage", page);
}

void KZMenuService::RenderStepPopup(CCSCustomHudLayout *layout)
{
	const KZOptItem *it = this->PopupItem();
	if (!it)
	{
		return;
	}
	const bool vstep = it->type == KZOptItemType::Position;
	if (this->applied.vstepHidden != !vstep)
	{
		this->applied.vstepHidden = !vstep;
		this->SetClass(layout, "m_step_up", "hidden", !vstep);
		this->SetClass(layout, "m_step_down", "hidden", !vstep);
	}
	auto *opts = this->player->optionService;
	char readout[32];
	if (vstep)
	{
		V_snprintf(readout, sizeof(readout), "%i%%, %i%%", (i32)opts->GetPreferenceFloat(it->prefKey, it->idef),
				   (i32)opts->GetPreferenceFloat(it->yKey, it->iydef));
	}
	else
	{
		V_snprintf(readout, sizeof(readout), "%ipx", (i32)opts->GetPreferenceFloat(it->prefKey, it->idef));
	}
	this->SetVar(layout, "step_readout", "step", readout);
	this->SetVar(layout, "step_label", "steplabel", KZMenuPhrase(this->player, it->phraseKey).c_str());
}

// === Interaction =====================================================================

void KZMenuService::SelectLeft(i32 slot)
{
	if (slot < 0 || slot >= this->leftCount)
	{
		return;
	}
	// The panes stay live while a picker is open; a click on them just closes the picker first, so it
	// can never be left pointing at an item the node change removed.
	if (this->popup != Popup::None)
	{
		this->ClosePopup();
	}
	const LeftEntry &e = this->leftSlots[slot];
	if (e.isSub)
	{
		this->selectedSub = e.subIndex;
	}
	else
	{
		// The active category's header is not a click target once a sub of it is chosen.
		if (!e.node->subs.empty() && e.categoryIndex == this->selectedCategory)
		{
			return;
		}
		this->selectedCategory = e.categoryIndex;
		KZOptNode *cat = KZMenu::Tree()[this->selectedCategory];
		this->selectedSub = cat->subs.empty() ? -1 : 0;
	}
	this->Render();
}

void KZMenuService::ActivateItem(i32 slot)
{
	if (slot < 0 || slot >= this->itemCount || !this->itemSlots[slot])
	{
		return;
	}
	// Clicking any item closes an open picker first, then acts (which may open a new one).
	if (this->popup != Popup::None)
	{
		this->ClosePopup();
	}
	const KZOptItem &it = *this->itemSlots[slot];
	switch (it.type)
	{
		case KZOptItemType::Toggle:
			if (it.onActivate)
			{
				it.onActivate(this->player, it.tag);
			}
			else
			{
				this->player->optionService->SetPreferenceBool(it.prefKey, !this->player->optionService->GetPreferenceBool(it.prefKey, it.idef != 0));
			}
			this->Render();
			break;
		case KZOptItemType::Color:
			this->OpenPopup(Popup::Color, slot);
			break;
		case KZOptItemType::Font:
		case KZOptItemType::Choice:
			this->OpenPopup(Popup::List, slot);
			break;
		case KZOptItemType::Position:
		case KZOptItemType::Size:
			this->OpenPopup(Popup::Step, slot);
			break;
		case KZOptItemType::Button:
			if (it.onActivate)
			{
				it.onActivate(this->player, it.tag);
			}
			this->Render();
			break;
	}
}

void KZMenuService::OpenPopup(Popup kind, i32 itemIdx)
{
	KZOptNode *node = this->ActiveNode();
	if (!node || itemIdx < 0 || itemIdx >= (i32)node->items.size())
	{
		return;
	}
	const KZOptItem &it = node->items[itemIdx];
	this->popup = kind;
	this->popupItemIndex = itemIdx;
	this->popupPage = 0;

	if (kind == Popup::List)
	{
		this->popupFont = it.type == KZOptItemType::Font;
		this->listChoices.clear();
		if (this->popupFont)
		{
			for (i32 f = 0; f < PANORAMA_FONT_COUNT; f++)
			{
				this->listChoices.push_back({PANORAMA_FONTS[f].displayName, f, NULL});
			}
		}
		else if (it.getChoices)
		{
			it.getChoices(this->player, it.tag, this->listChoices);
		}
	}

	if (it.onEdit)
	{
		it.onEdit(this->player, it.tag, true);
	}
	this->Render();
}

void KZMenuService::ClosePopup()
{
	if (const KZOptItem *it = this->PopupItem())
	{
		if (it->onEdit)
		{
			it->onEdit(this->player, it->tag, false);
		}
	}
	this->popup = Popup::None;
	this->popupItemIndex = -1;
	this->listChoices.clear();
	this->Render();
}

void KZMenuService::PopupPageStep(i32 delta)
{
	i32 pages = 1;
	if (this->popup == Popup::Color)
	{
		pages = GetColorPageCount();
	}
	else if (this->popup == Popup::List)
	{
		pages = MAX(1, ((i32)this->listChoices.size() + KZ_MENU_LIST - 1) / KZ_MENU_LIST);
	}
	this->popupPage = clamp(this->popupPage + delta, 0, pages - 1);
	this->Render();
}

void KZMenuService::PopupPick(i32 slot)
{
	const KZOptItem *it = this->PopupItem();
	if (!it)
	{
		return;
	}
	if (this->popup == Popup::Color)
	{
		const i32 idx = this->popupPage * KZ_MENU_SWATCH + slot;
		if (idx >= 0 && idx < panorama::GetColorEntryCount())
		{
			this->player->optionService->SetPreferenceColor(it->prefKey, panorama::GetColorEntryValue(idx));
			this->Render(); // move the selected-swatch highlight; HUD previews on its own entity
		}
	}
	else if (this->popup == Popup::List)
	{
		const i32 idx = this->popupPage * KZ_MENU_LIST + slot;
		if (idx < 0 || idx >= (i32)this->listChoices.size())
		{
			return;
		}
		const KZChoice &c = this->listChoices[idx];
		if (this->popupFont)
		{
			this->player->optionService->SetPreferenceStr(it->prefKey, PANORAMA_FONTS[c.id].slug);
		}
		else if (it->onPick)
		{
			it->onPick(this->player, it->tag, c.id);
		}
		this->Render(); // move the selected highlight
	}
}

void KZMenuService::Step(bool vertical, i32 delta)
{
	const KZOptItem *it = this->PopupItem();
	if (!it)
	{
		return;
	}
	auto *opts = this->player->optionService;
	if (it->type == KZOptItemType::Position)
	{
		const char *key = vertical ? it->yKey : it->prefKey;
		const i32 def = vertical ? it->iydef : it->idef;
		const i32 value = (i32)opts->GetPreferenceFloat(key, def) + delta;
		opts->SetPreferenceFloat(key, panorama::SnapToStep(value, -100, 100));
	}
	else if (it->type == KZOptItemType::Size)
	{
		const i32 value = (i32)opts->GetPreferenceFloat(it->prefKey, it->idef) + delta;
		opts->SetPreferenceFloat(it->prefKey, panorama::SnapToStep(value, it->lo, it->hi));
	}
	this->Render();
}

// === Click routing ===================================================================

void KZMenuService::OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (!player || !player->menuService->IsOpen())
	{
		return;
	}
	if ((CBaseEntity *)layout != player->menuService->layoutEntity.Get())
	{
		return;
	}
	KZMenuService *menu = player->menuService;

	if (V_strcmp(buttonId, "m_close") == 0)
	{
		menu->Close();
	}
	else if (V_strcmp(buttonId, "color_close") == 0 || V_strcmp(buttonId, "list_close") == 0 || V_strcmp(buttonId, "step_close") == 0)
	{
		menu->ClosePopup();
	}
	else if (V_strcmp(buttonId, "cp_prev") == 0 || V_strcmp(buttonId, "lp_prev") == 0)
	{
		menu->PopupPageStep(-1);
	}
	else if (V_strcmp(buttonId, "cp_next") == 0 || V_strcmp(buttonId, "lp_next") == 0)
	{
		menu->PopupPageStep(1);
	}
	else if (V_strcmp(buttonId, "m_v_n5") == 0)
	{
		menu->Step(true, -5);
	}
	else if (V_strcmp(buttonId, "m_v_n1") == 0)
	{
		menu->Step(true, -1);
	}
	else if (V_strcmp(buttonId, "m_v_p1") == 0)
	{
		menu->Step(true, 1);
	}
	else if (V_strcmp(buttonId, "m_v_p5") == 0)
	{
		menu->Step(true, 5);
	}
	else if (V_strcmp(buttonId, "m_h_n5") == 0)
	{
		menu->Step(false, -5);
	}
	else if (V_strcmp(buttonId, "m_h_n1") == 0)
	{
		menu->Step(false, -1);
	}
	else if (V_strcmp(buttonId, "m_h_p1") == 0)
	{
		menu->Step(false, 1);
	}
	else if (V_strcmp(buttonId, "m_h_p5") == 0)
	{
		menu->Step(false, 5);
	}
	else if (V_strncmp(buttonId, "cat", 3) == 0 && V_isdigit(buttonId[3]))
	{
		menu->SelectLeft(atoi(buttonId + 3));
	}
	else if (V_strncmp(buttonId, "item", 4) == 0 && V_isdigit(buttonId[4]))
	{
		menu->ActivateItem(atoi(buttonId + 4));
	}
	else if (V_strncmp(buttonId, "sw", 2) == 0 && V_isdigit(buttonId[2]))
	{
		menu->PopupPick(atoi(buttonId + 2));
	}
	else if (V_strncmp(buttonId, "li", 2) == 0 && V_isdigit(buttonId[2]))
	{
		menu->PopupPick(atoi(buttonId + 2));
	}
}

// === Public API ======================================================================

void KZMenuService::DropCapture()
{
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	this->popup = Popup::None;
	this->popupItemIndex = -1;
	this->listChoices.clear();
	if (CCSCustomHudLayout *layout = this->MenuLayout())
	{
		this->SetClass(layout, "menu_root", "hidden", true);
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

	CCSCustomHudLayout *layout = this->MenuLayout();
	if (!layout || !layout->GetPlayerLayoutState(slot))
	{
		this->player->languageService->PrintChat(true, false, "Menu - Unavailable");
		return;
	}
	if (KZMenu::Tree().empty())
	{
		return;
	}

	if (g_pMenus)
	{
		g_pMenus->CancelMenu(slot.Get());
		g_pMenus->SetExternalBusy(slot.Get(), true);
	}

	this->open = true;
	this->popup = Popup::None;
	this->selectedCategory = 0;
	this->selectedSub = KZMenu::Tree()[0]->subs.empty() ? -1 : 0;
	layout->SetInputCaptureEnabled(slot, true);
	this->Render();
}

void KZMenuService::Reset()
{
	// Like the HUD service, Reset keeps the owned entity (it survives respawns/map changes and is
	// recreated lazily); only OnClientDisconnect destroys it.
	if (this->open)
	{
		this->open = false;
		this->DropCapture();
	}
	this->applied = Applied();
	this->writtenVars.clear();
}

void KZMenuService::OnClientDisconnect()
{
	this->Reset();
	this->DestroyOwnedLayout();
}

void KZMenuService::Cleanup()
{
	for (i32 i = 0; i < MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(i));
		if (player && player->menuService)
		{
			player->menuService->Reset();
		}
	}
}

void KZMenuService::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
{
	static_persist const int offset = g_pGameConfig->GetOffset("QuietPlayerSlot");
	for (int i = 0; i < infoCount; i++)
	{
		TransmitInfo *info = reinterpret_cast<TransmitInfo *>(pInfo[i]);
		const int recipient = *reinterpret_cast<int *>(reinterpret_cast<uintptr_t>(info) + offset);
		for (int owner = 0; owner < MAXPLAYERS; owner++)
		{
			if (owner == recipient)
			{
				continue;
			}
			KZPlayer *ownerPlayer = g_pKZPlayerManager->ToPlayer(CPlayerSlot(owner));
			if (!ownerPlayer || !ownerPlayer->menuService)
			{
				continue;
			}
			CBaseEntity *ent = ownerPlayer->menuService->layoutEntity.Get();
			if (ent)
			{
				info->m_pTransmitEdict->Clear(ent->entindex());
			}
		}
	}
}

SCMD(kz_options, SCFL_PREFERENCE)
{
	g_pKZPlayerManager->ToPlayer(controller)->menuService->Toggle();
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_o, kz_options);
