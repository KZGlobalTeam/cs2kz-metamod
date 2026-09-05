#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "sdk/datatypes.h"
#include "checktransmitinfo.h"
#include "entitykeyvalues.h"
#include "utils/utils.h"
#include "cs2kz.h"

#include "tier0/memdbgon.h"

// === Writing classes onto the layout (global) =================================

void KZHUDService::SetLayoutClass(CCSCustomHudLayout *layout, const char *panelId, const char *&cache, const char *className)
{
	if (cache == className)
	{
		return;
	}
	if (cache)
	{
		layout->SetHasClass(panelId, cache, k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	if (className)
	{
		layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_HasClass);
	}
	cache = className;
}

void KZHUDService::SetLayoutValueClass(CCSCustomHudLayout *layout, const char *panelId, i32 &cache, i32 value, const char *prefix, bool percent)
{
	value = percent ? panorama::SnapToStep(value, -100, 100) : panorama::SnapToStep(value, 0, 500);
	if (cache == value)
	{
		return;
	}
	const char *unit = percent ? "pct" : "px";
	char className[64];
	if (cache != INT_MIN)
	{
		V_snprintf(className, sizeof(className), "%s--%s%i%s", prefix, cache < 0 ? "neg" : "", abs(cache), unit);
		layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	V_snprintf(className, sizeof(className), "%s--%s%i%s", prefix, value < 0 ? "neg" : "", abs(value), unit);
	layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_HasClass);
	cache = value;
}

void KZHUDService::UpdateLayoutElement(CCSCustomHudLayout *layout, MHUDElement element, bool show, const char *text, const Color &color, bool force)
{
	const MHUDElementDef &def = MHUD_ELEMENTS[(i32)element];
	LayoutElementState &state = this->layoutElements[(i32)element];

	if (force)
	{
		state = LayoutElementState();
	}

	if (state.hidden != !show)
	{
		state.hidden = !show;
		layout->SetHasClass(def.panelId, "hidden", state.hidden ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
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
		layout->SetDialogVariableString(def.panelId, def.varName, text);
	}

	const MHUDPrefs::Element &cached = this->GetPrefs().elements[(i32)element];
	this->SetLayoutValueClass(layout, def.panelId, state.x, (i32)cached.x, "x", true);
	this->SetLayoutValueClass(layout, def.panelId, state.y, (i32)cached.y, "y", true);
	this->SetLayoutValueClass(layout, def.panelId, state.fontSize, (i32)cached.size, "font-size", false);
	const u32 packed = ((u32)color.r() << 24) | ((u32)color.g() << 16) | ((u32)color.b() << 8) | (u32)color.a();
	if (!state.colorComputed || state.lastColorPacked != packed)
	{
		state.colorComputed = true;
		state.lastColorPacked = packed;
		state.colorClassComputed = panorama::ResolveColorClass(color);
	}
	this->SetLayoutClass(layout, def.panelId, state.colorClass, state.colorClassComputed);
	this->SetLayoutClass(layout, def.panelId, state.fontClass, KZHUDService::GetMHUDFontClass(this->player, element));

	const i32 opacity = Clamp(cached.opacity, 0, 100);
	if (state.opacity != opacity)
	{
		char className[32];
		if (state.opacity != INT_MIN)
		{
			V_snprintf(className, sizeof(className), "opacity--%ipct", state.opacity);
			layout->SetHasClass(def.panelId, className, k_eHudPanelClassStatus_DoesNotHaveClass);
		}
		V_snprintf(className, sizeof(className), "opacity--%ipct", opacity);
		layout->SetHasClass(def.panelId, className, k_eHudPanelClassStatus_HasClass);
		state.opacity = opacity;
	}

	const bool outline = this->IsMHUDOutlineEnabled(element);
	if (state.outline != outline)
	{
		state.outline = outline;
		layout->SetHasClass(def.panelId, "outline", outline ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
}

CCSCustomHudLayout *KZHUDService::EnsureOwnedLayout(bool &created)
{
	created = false;
	if (g_KZPlugin.unloading || !KZHUDService::IsLayoutHudAvailable())
	{
		return NULL;
	}
	if (CBaseEntity *cached = this->ownedLayout.Get())
	{
		return (CCSCustomHudLayout *)cached;
	}
	CCSCustomHudLayout *layout = utils::CreateEntityByName<CCSCustomHudLayout>("custom_hud_layout");
	if (!layout)
	{
		return NULL;
	}
	CEntityKeyValues *pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("layout", KZ_MHUD_LAYOUT);
	// A per-slot targetname so the entity is identifiable in a debugger.
	char name[32];
	V_snprintf(name, sizeof(name), "kzmhud%i", this->player->GetPlayerSlot().Get());
	pKeyValues->SetString("targetname", name);
	layout->DispatchSpawn(pKeyValues);
	this->ownedLayout = layout->GetRefEHandle();
	created = true;
	return layout;
}

void KZHUDService::DestroyOwnedLayout()
{
	if (CBaseEntity *ent = this->ownedLayout.Get())
	{
		g_pKZUtils->RemoveEntity(ent);
	}
	this->ownedLayout = nullptr;
	// Cleared here so a stale cache never survives a destroy without a matching entity.
	for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
	{
		this->layoutElements[i] = LayoutElementState();
	}
	this->layoutKeys = LayoutKeysState();
	this->layoutCrosshair = LayoutCrosshairState();
}

void KZHUDService::Cleanup()
{
	for (i32 i = 0; i < MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(i));
		if (player && player->hudService)
		{
			player->hudService->DestroyOwnedLayout();
		}
	}
}

void KZHUDService::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
{
	static_persist const i32 offset = g_pGameConfig->GetOffset("QuietPlayerSlot");
	for (i32 i = 0; i < infoCount; i++)
	{
		TransmitInfo *info = reinterpret_cast<TransmitInfo *>(pInfo[i]);
		const i32 recipient = *reinterpret_cast<int *>(reinterpret_cast<uintptr_t>(info) + offset);
		for (i32 owner = 0; owner < MAXPLAYERS; owner++)
		{
			if (owner == recipient)
			{
				continue;
			}
			KZPlayer *ownerPlayer = g_pKZPlayerManager->ToPlayer(CPlayerSlot(owner));
			if (!ownerPlayer || !ownerPlayer->hudService)
			{
				continue;
			}
			CBaseEntity *ent = ownerPlayer->hudService->ownedLayout.Get();
			if (ent)
			{
				info->m_pTransmitEdict->Clear(ent->entindex());
			}
		}
	}
}

// === Shared entity (menu only) ======================================================

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
	CEntityKeyValues *pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("layout", layoutPath);
	layout->DispatchSpawn(pKeyValues);
	cache = layout->GetRefEHandle();
	return layout;
}
