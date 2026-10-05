#include "kz_help.h"
#include "cs2kz.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/kz_menu.h"
#include "kz/option/menu/tables.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "sdk/datatypes.h"
#include "checktransmitinfo.h"
#include "entitykeyvalues.h"
#include "utils/simplecmds.h"
#include "utils/utils.h"

#include <vendor/mm-cs2menus/src/public/ics2menus.h>
extern ICS2Menus *g_pMenus;

#include "tier0/memdbgon.h"

#define KZ_HELP_LAYOUT       "panorama/layout/custom_game/cs2kz/help.xml"
#define KZ_HELP_DEFAULT_FONT "stratum2-medium-tf"

#define SLOT_ID(fn, fmt) \
	static_function const char *fn(i32 i) \
	{ \
		static_persist char buf[24]; \
		V_snprintf(buf, sizeof(buf), fmt, i); \
		return buf; \
	}

SLOT_ID(CategoryPanel, "help_cat%i")
SLOT_ID(CategoryLabel, "help_cat_label%i")
SLOT_ID(CategoryVar, "hc%i")
SLOT_ID(RowPanel, "help_row%i")
SLOT_ID(CommandLabel, "help_command%i")
SLOT_ID(CommandVar, "hn%i")
SLOT_ID(DescriptionLabel, "help_description%i")
SLOT_ID(DescriptionVar, "hd%i")
#undef SLOT_ID

CCSCustomHudLayout *KZHelpService::EnsureLayout()
{
	if (g_KZPlugin.unloading || !KZHUDService::IsLayoutHudAvailable())
	{
		return nullptr;
	}
	if (CBaseEntity *cached = this->layoutEntity.Get())
	{
		return (CCSCustomHudLayout *)cached;
	}
	CCSCustomHudLayout *layout = utils::CreateEntityByName<CCSCustomHudLayout>("custom_hud_layout");
	if (!layout)
	{
		return nullptr;
	}
	CEntityKeyValues *kv = new CEntityKeyValues();
	kv->SetString("layout", KZ_HELP_LAYOUT);
	char name[32];
	V_snprintf(name, sizeof(name), "kzhelp%i", this->player->GetPlayerSlot().Get());
	kv->SetString("targetname", name);
	layout->DispatchSpawn(kv);
	this->layoutEntity = layout->GetRefEHandle();
	this->applied = Applied();
	this->writtenVars.clear();
	return layout;
}

bool KZHelpService::Show(i32 category)
{
	if (this->player->hudService->IsEditingHud())
	{
		return false;
	}
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	CCSCustomHudLayout *layout = this->EnsureLayout();
	if (!layout || !layout->GetPlayerLayoutState(slot))
	{
		return false;
	}
	// Only the active window owns cursor input and the external menu slot.
	this->player->menuService->Close();
	if (g_pMenus)
	{
		g_pMenus->CancelMenu(slot.Get());
		g_pMenus->SetExternalBusy(slot.Get(), true);
	}
	this->category = Clamp(category, 0, MIN(scmd::GetCategoryCount(), KZ_HELP_CATEGORIES) - 1);
	this->page = 0;
	this->open = true;
	layout->SetInputCaptureEnabled(slot, true);
	layout->SetHasClass("help_root", "hidden", k_eHudPanelClassStatus_DoesNotHaveClass);
	this->Render();
	return true;
}

void KZHelpService::Close()
{
	if (!this->open)
	{
		return;
	}
	this->open = false;
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	if (CBaseEntity *ent = GameEntitySystem() ? this->layoutEntity.Get() : nullptr)
	{
		CCSCustomHudLayout *layout = (CCSCustomHudLayout *)ent;
		layout->SetHasClass("help_root", "hidden", k_eHudPanelClassStatus_HasClass);
		layout->SetInputCaptureEnabled(slot, false);
	}
	if (g_pMenus)
	{
		g_pMenus->SetExternalBusy(slot.Get(), false);
	}
}

