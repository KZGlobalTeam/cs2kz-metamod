#include "cs2kz.h"
#include "kz/option/menu/kz_menu.h"
#include "kz/option/menu/model.h"
#include "kz/option/menu/tables.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"

#include "tier0/memdbgon.h"

// Color pages derive from the picker entry count (solids + gradients) and the per-page swatch count.
static_function i32 GetItemColorCount(const KZOptItem *item)
{
	return item && item->solidOnly ? panorama::GetSolidColorCount() : panorama::GetColorEntryCount();
}

static_function i32 GetColorPageCount(const KZOptItem *item)
{
	return MAX(1, (GetItemColorCount(item) + KZ_MENU_SWATCH - 1) / KZ_MENU_SWATCH);
}

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
SLOT_ID(OrPanel, "or%i")
SLOT_ID(OrLbl, "or_lbl%i")
SLOT_ID(OrVar, "ol%i")
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
		case KZOptItemType::Order:
			return "type-order";
	}
	return "type-button";
}

// A Size item backs either an int or a float preference, so both ends go through these.
static_function i32 GetSizeValue(KZOptionService *opts, const KZOptItem &item)
{
	if (item.storage == KZOptStorage::Int)
	{
		return (i32)opts->GetPreferenceInt(item.prefKey, item.idef);
	}
	const i32 scale = MAX(1, item.scale);
	return (i32)(opts->GetPreferenceFloat(item.prefKey, (f64)item.idef / scale) * scale + 0.5);
}

static_function void SetSizeValue(KZOptionService *opts, const KZOptItem &item, i32 value)
{
	if (item.storage == KZOptStorage::Int)
	{
		opts->SetPreferenceInt(item.prefKey, value);
	}
	else
	{
		opts->SetPreferenceFloat(item.prefKey, (f64)value / MAX(1, item.scale));
	}
}

// A dragged position keeps a tenth of a percent; whole values print without the decimal.
static_function void FormatPosition(char *out, i32 outLen, f64 x, f64 y)
{
	V_snprintf(out, outLen, "%g%%, %g%%", RoundFloatToInt((f32)x * 10.0f) / 10.0f, RoundFloatToInt((f32)y * 10.0f) / 10.0f);
}

std::string KZMenuService::GetPhrase(KZPlayer *player, const char *key)
{
	return player->languageService->PrepareMessage(key);
}

// === Owned entity ====================================================================

CCSCustomHudLayout *KZMenuService::EnsureLayout()
{
	if (!KZHUDService::IsLayoutHudAvailable())
	{
		return NULL;
	}
	bool created = false;
	CCSCustomHudLayout *layout = this->menuLayout.Ensure(created);
	if (created)
	{
		this->applied = Applied();
	}
	return layout;
}

// === Registration ====================================================================

void KZMenuService::Init()
{
	KZMenuService::RegisterChromePrefs();
}

// === Model navigation ================================================================

KZOptNode *KZMenuService::ActiveNode()
{
	const std::vector<KZOptNode *> &tree = KZ::menu::GetTree();
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
	const std::vector<KZOptNode *> &tree = KZ::menu::GetTree();
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
	// Only hand back an item whose type matches the open popup, so a stale index cannot feed a
	// mismatched item into rendering.
	const KZOptItem &it = node->items[this->popupItemIndex];
	const bool ok =
		(this->popup == Popup::Color && it.type == KZOptItemType::Color)
		|| (this->popup == Popup::List && (it.type == KZOptItemType::Font || it.type == KZOptItemType::Choice))
		|| (this->popup == Popup::Step && (it.type == KZOptItemType::Position || it.type == KZOptItemType::Size || it.type == KZOptItemType::Vector))
		|| (this->popup == Popup::Order && it.type == KZOptItemType::Order);
	return ok ? &it : NULL;
}

// === Rendering =======================================================================

