// The paint category in the options menu.
#include "kz/paint/kz_paint.h"
#include "kz/option/menu/model.h"

#include "tier0/memdbgon.h"

void KZPaintService::RegisterMenu()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Paint");
	KZ::menu::AddColor(cat, "Menu - Color", "paintColor", KZ_PAINT_DEFAULT_COLOR);
	// Paint draws a decal with one solid color, so the gradients in the picker would not render.
	KZ::menu::SetItemSolidOnly(cat);
	KZ::menu::SetItemSubtext(cat, "Menu - Paint Color Sub");
	KZ::menu::AddSize(cat, "Menu - Size", "paintSize", (i32)KZPaintService::DEFAULT_PAINT_SIZE, (i32)KZPaintService::MIN_PAINT_SIZE,
					  (i32)KZPaintService::MAX_PAINT_SIZE);
	KZ::menu::AddToggle(cat, "Menu - Show All Paint", "showAllPaint", false);
}
