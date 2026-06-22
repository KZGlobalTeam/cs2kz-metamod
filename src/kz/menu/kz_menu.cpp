#include "kz_menu.h"

#include <vendor/mm-cs2menus/src/public/ics2menus.h>

namespace KZ::menu
{
	static ICS2Menus *g_pMenus = nullptr;

	void UpdateInterface()
	{
		g_pMenus = g_SMAPI ? static_cast<ICS2Menus *>(g_SMAPI->MetaFactory(CS2MENUS_INTERFACE, nullptr, nullptr)) : nullptr;
	}

	bool IsAvailable()
	{
		return g_pMenus != nullptr;
	}

	ICS2Menus *GetInterface()
	{
		return g_pMenus;
	}

	bool HasActiveHtmlMenu(CPlayerSlot slot)
	{
		return g_pMenus && g_pMenus->GetActiveMenuType(slot.Get()) == MenuType::Html;
	}
} // namespace KZ::menu
