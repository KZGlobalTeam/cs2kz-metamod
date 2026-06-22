#pragma once

#include "common.h"
#include "playerslot.h"

class ICS2Menus;

// Bridge to the optional mm-cs2menus plugin (ICS2Menus).
namespace KZ::menu
{
	// (Re)resolve the interface. Call on AllPluginsLoaded and plugin load/unload.
	void UpdateInterface();
	bool IsAvailable();
	ICS2Menus *GetInterface();

	// True if the slot has an HTML (center-screen) cs2menus menu open
	// so the HUD can yield the center channel.
	bool HasActiveHtmlMenu(CPlayerSlot slot);
} // namespace KZ::menu
