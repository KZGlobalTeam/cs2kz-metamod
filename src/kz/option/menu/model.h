#pragma once
#include "kz/kz.h"

#include <string>
#include <vector>

// A registry of categories -> optional subcategories -> typed items, built by services at Init with
// the KZ::menu::Add* helpers and walked by KZMenuService.

enum class KZOptItemType
{
	Toggle,   // bool pref, flipped inline
	Color,    // palette index, opens the swatch popup
	Font,     // font slug pref, opens the list popup (rows preview their own face)
	Position, // two int percent prefs (prefKey = x, yKey = y), opens the 2-axis stepper popup
	Size,     // one int px pref, opens the 1-axis stepper popup
	Vector,   // one vector pref, opens the 3-axis stepper popup
	Button,   // no value, click runs onActivate
	Choice,   // arbitrary runtime option list, opens the list popup
};

// Which typed accessor a preference is stored through, for the generic export/import path.
enum class KZOptStorage
{
	None = 0,
	Bool,
	Int,
	Float,
	Str,
	Vector,
};

struct KZChoice
{
	std::string label;
	i64 id {};
	const char *colorClass {}; // NULL: the menu color
	bool selected {};          // for a list where more than one row can be on at once
};

// tag is opaque context passed to every callback, so one free function can serve many items.
struct KZOptItem
{
	const char *phraseKey {};
	const char *subKey {}; // optional second line under the label
	// Greyed out and unclickable while any of these bool preferences is off. Two is enough for the
	// HUD, where an element's own toggle gates the whole page and a row can gate itself on top.
	const char *enabledBy[2] {};
	bool dividerAfter {}; // draw a horizontal rule under this item
	bool solidOnly {};    // Color: hide the gradients, for a consumer that cannot render one
	KZOptItemType type {};
	KZOptStorage storage {};         // how prefKey (and yKey) are stored; None: nothing to export
	const char *prefKey {};          // Toggle/Color/Font/Size; Position uses prefKey (x) + yKey
	const char *yKey {};             // Position only
	i32 lo {};                       // Size/Position range low (also Toggle unused)
	i32 hi {};                       // Size/Position range high
	i32 idef {};                     // Size/Toggle default; Position x default
	i32 iydef {};                    // Position y default
	i32 izdef {};                    // Vector z default
	Color cdef {255, 255, 255, 255}; // Color default
	const char *sdef {};             // Font default slug
	const char *unit {};             // Size: the suffix shown after the value
	i32 scale {};                    // Size: the preference stores value / scale (0 or 1 = as-is)
	i64 tag {};                      // opaque, handed to the callbacks below

	// Choice: rebuilt from getChoices each time the popup opens, so runtime-only choices work.
	void (*getChoices)(KZPlayer *, i64 tag, std::vector<KZChoice> &) {};
	i64 (*getCurrent)(KZPlayer *, i64 tag) {};
	void (*onPick)(KZPlayer *, i64 tag, i64 id) {};

	void (*onActivate)(KZPlayer *, i64 tag) {}; // Button
	// Called when a popup for this item opens (begin) and closes (begin=false).
	void (*onEdit)(KZPlayer *, i64 tag, bool begin) {};
};

struct KZOptNode
{
	const char *phraseKey {};
	std::vector<KZOptItem> items;
	std::vector<KZOptNode *> subs; // heap nodes: pointers stay valid as the registry grows
};

namespace KZ::menu
{
	// Call from a service's Init. Returns heap nodes with stable addresses.
	KZOptNode *AddCategory(const char *phraseKey);
	KZOptNode *AddSub(KZOptNode *parent, const char *phraseKey);

	void AddToggle(KZOptNode *node, const char *phraseKey, const char *prefKey, bool def, i64 tag = 0);
	// A toggle whose state and flip run through callbacks instead of a raw preference.
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
	void AddVector(KZOptNode *node, const char *phraseKey, const char *prefKey, const Vector &def, i32 lo, i32 hi, i64 tag = 0,
				   void (*onEdit)(KZPlayer *, i64, bool) = nullptr);
	void AddButton(KZOptNode *node, const char *phraseKey, void (*onActivate)(KZPlayer *, i64), i64 tag = 0);
	void AddChoice(KZOptNode *node, const char *phraseKey, void (*getChoices)(KZPlayer *, i64, std::vector<KZChoice> &),
				   i64 (*getCurrent)(KZPlayer *, i64), void (*onPick)(KZPlayer *, i64, i64), i64 tag = 0);

	void SetItemSubtext(KZOptNode *node, const char *phraseKey);
	void SetItemDivider(KZOptNode *node);
	// Declares the preference an action toggle or a choice persists to, for export and import.
	// Purely declarative: nothing else reads prefKey for those item types.
	void SetItemPref(KZOptNode *node, const char *prefKey, KZOptStorage storage);
	// Greys the item out and ignores clicks on it while the named bool preference is off. Call it
	// twice to require both.
	void SetItemEnabledBy(KZOptNode *node, const char *prefKey);
	// The suffix a Size item shows after its value, "px" unless set.
	void SetItemUnit(KZOptNode *node, const char *unit);
	// Step a Size item in whole units while the preference keeps a fraction of them.
	void SetItemScale(KZOptNode *node, i32 scale);
	// Keep gradients out of a color item's picker.
	void SetItemSolidOnly(KZOptNode *node);

	// Writes every value item in this node back to its registered default. Items with no preference
	// behind them are skipped.
	void ResetNode(KZPlayer *player, KZOptNode *node);

	const std::vector<KZOptNode *> &GetTree();
} // namespace KZ::menu
