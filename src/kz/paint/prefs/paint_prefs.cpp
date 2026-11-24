// The paint category in the options menu.
#include "kz/paint/kz_paint.h"
#include "kz/option/menu/model.h"

#include "tier0/memdbgon.h"

void KZPaintService::RegisterMenu()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Paint");
	KZ::menu::AddColor(cat, "Menu - Color", "paintColor", KZ_PAINT_DEFAULT_COLOR);
	// Paint draws a decal with one solid color and the gradients in the picker would not render.
	KZ::menu::SetItemSolidOnly(cat);
	KZ::menu::SetItemSubtext(cat, "Menu - Paint Color Sub");
	KZ::menu::AddSize(cat, "Menu - Size", "paintSize", (i32)KZPaintService::defaultPaintSize, (i32)KZPaintService::minPaintSize,
					  (i32)KZPaintService::maxPaintSize);
	KZ::menu::AddToggle(cat, "Menu - Show All Paint", "showAllPaint", false);
}