void KZMenuService::Render()
{
	CCSCustomHudLayout *layout = this->EnsureLayout();
	if (!layout)
	{
		return;
	}

	KZ::ui::SetBoolClass(layout, "menu_root", "hidden", this->applied.rootHidden, !this->shown);
	if (!this->shown)
	{
		return;
	}

	// Nudge the whole menu left while one is open for 4:3 aspect ratio.
	const bool shift = this->popup != Popup::None && this->player->optionService->GetPreferenceBool("menuPopupShift", true);
	KZ::ui::SetBoolClass(layout, "menu_root", "shift", this->applied.shift, shift);

	// remove world-blur from the menu and popups when anything hud related is being adjusted so the player can see
	const std::vector<KZOptNode *> &tree = KZ::menu::GetTree();
	const bool noBlur = this->popup != Popup::None && this->selectedCategory >= 0 && this->selectedCategory < (i32)tree.size()
						&& KZ_STREQ(tree[this->selectedCategory]->phraseKey, "Menu - HUD");
	if (this->applied.noBlur != noBlur)
	{
		this->applied.noBlur = noBlur;
		for (const char *panelId : {"menu_box", "color_popup", "list_popup", "step_popup", "order_popup"})
		{
			layout->SetHasClass(panelId, "no-blur", noBlur ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
		}
	}

	// The box stays put and centred; a popup is a third panel that appears to its right.
	KZ::ui::SetBoolClass(layout, "color_popup", "hidden", this->applied.colorHidden, this->popup != Popup::Color);
	KZ::ui::SetBoolClass(layout, "list_popup", "hidden", this->applied.listHidden, this->popup != Popup::List);
	KZ::ui::SetBoolClass(layout, "step_popup", "hidden", this->applied.stepHidden, this->popup != Popup::Step);
	KZ::ui::SetBoolClass(layout, "order_popup", "hidden", this->applied.orderHidden, this->popup != Popup::Order);

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
	else if (this->popup == Popup::Order)
	{
		this->RenderOrderPopup(layout);
	}
}

void KZMenuService::RenderChrome(CCSCustomHudLayout *layout)
{
	KZ::ui::ApplyWindowStyle(this->player, layout, "menu_root", this->applied.style);
	this->menuLayout.SetVar("menu_title", "title", KZMenuService::GetPhrase(this->player, "Menu - Title Options").c_str());
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
			// Every top-level category is styled as a header, and one that owns subs is inert once a sub
			// of it is active.
			const bool isParent = !e.isSub;
			const bool disabled = isParent && !e.node->subs.empty() && e.categoryIndex == this->selectedCategory;
			this->menuLayout.SetVar(CatLbl(i), CatVar(i), KZMenuService::GetPhrase(this->player, e.node->phraseKey).c_str());
			KZ::ui::SetBoolClass(layout, CatPanel(i), "indent", this->applied.catIndent[i], e.isSub);
			KZ::ui::SetBoolClass(layout, CatPanel(i), "cat-parent", this->applied.catParent[i], isParent);
			KZ::ui::SetBoolClass(layout, CatPanel(i), "disabled", this->applied.catDisabled[i], disabled);
			KZ::ui::SetBoolClass(layout, CatPanel(i), "selected", this->applied.catSel[i], e.node == active);
		}
		KZ::ui::SetBoolClass(layout, CatPanel(i), "hidden", this->applied.catHidden[i], !used);
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
			this->menuLayout.SetVar(ItemLbl(i), ItemLblVar(i), KZMenuService::GetPhrase(this->player, it.phraseKey).c_str());

			std::string value;
			const char *swatch = NULL;
			bool on = false;
			switch (it.type)
			{
				case KZOptItemType::Toggle:
					on = it.getCurrent ? it.getCurrent(this->player, it.tag) != 0 : opts->GetPreferenceBool(it.prefKey, it.idef != 0);
					value = KZMenuService::GetPhrase(this->player, on ? "Menu - On" : "Menu - Off");
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
					FormatPosition(buf, sizeof(buf), opts->GetPreferenceFloat(it.prefKey, it.idef), opts->GetPreferenceFloat(it.yKey, it.iydef));
					value = buf;
					break;
				}
				case KZOptItemType::Size:
				{
					char buf[16];
					V_snprintf(buf, sizeof(buf), "%i%s", GetSizeValue(opts, it), it.unit ? it.unit : "");
					value = buf;
					break;
				}
				case KZOptItemType::Vector:
				{
					const Vector v = opts->GetPreferenceVector(it.prefKey, Vector((f32)it.idef, (f32)it.iydef, (f32)it.izdef));
					char buf[32];
					V_snprintf(buf, sizeof(buf), "%i, %i, %i", (i32)v.x, (i32)v.y, (i32)v.z);
					value = buf;
					break;
				}
				case KZOptItemType::Button:
				case KZOptItemType::Order:
					break;
				case KZOptItemType::Choice:
				{
					if (it.getChoices)
					{
						std::vector<KZChoice> choices;
						it.getChoices(this->player, it.tag, choices);
						if (it.getCurrent)
						{
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
						else
						{
							// A multi-select list has no single current row, so the value lists what is on.
							for (const KZChoice &c : choices)
							{
								if (c.selected)
								{
									value += value.empty() ? c.label : ", " + c.label;
								}
							}
						}
					}
					break;
				}
			}

			this->menuLayout.SetVar(ItemVal(i), ItemValVar(i), value.c_str());
			this->menuLayout.SetVar(ItemSub(i), ItemSubVar(i), it.subKey ? KZMenuService::GetPhrase(this->player, it.subKey).c_str() : "");
			KZ::ui::SetSwapClass(layout, ItemSw(i), this->applied.itemSwatch[i], swatch);
			KZ::ui::SetSwapClass(layout, ItemPanel(i), this->applied.itemType[i], GetTypeClass(it.type));
			KZ::ui::SetBoolClass(layout, ItemPanel(i), "on", this->applied.itemOn[i], on);
			KZ::ui::SetBoolClass(layout, ItemPanel(i), "has-sub", this->applied.itemSub[i], it.subKey != NULL);
			KZ::ui::SetBoolClass(layout, ItemPanel(i), "divider", this->applied.itemDiv[i], it.dividerAfter);
			KZ::ui::SetBoolClass(layout, ItemPanel(i), "disabled", this->applied.itemDisabled[i], !this->IsItemEnabled(it));
		}
		KZ::ui::SetBoolClass(layout, ItemPanel(i), "hidden", this->applied.itemHidden[i], !used);
	}
}