void KZHelpService::SetBoolClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool &cache, bool want)
{
	if (cache != want)
	{
		cache = want;
		layout->SetHasClass(panelId, className, want ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
}

void KZHelpService::SetSwapClass(CCSCustomHudLayout *layout, const char *&cache, const char *want)
{
	if (cache == want)
	{
		return;
	}
	if (cache)
	{
		layout->SetHasClass("help_root", cache, k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	if (want)
	{
		layout->SetHasClass("help_root", want, k_eHudPanelClassStatus_HasClass);
	}
	cache = want;
}

void KZHelpService::SetVar(CCSCustomHudLayout *layout, const char *panelId, const char *var, const char *value)
{
	// Unchanged values must not trigger a full layout resend.
	std::string &cached = this->writtenVars[var];
	if (cached != value)
	{
		cached = value;
		layout->SetDialogVariableString(panelId, var, value);
	}
}

void KZHelpService::Render()
{
	CCSCustomHudLayout *layout = (CCSCustomHudLayout *)this->layoutEntity.Get();
	if (!this->open || !layout)
	{
		return;
	}
	auto *language = this->player->languageService;
	auto *opts = this->player->optionService;
	const char *font = panorama::ResolveFontClass(opts->GetPreferenceStr("menuFont", KZ_HELP_DEFAULT_FONT), KZ_HELP_DEFAULT_FONT);
	const char *color = panorama::ResolveColorClass(opts->GetPreferenceColor("menuColor", Color(255, 255, 255, 255)));
	if (this->applied.font != font)
	{
		this->SetBoolClass(layout, "help_root", "font-reflow", this->applied.fontReflow, !this->applied.fontReflow);
	}
	this->SetSwapClass(layout, this->applied.font, font);
	this->SetSwapClass(layout, this->applied.color, color);
	this->SetBoolClass(layout, "help_root", "snd", this->applied.sounds, opts->GetPreferenceBool("menuSounds", true));
	this->SetVar(layout, "help_title", "title", language->PrepareMessage("Help - Title").c_str());
	this->SetVar(layout, "help_hint", "hint", language->PrepareMessage("Help - Hint").c_str());
	for (i32 i = 0; i < KZ_HELP_CATEGORIES; i++)
	{
		const bool used = i < scmd::GetCategoryCount();
		if (used)
		{
			char key[64];
			V_snprintf(key, sizeof(key), "Command List - %s", scmd::GetCategoryName(i));
			this->SetVar(layout, CategoryLabel(i), CategoryVar(i), language->PrepareMessage(key).c_str());
			this->SetBoolClass(layout, CategoryPanel(i), "selected", this->applied.categorySelected[i], i == this->category);
		}
		this->SetBoolClass(layout, CategoryPanel(i), "hidden", this->applied.categoryHidden[i], !used);
	}
	const auto commands = scmd::GetCategoryCommands(this->category, true);
	const i32 pages = MAX(1, ((i32)commands.size() + KZ_HELP_ROWS - 1) / KZ_HELP_ROWS);
	this->page = Clamp(this->page, 0, pages - 1);
	const i32 first = this->page * KZ_HELP_ROWS;
	for (i32 i = 0; i < KZ_HELP_ROWS; i++)
	{
		const bool used = first + i < (i32)commands.size();
		if (used)
		{
			const auto &command = commands[first + i];
			const auto description = language->PrepareMessage(command.descriptionKey.c_str());
			this->SetVar(layout, CommandLabel(i), CommandVar(i), command.names.c_str());
			this->SetVar(layout, DescriptionLabel(i), DescriptionVar(i), description.c_str());
			this->SetBoolClass(layout, DescriptionLabel(i), "hidden", this->applied.descriptionHidden[i], description == command.descriptionKey);
		}
		this->SetBoolClass(layout, RowPanel(i), "hidden", this->applied.rowHidden[i], !used);
	}
	char page[32];
	V_snprintf(page, sizeof(page), "%i / %i", this->page + 1, pages);
	this->SetVar(layout, "help_page", "page", page);
	this->SetVar(layout, "help_empty", "empty", commands.empty() ? language->PrepareMessage("Help - Empty").c_str() : "");
	this->SetBoolClass(layout, "help_empty", "hidden", this->applied.emptyHidden, !commands.empty());
	this->SetBoolClass(layout, "help_prev", "disabled", this->applied.previousDisabled, this->page == 0);
	this->SetBoolClass(layout, "help_next", "disabled", this->applied.nextDisabled, this->page == pages - 1);
}

void KZHelpService::OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (!player || !player->helpService || !player->helpService->open || (CBaseEntity *)layout != player->helpService->layoutEntity.Get())
	{
		return;
	}
	KZHelpService *help = player->helpService;
	if (KZ_STREQ(buttonId, "help_close"))
	{
		help->Close();
	}
	else if (KZ_STREQ(buttonId, "help_prev") || KZ_STREQ(buttonId, "help_next"))
	{
		const i32 count = (i32)scmd::GetCategoryCommands(help->category, true).size();
		const i32 pages = MAX(1, (count + KZ_HELP_ROWS - 1) / KZ_HELP_ROWS);
		const i32 page = Clamp(help->page + (KZ_STREQ(buttonId, "help_next") ? 1 : -1), 0, pages - 1);
		if (page != help->page)
		{
			help->page = page;
			help->Render();
		}
	}
	else
	{
		for (i32 i = 0; i < MIN(scmd::GetCategoryCount(), KZ_HELP_CATEGORIES); i++)
		{
			if (KZ_STREQ(buttonId, CategoryPanel(i)))
			{
				help->category = i;
				help->page = 0;
				help->Render();
				break;
			}
		}
	}
}

void KZHelpService::Reset()
{
	this->Close();
	this->DestroyLayout();
	this->category = 0;
	this->page = 0;
	this->applied = Applied();
	this->writtenVars.clear();
}

void KZHelpService::DestroyLayout()
{
	if (CBaseEntity *ent = GameEntitySystem() ? this->layoutEntity.Get() : nullptr)
	{
		g_pKZUtils->RemoveEntity(ent);
	}
	this->layoutEntity = nullptr;
}

void KZHelpService::OnClientDisconnect()
{
	this->Reset();
}

void KZHelpService::Cleanup()
{
	for (i32 i = 0; i < MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(i));
		if (player && player->helpService)
		{
			player->helpService->OnClientDisconnect();
		}
	}
}

void KZHelpService::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
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
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(owner));
			CBaseEntity *ent = player && player->helpService ? player->helpService->layoutEntity.Get() : nullptr;
			if (ent)
			{
				info->m_pTransmitEdict->Clear(ent->entindex());
			}
		}
	}
}
