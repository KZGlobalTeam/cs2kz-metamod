#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "entitykeyvalues.h"
#include "utils/utils.h"
#include "cs2kz.h"

#include "tier0/memdbgon.h"

// === Writing classes onto the layout ================================================

void KZHUDService::SetLayoutClass(CCSCustomHudLayout *layout, const char *panelId, const char *&cache, const char *className)
{
	// Both names come from static storage and comparing the pointers
	// is enough to tell whether the class actually changed.
	if (cache == className)
	{
		return;
	}
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	if (cache)
	{
		layout->SetHasClassForPlayer(slot, panelId, cache, k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	if (className)
	{
		layout->SetHasClassForPlayer(slot, panelId, className, k_eHudPanelClassStatus_HasClass);
	}
	cache = className;
}

// Same swap for the classes whose value space is too large to keep literals for: the number is
// cached instead and the class name only built when it moves.
void KZHUDService::SetLayoutValueClass(CCSCustomHudLayout *layout, const char *panelId, i32 &cache, i32 value, const char *prefix, bool percent)
{
	value = percent ? PanoramaSnapToStep(value, -100, 100) : PanoramaSnapToStep(value, 0, 500);
	if (cache == value)
	{
		return;
	}
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	const char *unit = percent ? "pct" : "px";
	char className[64];
	if (cache != INT_MIN)
	{
		V_snprintf(className, sizeof(className), "%s--%s%i%s", prefix, cache < 0 ? "neg" : "", abs(cache), unit);
		layout->SetHasClassForPlayer(slot, panelId, className, k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	V_snprintf(className, sizeof(className), "%s--%s%i%s", prefix, value < 0 ? "neg" : "", abs(value), unit);
	layout->SetHasClassForPlayer(slot, panelId, className, k_eHudPanelClassStatus_HasClass);
	cache = value;
}

void KZHUDService::UpdateLayoutElement(CCSCustomHudLayout *layout, MHUDElement element, bool show, const char *text, const Color &color, bool force)
{
	const MHUDElementDef &def = MHUD_ELEMENTS[(i32)element];
	LayoutElementState &state = this->layoutElements[(i32)element];
	const CPlayerSlot slot = this->player->GetPlayerSlot();

	if (force)
	{
		state = LayoutElementState();
	}

	if (state.hidden != !show)
	{
		state.hidden = !show;
		layout->SetHasClassForPlayer(slot, def.panelId, "hidden",
									 state.hidden ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	// A hidden element keeps its last values, so toggling it back on costs nothing.
	if (state.hidden)
	{
		return;
	}

	// NULL means the caller had nothing to read: hold the last value instead of blanking it.
	if (text && state.text != text)
	{
		state.text = text;
		layout->SetDialogVariableStringForPlayer(slot, def.panelId, def.varName, text);
	}

	auto *opts = this->player->optionService;
	this->SetLayoutValueClass(layout, def.panelId, state.x, (i32)opts->GetPreferenceFloat(def.xKey, def.xDefault), "x", true);
	this->SetLayoutValueClass(layout, def.panelId, state.y, (i32)opts->GetPreferenceFloat(def.yKey, def.yDefault), "y", true);
	this->SetLayoutValueClass(layout, def.panelId, state.fontSize, (i32)opts->GetPreferenceFloat(def.sizeKey, def.sizeDefault), "font-size", false);
	this->SetLayoutClass(layout, def.panelId, state.colorClass, PanoramaNearestColorClass(color));
	this->SetLayoutClass(layout, def.panelId, state.fontClass, MHUDFontClass(this->player, element));

	const bool outline = this->IsMHUDOutlineEnabled(element);
	if (state.outline != outline)
	{
		state.outline = outline;
		layout->SetHasClassForPlayer(slot, def.panelId, "outline",
									 outline ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
}

// === The layout entity ==============================================================

CHandle<CBaseEntity> g_hMHUDLayout;

CCSCustomHudLayout *KZHUDService::GetLayoutEntity(const char *layoutPath, CHandle<CBaseEntity> &cache)
{
	if (g_KZPlugin.unloading || !KZHUDService::IsLayoutHudAvailable())
	{
		return NULL;
	}
	if (CBaseEntity *cached = cache.Get())
	{
		return (CCSCustomHudLayout *)cached;
	}
	// A reloaded plugin loses the handle but not the entity, so adopt ours before spawning a second.
	for (CBaseEntity *ent = utils::FindEntityByClassname(NULL, "custom_hud_layout"); ent;
		 ent = utils::FindEntityByClassname(ent, "custom_hud_layout"))
	{
		CCSCustomHudLayout *existing = (CCSCustomHudLayout *)ent;
		const char *path = existing->m_strLayout().String();
		if (path && V_strcmp(path, layoutPath) == 0)
		{
			cache = existing->GetRefEHandle();
			return existing;
		}
	}
	CCSCustomHudLayout *layout = utils::CreateEntityByName<CCSCustomHudLayout>("custom_hud_layout");
	if (!layout)
	{
		return NULL;
	}
	// "layout" is the keyvalue name the FGD declares for m_strLayout.
	CEntityKeyValues *pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("layout", layoutPath);
	layout->DispatchSpawn(pKeyValues);
	cache = layout->GetRefEHandle();
	return layout;
}