void KZMenuService::RenderColorPopup(CCSCustomHudLayout *layout)
{
	// Highlight the entry matching the item's current color, if it is on this page.
	const KZOptItem *it = this->PopupItem();
	i32 curIdx = -1;
	if (it)
	{
		curIdx = panorama::FindColorEntry(this->player->optionService->GetPreferenceColor(it->prefKey, it->cdef));
	}
	const i32 total = GetItemColorCount(it);
	for (i32 i = 0; i < KZ_MENU_SWATCH; i++)
	{
		const i32 idx = this->popupPage * KZ_MENU_SWATCH + i;
		const bool used = idx < total;
		if (used)
		{
			KZ::ui::SetSwapClass(layout, SwPanel(i), this->applied.swBg[i], panorama::GetColorEntryBgClass(idx));
			KZ::ui::SetBoolClass(layout, SwPanel(i), "selected", this->applied.swSel[i], idx == curIdx);
		}
		KZ::ui::SetBoolClass(layout, SwPanel(i), "hidden", this->applied.swHidden[i], !used);
	}
	char page[16];
	V_snprintf(page, sizeof(page), "%i/%i", this->popupPage + 1, GetColorPageCount(it));
	this->menuLayout.SetVar("cp_page", "cppage", page);
}

void KZMenuService::RenderListPopup(CCSCustomHudLayout *layout)
{
	const i32 n = (i32)this->listChoices.size();
	// The font picker pages by family; a choice list pages by slot count.
	const i32 pages = this->popupFont ? MAX(1, (i32)this->fontPageStart.size()) : MAX(1, (n + KZ_MENU_LIST - 1) / KZ_MENU_LIST);
	this->popupPage = Clamp(this->popupPage, 0, pages - 1);
	i32 first = 0;
	i32 count = 0;
	if (this->popupFont)
	{
		first = this->fontPageStart[this->popupPage];
		count = (this->popupPage + 1 < pages ? this->fontPageStart[this->popupPage + 1] : n) - first;
	}
	else
	{
		first = this->popupPage * KZ_MENU_LIST;
		count = n - first;
	}
	count = Clamp(count, 0, KZ_MENU_LIST);

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

	for (i32 i = 0; i < KZ_MENU_LIST; i++)
	{
		const bool used = i < count;
		if (used)
		{
			const KZChoice &c = this->listChoices[first + i];
			this->menuLayout.SetVar(LiLbl(i), LiVar(i), c.label.c_str());
			KZ::ui::SetBoolClass(layout, LiPanel(i), "selected", this->applied.liSel[i], c.selected || c.id == curId);
			// Font rows preview their own face; a choice row inherits the menu font from the root.
			const char *face = this->popupFont ? PANORAMA_FONTS[c.id].className : NULL;
			KZ::ui::SetSwapClass(layout, LiLbl(i), this->applied.liFont[i], face);
		}
		KZ::ui::SetBoolClass(layout, LiPanel(i), "hidden", this->applied.liHidden[i], !used);
	}
	// The header names the family being browsed, or the item for a plain choice list.
	const char *title = "";
	if (this->popupFont && count > 0)
	{
		title = PANORAMA_FONTS[this->listChoices[first].id].family;
		this->menuLayout.SetVar("lp_title", "lptitle", title);
	}
	else if (it)
	{
		this->menuLayout.SetVar("lp_title", "lptitle", KZMenuService::GetPhrase(this->player, it->phraseKey).c_str());
	}
	char page[16];
	V_snprintf(page, sizeof(page), "%i/%i", this->popupPage + 1, pages);
	this->menuLayout.SetVar("lp_page", "lppage", page);
	// Only the font list carries the * marker, so only it needs the footnote.
	KZ::ui::SetBoolClass(layout, "lp_note", "hidden", this->applied.noteHidden, !this->popupFont);
	if (this->popupFont)
	{
		this->menuLayout.SetVar("lp_note", "lpnote", KZMenuService::GetPhrase(this->player, "Menu - Font Local Note").c_str());
	}
}

