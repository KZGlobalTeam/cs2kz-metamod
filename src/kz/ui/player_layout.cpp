#include "kz/ui/player_layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "sdk/datatypes.h"
#include "checktransmitinfo.h"
#include "entitykeyvalues.h"
#include "utils/utils.h"
#include "cs2kz.h"

#include <vector>

#include "tier0/memdbgon.h"

// Players are built during static initialisation, so the list has to exist before its first use rather than in static
// order.
static_function std::vector<KZ::ui::PlayerLayout *> &GetPlayerLayouts()
{
	static_persist std::vector<KZ::ui::PlayerLayout *> playerLayouts;
	return playerLayouts;
}

namespace KZ::ui
{
	PlayerLayout::PlayerLayout(KZPlayer *owner, const char *layoutPath, const char *namePrefix)
		: owner(owner), layoutPath(layoutPath), namePrefix(namePrefix)
	{
		GetPlayerLayouts().push_back(this);
	}

	PlayerLayout::~PlayerLayout()
	{
		std::vector<PlayerLayout *> &playerLayouts = GetPlayerLayouts();
		for (size_t i = 0; i < playerLayouts.size(); i++)
		{
			if (playerLayouts[i] == this)
			{
				playerLayouts.erase(playerLayouts.begin() + i);
				break;
			}
		}
	}

	CCSCustomHudLayout *PlayerLayout::Ensure(bool &created)
	{
		created = false;
		if (g_KZPlugin.unloading)
		{
			return NULL;
		}
		if (CCSCustomHudLayout *existing = this->Get())
		{
			return existing;
		}
		CCSCustomHudLayout *layout = utils::CreateEntityByName<CCSCustomHudLayout>("custom_hud_layout");
		if (!layout)
		{
			return NULL;
		}
		CEntityKeyValues *kv = new CEntityKeyValues();
		kv->SetString("layout", this->layoutPath);
		// A per-slot targetname so the entity is identifiable in a debugger.
		char name[32];
		V_snprintf(name, sizeof(name), "%s%i", this->namePrefix, this->owner->GetPlayerSlot().Get());
		kv->SetString("targetname", name);
		layout->DispatchSpawn(kv);
		this->entity = layout->GetRefEHandle();
		this->writtenVars.clear();
		created = true;
		return layout;
	}

	CCSCustomHudLayout *PlayerLayout::Get()
	{
		// Null on server exit.
		return GameEntitySystem() ? (CCSCustomHudLayout *)this->entity.Get() : NULL;
	}

	void PlayerLayout::Destroy()
	{
		if (CCSCustomHudLayout *layout = this->Get())
		{
			g_pKZUtils->RemoveEntity(layout);
		}
		this->entity = nullptr;
		this->writtenVars.clear();
	}

	bool PlayerLayout::Owns(CCSCustomHudLayout *layout)
	{
		return layout && layout == this->Get();
	}

	void PlayerLayout::SetInputCapture(bool enabled)
	{
		if (CCSCustomHudLayout *layout = this->Get())
		{
			layout->SetInputCaptureEnabled(this->owner->GetPlayerSlot(), enabled);
		}
	}

	void PlayerLayout::SetVar(const char *panelId, const char *var, const char *value)
	{
		CCSCustomHudLayout *layout = this->Get();
		if (!layout)
		{
			return;
		}
		std::string &cached = this->writtenVars[var];
		if (cached == value)
		{
			return;
		}
		cached = value;
		layout->SetDialogVariableString(panelId, var, value);
	}

	void PlayerLayout::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
	{
		struct Owned
		{
			i32 entIndex;
			i32 ownerSlot;
		};

		static_persist const i32 offset = g_pGameConfig->GetOffset("QuietPlayerSlot");
		static_persist std::vector<Owned> owned;
		owned.clear();
		for (PlayerLayout *playerLayout : GetPlayerLayouts())
		{
			if (CCSCustomHudLayout *layout = playerLayout->Get())
			{
				owned.push_back({layout->entindex(), playerLayout->owner->GetPlayerSlot().Get()});
			}
		}
		if (owned.empty())
		{
			return;
		}
		for (i32 i = 0; i < infoCount; i++)
		{
			TransmitInfo *info = reinterpret_cast<TransmitInfo *>(pInfo[i]);
			const i32 recipient = *reinterpret_cast<int *>(reinterpret_cast<uintptr_t>(info) + offset);
			for (const Owned &entry : owned)
			{
				if (entry.ownerSlot != recipient)
				{
					info->m_pTransmitEdict->Clear(entry.entIndex);
				}
			}
		}
	}

	void PlayerLayout::DestroyAll()
	{
		for (PlayerLayout *playerLayout : GetPlayerLayouts())
		{
			playerLayout->Destroy();
		}
	}

	void SetClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool on)
	{
		layout->SetHasClass(panelId, className, on ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}

	void SetBoolClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool &cache, bool want)
	{
		if (cache != want)
		{
			cache = want;
			SetClass(layout, panelId, className, want);
		}
	}

	void SetSwapClass(CCSCustomHudLayout *layout, const char *panelId, const char *&cache, const char *want)
	{
		if (cache == want)
		{
			return;
		}
		if (cache)
		{
			SetClass(layout, panelId, cache, false);
		}
		if (want)
		{
			SetClass(layout, panelId, want, true);
		}
		cache = want;
	}

	void ApplyWindowStyle(KZPlayer *player, CCSCustomHudLayout *layout, const char *rootPanelId, WindowStyle &cache)
	{
		KZOptionService *opts = player->optionService;
		const char *font = panorama::ResolveFontClass(opts->GetPreferenceStr("menuFont", KZ_UI_DEFAULT_FONT), KZ_UI_DEFAULT_FONT);
		const char *color = panorama::ResolveColorClass(opts->GetPreferenceColor("menuColor", KZ_UI_DEFAULT_COLOR));

		// Labels keep their old font until their text or width changes, so a font change also flips font-reflow.
		if (cache.font != font)
		{
			SetBoolClass(layout, rootPanelId, "font-reflow", cache.fontReflow, !cache.fontReflow);
		}
		SetSwapClass(layout, rootPanelId, cache.font, font);
		SetSwapClass(layout, rootPanelId, cache.color, color);
		SetBoolClass(layout, rootPanelId, "snd", cache.sounds, opts->GetPreferenceBool("menuSounds", true));
	}
} // namespace KZ::ui
