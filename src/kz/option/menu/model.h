#pragma once
#include "kz/kz.h"

#include <string>
#include <vector>

// The options-menu model: a registry of categories -> optional subcategories -> typed items that
// services build at Init with the KZMenu::Add* helpers. The renderer (KZMenuService) walks this tree;
// it owns no knowledge of any particular service. Lives beside the option service because it is the
// preference tree.

enum class KZOptItemType
{
	Toggle,   // bool pref, flipped inline
	Color,    // palette index, opens the swatch popup
	Font,     // font slug pref, opens the list popup (rows preview their own face)
	Position, // two int percent prefs (prefKey = x, yKey = y), opens the 2-axis stepper popup
	Size,     // one int px pref, opens the 1-axis stepper popup
	Button,   // no value, click runs onActivate
	Choice,   // arbitrary runtime option list, opens the list popup
};

// One runtime option of a Choice item. id is opaque - the item's callbacks give it meaning.
struct KZChoice
{
	std::string label;
	i64 id {};
	const char *colorClass {}; // NULL: the menu color
};

// tag is opaque context passed to every callback so one free function can serve many items (e.g. the
// MHUDElement a per-element position/size/color/font item belongs to).
struct KZOptItem
{
	const char *phraseKey {};
	const char *subKey {}; // optional second line under the label
	bool dividerAfter {};  // draw a horizontal rule under this item
	KZOptItemType type {};
	const char *prefKey {};          // Toggle/Color/Font/Size; Position uses prefKey (x) + yKey
	const char *yKey {};             // Position only
	i32 lo {};                       // Size/Position range low (also Toggle unused)
	i32 hi {};                       // Size/Position range high
	i32 idef {};                     // Size/Toggle default; Position x default
	i32 iydef {};                    // Position y default
	Color cdef {255, 255, 255, 255}; // Color default
	const char *sdef {};             // Font default slug
	i64 tag {};                      // opaque, handed to the callbacks below

	// Choice: the option list is rebuilt from getChoices each time the popup opens, so items whose
	// choices are only known at runtime need no static declaration.
	void (*getChoices)(KZPlayer *, i64 tag, std::vector<KZChoice> &) {};
	i64 (*getCurrent)(KZPlayer *, i64 tag) {};
	void (*onPick)(KZPlayer *, i64 tag, i64 id) {};

	void (*onActivate)(KZPlayer *, i64 tag) {}; // Button
	// Called when a popup for this item opens (begin) and closes (begin=false). HUD items use it to
	// force their element on screen while it is being edited.
	void (*onEdit)(KZPlayer *, i64 tag, bool begin) {};
};

struct KZOptNode
{
	const char *phraseKey {};
	std::vector<KZOptItem> items;
	std::vector<KZOptNode *> subs; // heap nodes: pointers stay valid as the registry grows
};

namespace KZMenu
{
	// Registration (call from a service's Init). Returns heap nodes with stable addresses.
	KZOptNode *AddCategory(const char *phraseKey);
	KZOptNode *AddSub(KZOptNode *parent, const char *phraseKey);

	void AddToggle(KZOptNode *node, const char *phraseKey, const char *prefKey, bool def, i64 tag = 0);
	// A toggle whose state and flip run through callbacks instead of a raw pref (service side effects).
	void AddActionToggle(KZOptNode *node, const char *phraseKey, i64 (*getCurrent)(KZPlayer *, i64), void (*onActivate)(KZPlayer *, i64),
						 i64 tag = 0);
	void AddColor(KZOptNode *node, const char *phraseKey, const char *prefKey, const Color &def, i64 tag = 0,
				  void (*onEdit)(KZPlayer *, i64, bool) = nullptr);
	void AddFont(KZOptNode *node, const char *phraseKey, const char *prefKey, const char *def, i64 tag = 0,
				 void (*onEdit)(KZPlayer *, i64, bool) = nullptr);
	void AddPosition(KZOptNode *node, const char *phraseKey, const char *xKey, const char *yKey, i32 xDef, i32 yDef, i64 tag = 0,
					 void (*onEdit)(KZPlayer *, i64, bool) = nullptr);
	void AddSize(KZOptNode *node, const char *phraseKey, const char *prefKey, i32 def, i32 lo, i32 hi, i64 tag = 0,
				 void (*onEdit)(KZPlayer *, i64, bool) = nullptr);
	void AddButton(KZOptNode *node, const char *phraseKey, void (*onActivate)(KZPlayer *, i64), i64 tag = 0);
	void AddChoice(KZOptNode *node, const char *phraseKey, void (*getChoices)(KZPlayer *, i64, std::vector<KZChoice> &),
				   i64 (*getCurrent)(KZPlayer *, i64), void (*onPick)(KZPlayer *, i64, i64), i64 tag = 0);

	// Optional decorations for the item added most recently to `node`.
	void SetItemSubtext(KZOptNode *node, const char *phraseKey);
	void SetItemDivider(KZOptNode *node);

	// Write every value item in this node back to the default it was registered with. Items with no
	// preference behind them - buttons, action toggles, choices - are skipped: their state lives in a
	// service. Keeps reset buttons from drifting away from the declared defaults.
	void ResetNode(KZPlayer *player, KZOptNode *node);

	// The registered categories, in registration order.
	const std::vector<KZOptNode *> &Tree();
} // namespace KZMenu