void KZMenuService::RenderStepPopup(CCSCustomHudLayout *layout)
{
	const KZOptItem *it = this->PopupItem();
	if (!it)
	{
		return;
	}
	const bool zstep = it->type == KZOptItemType::Vector;
	const bool vstep = it->type == KZOptItemType::Position || zstep;
	if (this->applied.vstepHidden != !vstep)
	{
		this->applied.vstepHidden = !vstep;
		KZ::ui::SetClass(layout, "m_step_up", "hidden", !vstep);
		KZ::ui::SetClass(layout, "m_step_down", "hidden", !vstep);
	}
	if (this->applied.zstepHidden != !zstep)
	{
		this->applied.zstepHidden = !zstep;
		KZ::ui::SetClass(layout, "m_step_z", "hidden", !zstep);
	}
	auto *opts = this->player->optionService;
	char readout[32];
	if (zstep)
	{
		const Vector v = opts->GetPreferenceVector(it->prefKey, Vector((f32)it->idef, (f32)it->iydef, (f32)it->izdef));
		V_snprintf(readout, sizeof(readout), "%i, %i, %i", (i32)v.x, (i32)v.y, (i32)v.z);
	}
	else if (vstep)
	{
		FormatPosition(readout, sizeof(readout), opts->GetPreferenceFloat(it->prefKey, it->idef), opts->GetPreferenceFloat(it->yKey, it->iydef));
	}
	else
	{
		V_snprintf(readout, sizeof(readout), "%i%s", GetSizeValue(opts, *it), it->unit ? it->unit : "");
	}
	this->menuLayout.SetVar("step_readout", "step", readout);
	this->menuLayout.SetVar("step_readout_top", "steptop", readout);
	KZ::ui::SetBoolClass(layout, "step_popup", "fine", this->applied.stepFine, it->type == KZOptItemType::Position);
	KZ::ui::SetBoolClass(layout, "m_step_drag", "hidden", this->applied.stepDragHidden, !it->onInteract);
	if (it->onInteract)
	{
		this->menuLayout.SetVar("m_step_drag_label", "stepdrag", KZMenuService::GetPhrase(this->player, "Menu - Move With Mouse").c_str());
	}
	this->menuLayout.SetVar("step_label", "steplabel", KZMenuService::GetPhrase(this->player, it->phraseKey).c_str());
}

