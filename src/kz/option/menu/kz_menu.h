#pragma once
#include "kz/kz.h"
#include "kz/option/menu/model.h"

#include <string>
#include <unordered_map>
#include <vector>

class CCSCustomHudLayout;
class CCheckTransmitInfo;

// Fixed slot counts, kept in step with menu.xml.
#define KZ_MENU_CATS   20
#define KZ_MENU_ITEMS  16
#define KZ_MENU_LIST   32 // must cover the largest font family (Stratum2, 29 faces)
#define KZ_MENU_SWATCH 40 // 10 columns x 4 rows per color page

// Renders the KZ::menu model tree into menu.xml on the player's own masked layout entity, and
// routes clicks back to it.
class KZMenuService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static void Init();
	static void RegisterChromePrefs();
	static std::string GetPhrase(KZPlayer *player, const char *key);
	virtual void Reset() override;

	void Toggle();
	void Close();

	bool IsOpen() const
	{
		return this->open;
	}

	static void OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId);
	static void Cleanup();
	// Masks each player's owned entity away from every client but its owner.
	static void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);

	void OnClientDisconnect();

private:
	enum class Popup
	{
		None,
		Color,
		List,
		Step,
	};

	struct LeftEntry
	{
		KZOptNode *node {};
		bool isSub {};
		i32 categoryIndex {};
		i32 subIndex {};
	};

	// Lazily spawned. `created` is set when this call spawned a fresh one, so the caller can rebuild its per-entity caches.
	CCSCustomHudLayout *EnsureMenuLayout(bool &created);
	CCSCustomHudLayout *MenuLayout();
	void DestroyOwnedLayout();
	// Drops cursor capture and releases the cs2menus slot.
	void DropCapture();

	KZOptNode *ActiveNode();
	// Flattens categories plus the selected category's subs into leftSlots; returns the count.
	i32 BuildLeft();

	// Cheap to call repeatedly: writes are diff-cached.
	void Render();
	void RenderChrome(CCSCustomHudLayout *layout);
	void RenderLeft(CCSCustomHudLayout *layout);
	void RenderItems(CCSCustomHudLayout *layout);
	void RenderColorPopup(CCSCustomHudLayout *layout);
	void RenderListPopup(CCSCustomHudLayout *layout);
	void RenderStepPopup(CCSCustomHudLayout *layout);

	void SelectLeft(i32 slot);
	bool IsItemEnabled(const KZOptItem &item);
	void ActivateItem(i32 slot);
	void OpenPopup(Popup kind, i32 itemIdx);
	void ClosePopup();
	void PopupPageStep(i32 delta);
	void PopupPick(i32 slot);
	// axis: 0 = x, 1 = y, 2 = z.
	void Step(i32 axis, i32 delta);

	const KZOptItem *PopupItem();

	// Writes are diff-cached: every write marks the whole entity for a full network resend.
	void SetClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool on);
	void SetBoolClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool &cache, bool want);
	void SetSwapClass(CCSCustomHudLayout *layout, const char *panelId, const char *&cache, const char *want);
	void SetVar(CCSCustomHudLayout *layout, const char *panelId, const char *var, const char *value);

	bool open {};
	i32 selectedCategory {};
	i32 selectedSub {-1};
	Popup popup {Popup::None};
	i32 popupItemIndex {-1};
	bool popupFont {}; // List popup: font faces vs a Choice provider
	i32 popupPage {};

	CHandle<CBaseEntity> layoutEntity {};

	// Snapshot of what is on screen, so a click routes without rebuilding.
	LeftEntry leftSlots[KZ_MENU_CATS] {};
	i32 leftCount {};
	const KZOptItem *itemSlots[KZ_MENU_ITEMS] {};
	i32 itemCount {};
	std::vector<KZChoice> listChoices; // the open list popup's full option list
	// Font picker only: index into listChoices where each family starts. One page per family.
	std::vector<i32> fontPageStart;

	// Last value written for each dialog variable, so an unchanged value is not resent.
	std::unordered_map<std::string, std::string> writtenVars;

	// The classes currently applied on the layout, so a render only writes what changed. Pointer
	// fields hold the class string last applied on that panel, NULL for none.
	struct Applied
	{
		const char *menuFont {};  // menu font class stamped on the text panels
		const char *menuColor {}; // menu color (pal-fg) class stamped on the text panels
		bool rootHidden {true};   // menu_root "hidden"
		bool sounds {};           // menu_root "snd", gating every hover/click sound in menu.css
		bool shift {};            // menu_root "shift", nudging the menu left so an open popup clears a 4:3/5:4 screen edge
		bool colorHidden {true};  // color_popup "hidden"
		bool listHidden {true};   // list_popup "hidden"
		bool stepHidden {true};   // step_popup "hidden"
		bool vstepHidden {true};  // the stepper's vertical rows "hidden" (Position and Vector)
		bool zstepHidden {true};  // the stepper's z row "hidden" (Vector only)
		bool noteHidden {true};   // the list popup's "* is a system font" footnote "hidden"
		// Left column, one slot each:
		bool catHidden[KZ_MENU_CATS] {};   // slot "hidden" (unused)
		bool catSel[KZ_MENU_CATS] {};      // "selected" (active node)
		bool catIndent[KZ_MENU_CATS] {};   // "indent" (a subcategory)
		bool catParent[KZ_MENU_CATS] {};   // "cat-parent" (top-level header styling)
		bool catDisabled[KZ_MENU_CATS] {}; // "disabled" (active category with a sub open)
		// Middle column, one slot each:
		bool itemHidden[KZ_MENU_ITEMS] {};      // slot "hidden" (unused)
		const char *itemType[KZ_MENU_ITEMS] {}; // the "type-*" control class
		bool itemOn[KZ_MENU_ITEMS] {};          // toggle "on"
		bool itemSub[KZ_MENU_ITEMS] {};
		bool itemDisabled[KZ_MENU_ITEMS] {};      // "disabled" (its enabledBy preference is off)           // "has-sub" (subtext line shown)
		bool itemDiv[KZ_MENU_ITEMS] {};           // "divider" (rule under the item, unused)
		const char *itemSwatch[KZ_MENU_ITEMS] {}; // color item's pal-bg swatch class
		// List popup rows, one slot each:
		bool liHidden[KZ_MENU_LIST] {};      // row "hidden"
		bool liSel[KZ_MENU_LIST] {};         // "selected" (current choice)
		const char *liFont[KZ_MENU_LIST] {}; // per-row font class (font picker previews its face)
		// Color popup swatches, one slot each:
		const char *swBg[KZ_MENU_SWATCH] {}; // swatch's pal-bg / gbg class
		bool swSel[KZ_MENU_SWATCH] {};       // "selected" (the item's current color)
		bool swHidden[KZ_MENU_SWATCH] {};    // swatch "hidden" (trailing empty slots on the last page)

		Applied()
		{
			for (i32 i = 0; i < KZ_MENU_CATS; i++)
			{
				catHidden[i] = true;
			}
			for (i32 i = 0; i < KZ_MENU_ITEMS; i++)
			{
				itemHidden[i] = true;
			}
			for (i32 i = 0; i < KZ_MENU_LIST; i++)
			{
				liHidden[i] = true;
			}
		}
	} applied;
};
