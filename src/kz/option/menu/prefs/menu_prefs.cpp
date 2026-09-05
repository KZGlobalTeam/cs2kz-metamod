// The options menu's own appearance.
#include "kz/option/menu/model.h"
#include "kz/option/menu/kz_menu.h"
#include "kz/option/pref_registry.h"

#include "tier0/memdbgon.h"

#define KZ_MENU_DEFAULT_FONT "stratum2-medium-tf"

void KZMenuService::RegisterChromePrefs()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Menu");
	KZ::menu::AddFont(cat, "Menu - Font", "menuFont", KZ_MENU_DEFAULT_FONT);
	KZ::menu::AddColor(cat, "Menu - Color", "menuColor", Color(255, 255, 255, 255));
	KZ::menu::AddToggle(cat, "Menu - Sounds", "menuSounds", true);
	KZ::menu::SetItemDivider(cat);
	KZ::prefs::RegisterMenu(cat);
}