void KZMenuService::RenderOrderPopup(CCSCustomHudLayout *layout)
{
	const KZOptItem *it = this->PopupItem();
	if (!it)
	{
		return;
	}
	const i32 count = MIN((i32)this->listChoices.size(), KZ_MENU_ORDER);
	for (i32 i = 0; i < KZ_MENU_ORDER; i++)
	{
		const bool used = i < count;
		if (used)
		{
			this->menuLayout.SetVar(OrLbl(i), OrVar(i), this->listChoices[i].label.c_str());
			KZ::ui::SetBoolClass(layout, OrPanel(i), "first", this->applied.orFirst[i], i == 0);
			KZ::ui::SetBoolClass(layout, OrPanel(i), "last", this->applied.orLast[i], i == count - 1);
		}
		KZ::ui::SetBoolClass(layout, OrPanel(i), "hidden", this->applied.orHidden[i], !used);
	}
	this->menuLayout.SetVar("op_title", "optitle", KZMenuService::GetPhrase(this->player, it->phraseKey).c_str());
}

// === Interaction =====================================================================

void KZMenuService::SelectLeft(i32 slot)
{
	if (slot < 0 || slot >= this->leftCount)
	{
		return;
	}
	// The panes stay live while a picker is open, so close it first: it must never be left pointing
	// at an item the node change removed.
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
		KZOptNode *cat = KZ::menu::GetTree()[this->selectedCategory];
		this->selectedSub = cat->subs.empty() ? -1 : 0;
	}
	this->Render();
}

