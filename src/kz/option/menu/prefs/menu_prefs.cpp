// The options menu's own appearance (font + color), registered as its own category.
#include "kz/option/menu/model.h"

#include "tier0/memdbgon.h"

#define KZ_MENU_DEFAULT_FONT "stratum2-medium-tf"

void MenuRegisterChromePrefs()
{
	KZOptNode *cat = KZMenu::AddCategory("Menu - Menu");
	KZMenu::AddFont(cat, "Menu - Font", "menuFont", KZ_MENU_DEFAULT_FONT);
	KZMenu::AddColor(cat, "Menu - Color", "menuColor", Color(255, 255, 255, 255));
}
