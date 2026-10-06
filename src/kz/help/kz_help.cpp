#include "kz_help.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/simplecmds.h"

#include "tier0/memdbgon.h"

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

bool HelpWindow::CanOpen()
{
	if (this->player->hudService->IsEditingHud() || !KZHUDService::IsLayoutHudAvailable())
	{
		return false;
	}
	bool created = false;
	CCSCustomHudLayout *entity = this->layout.Ensure(created);
	if (created)
	{
		this->applied = Applied();
	}
	return entity && entity->GetPlayerLayoutState(this->player->GetPlayerSlot());
}

bool HelpWindow::Show(i32 category)
{
	this->category = Clamp(category, 0, MAX(0, MIN(scmd::GetCategoryCount(), KZ_HELP_CATEGORIES) - 1));
	if (scmd::GetCategoryCommands(this->category, true).size() > KZ_HELP_ROWS)
	{
		this->player->uiService->Close(this);
		return false;
	}
	if (!this->player->uiService->Open(this))
	{
		return false;
	}
	this->Render();
	return true;
}

void HelpWindow::OnOpen()
{
	this->shown = true;
	this->Render();
}

void HelpWindow::Hide()
{
	this->shown = false;
	if (CCSCustomHudLayout *entity = this->layout.Get())
	{
		KZ::ui::SetBoolClass(entity, "help_root", "hidden", this->applied.rootHidden, true);
	}
}

void HelpWindow::OnClose(KZ::ui::CloseReason reason)
{
	this->Hide();
}

void HelpWindow::OnSuspend()
{
	this->Hide();
}

void HelpWindow::OnResume()
{
	this->shown = true;
	this->Render();
}

void HelpWindow::Render()
{
	CCSCustomHudLayout *entity = this->layout.Get();
	if (!this->shown || !entity)
	{
		return;
	}
	auto *language = this->player->languageService;
	KZ::ui::SetBoolClass(entity, "help_root", "hidden", this->applied.rootHidden, false);
	KZ::ui::ApplyWindowStyle(this->player, entity, "help_root", this->applied.style);
	this->layout.SetVar("help_title", "title", language->PrepareMessage("Help - Title").c_str());
	this->layout.SetVar("help_hint", "hint", language->PrepareMessage("Help - Hint").c_str());
	for (i32 i = 0; i < KZ_HELP_CATEGORIES; i++)
	{
		const bool used = i < scmd::GetCategoryCount();
		if (used)
		{
			char key[64];
			V_snprintf(key, sizeof(key), "Help - Category - %s", scmd::GetCategoryName(i));
			this->layout.SetVar(CategoryLabel(i), CategoryVar(i), language->PrepareMessage(key).c_str());
			KZ::ui::SetBoolClass(entity, CategoryPanel(i), "selected", this->applied.categorySelected[i], i == this->category);
		}
		KZ::ui::SetBoolClass(entity, CategoryPanel(i), "hidden", this->applied.categoryHidden[i], !used);
	}
	const auto &commands = scmd::GetCategoryCommands(this->category, true);
	const bool overflow = commands.size() > KZ_HELP_ROWS;
	for (i32 i = 0; i < KZ_HELP_ROWS; i++)
	{
		const bool used = !overflow && i < (i32)commands.size();
		if (used)
		{
			const auto &command = commands[i];
			const auto description = language->PrepareMessage(command.descriptionKey.c_str());
			this->layout.SetVar(CommandLabel(i), CommandVar(i), command.names.c_str());
			this->layout.SetVar(DescriptionLabel(i), DescriptionVar(i), description.c_str());
			KZ::ui::SetBoolClass(entity, DescriptionLabel(i), "hidden", this->applied.descriptionHidden[i], description == command.descriptionKey);
		}
		KZ::ui::SetBoolClass(entity, RowPanel(i), "hidden", this->applied.rowHidden[i], !used);
	}
	const bool empty = commands.empty() || overflow;
	this->layout.SetVar("help_empty", "empty", empty ? language->PrepareMessage(overflow ? "Help - Overflow" : "Help - Empty").c_str() : "");
	KZ::ui::SetBoolClass(entity, "help_empty", "hidden", this->applied.emptyHidden, !empty);
}

void HelpWindow::OnClick(const char *buttonId)
{
	if (KZ_STREQ(buttonId, "help_close"))
	{
		this->player->uiService->Close(this);
		return;
	}
	for (i32 i = 0; i < MIN(scmd::GetCategoryCount(), KZ_HELP_CATEGORIES); i++)
	{
		if (KZ_STREQ(buttonId, CategoryPanel(i)))
		{
			this->category = i;
			this->Render();
			break;
		}
	}
}