bool KZMenuService::IsItemEnabled(const KZOptItem &item)
{
	for (i32 i = 0; i < KZ_ARRAYSIZE(item.enabledBy); i++)
	{
		if (item.enabledBy[i] && !this->player->optionService->GetPreferenceBool(item.enabledBy[i], item.enabledByDef[i]))
		{
			return false;
		}
	}
	return true;
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
	if (!this->IsItemEnabled(it))
	{
		return;
	}
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
		case KZOptItemType::Vector:
			this->OpenPopup(Popup::Step, slot);
			break;
		case KZOptItemType::Order:
			this->OpenPopup(Popup::Order, slot);
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
		this->fontPageStart.clear();
		if (this->popupFont)
		{
			// The table is already ordered by family, so a single pass finds the page boundaries.
			const char *family = NULL;
			for (i32 f = 0; f < PANORAMA_FONT_COUNT; f++)
			{
				const PanoramaFontDef &def = PANORAMA_FONTS[f];
				if (!family || V_strcmp(family, def.family) != 0)
				{
					family = def.family;
					this->fontPageStart.push_back((i32)this->listChoices.size());
				}
				KZChoice face;
				face.label = def.variant;
				face.id = f;
				this->listChoices.push_back(face);
			}
			// Open on the family the player is already using rather than always at the first.
			const char *slug = panorama::ResolveFontSlug(this->player->optionService->GetPreferenceStr(it.prefKey, it.sdef), it.sdef);
			for (i32 f = 0; f < PANORAMA_FONT_COUNT; f++)
			{
				if (V_strcmp(slug, PANORAMA_FONTS[f].slug) != 0)
				{
					continue;
				}
				for (i32 p = (i32)this->fontPageStart.size() - 1; p >= 0; p--)
				{
					if (this->fontPageStart[p] <= f)
					{
						this->popupPage = p;
						break;
					}
				}
				break;
			}
		}
		else if (it.getChoices)
		{
			it.getChoices(this->player, it.tag, this->listChoices);
		}
	}
	else if (kind == Popup::Order)
	{
		this->listChoices.clear();
		if (it.getChoices)
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
		pages = GetColorPageCount(this->PopupItem());
	}
	else if (this->popup == Popup::List)
	{
		// One page per font family; a plain choice list pages by slot count.
		pages = this->popupFont ? MAX(1, (i32)this->fontPageStart.size()) : MAX(1, ((i32)this->listChoices.size() + KZ_MENU_LIST - 1) / KZ_MENU_LIST);
	}
	this->popupPage = Clamp(this->popupPage + delta, 0, pages - 1);
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
		if (idx >= 0 && idx < GetItemColorCount(it))
		{
			this->player->optionService->SetPreferenceColor(it->prefKey, panorama::GetColorEntryValue(idx));
			this->Render(); // move the selected-swatch highlight; HUD previews on its own entity
		}
	}
	else if (this->popup == Popup::List)
	{
		const i32 n = (i32)this->listChoices.size();
		const i32 pages = (i32)this->fontPageStart.size();
		const bool byFamily = this->popupFont && this->popupPage < pages;
		const i32 first = byFamily ? this->fontPageStart[this->popupPage] : this->popupPage * KZ_MENU_LIST;
		// A click must stay inside this page, or it would spill into the next family.
		const i32 end = byFamily ? (this->popupPage + 1 < pages ? this->fontPageStart[this->popupPage + 1] : n) : MIN(first + KZ_MENU_LIST, n);
		const i32 idx = first + slot;
		if (idx < 0 || idx >= end)
		{
			return;
		}
		const i64 choiceId = this->listChoices[idx].id;
		if (this->popupFont)
		{
			this->player->optionService->SetPreferenceStr(it->prefKey, PANORAMA_FONTS[choiceId].slug);
		}
		else if (it->onPick)
		{
			it->onPick(this->player, it->tag, choiceId);
			// A multi-select list keeps each row's state in the row, so rebuild for the highlight to
			// follow the pick. A single-select one re-reads getCurrent every render.
			this->listChoices.clear();
			if (it->getChoices)
			{
				it->getChoices(this->player, it->tag, this->listChoices);
			}
		}
		this->Render(); // move the selected highlight
	}
}

void KZMenuService::Step(i32 axis, f32 delta)
{
	const KZOptItem *it = this->PopupItem();
	if (!it)
	{
		return;
	}
	auto *opts = this->player->optionService;
	if (it->type == KZOptItemType::Position)
	{
		const char *key = axis == 1 ? it->yKey : it->prefKey;
		const i32 def = axis == 1 ? it->iydef : it->idef;
		// Dragging stores tenths of a percent, so step without truncating them.
		const f32 value = Clamp((f32)opts->GetPreferenceFloat(key, def) + delta, (f32)it->lo, (f32)it->hi);
		opts->SetPreferenceFloat(key, RoundFloatToInt(value * 10.0f) / 10.0f);
	}
	else if (it->type == KZOptItemType::Vector)
	{
		Vector value = opts->GetPreferenceVector(it->prefKey, Vector((f32)it->idef, (f32)it->iydef, (f32)it->izdef));
		f32 &component = axis == 2 ? value.z : (axis == 1 ? value.y : value.x);
		component = (f32)panorama::SnapToStep((i32)component + (i32)delta, it->lo, it->hi);
		opts->SetPreferenceVector(it->prefKey, value);
	}
	else if (it->type == KZOptItemType::Size)
	{
		SetSizeValue(opts, *it, panorama::SnapToStep(GetSizeValue(opts, *it) + (i32)delta, it->lo, it->hi));
	}
	this->Render();
}

