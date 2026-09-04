// The paint category in the options menu.
#include "kz/paint/kz_paint.h"
#include "kz/option/menu/model.h"

#include "tier0/memdbgon.h"

#define KZ_PAINT_SIZE_MIN 1
#define KZ_PAINT_SIZE_MAX 32

void KZPaintService::RegisterMenu()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Paint");
	KZ::menu::AddColor(cat, "Menu - Color", "paintColor", KZ_PAINT_DEFAULT_COLOR);
	// Paint draws a decal with one solid color, so the gradients in the picker would not render.
	KZ::menu::SetItemSolidOnly(cat);
	KZ::menu::SetItemSubtext(cat, "Menu - Paint Color Sub");
	KZ::menu::AddSize(cat, "Menu - Size", "paintSize", (i32)KZPaintService::DEFAULT_PAINT_SIZE, KZ_PAINT_SIZE_MIN, KZ_PAINT_SIZE_MAX);
	KZ::menu::AddToggle(cat, "Menu - Show All Paint", "showAllPaint", false);
}