void KZMenuService::InteractPopupItem()
{
	const KZOptItem *it = this->PopupItem();
	if (it && it->onInteract)
	{
		it->onInteract(this->player, it->tag);
	}
}

void KZMenuService::MoveOrderRow(i32 slot, i32 delta)
{
	const KZOptItem *it = this->PopupItem();
	if (!it || !it->onMove || slot < 0 || slot >= MIN((i32)this->listChoices.size(), KZ_MENU_ORDER))
	{
		return;
	}
	it->onMove(this->player, it->tag, this->listChoices[slot].id, delta);
	this->listChoices.clear();
	if (it->getChoices)
	{
		it->getChoices(this->player, it->tag, this->listChoices);
	}
	this->Render();
}

// === Click routing ===================================================================

void KZMenuService::OnClick(const char *buttonId)
{
	if (V_strcmp(buttonId, "m_close") == 0)
	{
		this->Close();
	}
	else if (V_strcmp(buttonId, "color_close") == 0 || V_strcmp(buttonId, "list_close") == 0 || V_strcmp(buttonId, "step_close") == 0
			 || V_strcmp(buttonId, "order_close") == 0)
	{
		this->ClosePopup();
	}
	else if (V_strcmp(buttonId, "cp_prev") == 0 || V_strcmp(buttonId, "lp_prev") == 0)
	{
		this->PopupPageStep(-1);
	}
	else if (V_strcmp(buttonId, "cp_next") == 0 || V_strcmp(buttonId, "lp_next") == 0)
	{
		this->PopupPageStep(1);
	}
	else if (V_strcmp(buttonId, "m_v_n01") == 0)
	{
		this->Step(1, -0.1f);
	}
	else if (V_strcmp(buttonId, "m_v_p01") == 0)
	{
		this->Step(1, 0.1f);
	}
	else if (V_strcmp(buttonId, "m_h_n01") == 0)
	{
		this->Step(0, -0.1f);
	}
	else if (V_strcmp(buttonId, "m_h_p01") == 0)
	{
		this->Step(0, 0.1f);
	}
	else if (V_strcmp(buttonId, "m_step_drag") == 0)
	{
		this->InteractPopupItem();
	}
	else if (V_strcmp(buttonId, "m_v_n5") == 0)
	{
		this->Step(1, -5);
	}
	else if (V_strcmp(buttonId, "m_v_n1") == 0)
	{
		this->Step(1, -1);
	}
	else if (V_strcmp(buttonId, "m_v_p1") == 0)
	{
		this->Step(1, 1);
	}
	else if (V_strcmp(buttonId, "m_v_p5") == 0)
	{
		this->Step(1, 5);
	}
	else if (V_strcmp(buttonId, "m_h_n5") == 0)
	{
		this->Step(0, -5);
	}
	else if (V_strcmp(buttonId, "m_h_n1") == 0)
	{
		this->Step(0, -1);
	}
	else if (V_strcmp(buttonId, "m_h_p1") == 0)
	{
		this->Step(0, 1);
	}
	else if (V_strcmp(buttonId, "m_h_p5") == 0)
	{
		this->Step(0, 5);
	}
	else if (V_strcmp(buttonId, "m_z_n5") == 0)
	{
		this->Step(2, -5);
	}
	else if (V_strcmp(buttonId, "m_z_n1") == 0)
	{
		this->Step(2, -1);
	}
	else if (V_strcmp(buttonId, "m_z_p1") == 0)
	{
		this->Step(2, 1);
	}
	else if (V_strcmp(buttonId, "m_z_p5") == 0)
	{
		this->Step(2, 5);
	}
	else if (V_strncmp(buttonId, "cat", 3) == 0 && V_isdigit(buttonId[3]))
	{
		this->SelectLeft(atoi(buttonId + 3));
	}
	else if (V_strncmp(buttonId, "item", 4) == 0 && V_isdigit(buttonId[4]))
	{
		this->ActivateItem(atoi(buttonId + 4));
	}
	else if (V_strncmp(buttonId, "sw", 2) == 0 && V_isdigit(buttonId[2]))
	{
		this->PopupPick(atoi(buttonId + 2));
	}
	else if (V_strncmp(buttonId, "li", 2) == 0 && V_isdigit(buttonId[2]))
	{
		this->PopupPick(atoi(buttonId + 2));
	}
	else if (V_strncmp(buttonId, "or_up", 5) == 0 && V_isdigit(buttonId[5]))
	{
		this->MoveOrderRow(atoi(buttonId + 5), -1);
	}
	else if (V_strncmp(buttonId, "or_dn", 5) == 0 && V_isdigit(buttonId[5]))
	{
		this->MoveOrderRow(atoi(buttonId + 5), 1);
	}
}

// === Public API ======================================================================

void KZMenuService::Hide()
{
	this->shown = false;
	// The already-spawned entity, not EnsureLayout(): that one hands nothing back while the plugin is unloading.
	if (CCSCustomHudLayout *layout = this->menuLayout.Get())
	{
		KZ::ui::SetClass(layout, "menu_root", "hidden", true);
		this->applied.rootHidden = true;
		// Undo the popup shift here too, or the next open animates the whole menu back from the left.
		KZ::ui::SetClass(layout, "menu_root", "shift", false);
		this->applied.shift = false;
	}
}

bool KZMenuService::CanOpen()
{
	CCSCustomHudLayout *layout = this->EnsureLayout();
	if (!layout || !layout->GetPlayerLayoutState(this->player->GetPlayerSlot()))
	{
		this->player->languageService->PrintChat(true, false, "Menu - Unavailable");
		return false;
	}
	return !KZ::menu::GetTree().empty();
}

void KZMenuService::OnOpen()
{
	this->shown = true;
	this->popup = Popup::None;
	this->selectedCategory = 0;
	this->selectedSub = KZ::menu::GetTree()[0]->subs.empty() ? -1 : 0;
	this->Render();
}

void KZMenuService::OnClose(KZ::ui::CloseReason reason)
{
	this->popup = Popup::None;
	this->popupItemIndex = -1;
	this->listChoices.clear();
	this->Hide();
}

void KZMenuService::OnSuspend()
{
	this->Hide();
}

void KZMenuService::OnResume()
{
	this->shown = true;
	this->Render();
}

void KZMenuService::Close()
{
	this->player->uiService->Close(this);
}

void KZMenuService::Toggle()
{
	this->player->uiService->Toggle(this);
}

bool KZMenuService::IsOpen() const
{
	return this->player->uiService->IsOpen(this);
}

void KZMenuService::Reset()
{
	this->shown = false;
	this->applied = Applied();
	this->menuLayout.ClearVarCache();
}

void KZMenuService::OnClientDisconnect()
{
	this->Reset();
	this->menuLayout.Destroy();
}

SCMD(kz_options, SCFL_PREFERENCE)
{
	g_pKZPlayerManager->ToPlayer(controller)->menuService->Toggle();
	return true;
}

SCMD_LINK(kz_o, kz_options);
