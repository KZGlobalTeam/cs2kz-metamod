#include "kz_help.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/simplecmds.h"
#include "utils/tables.h"

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

bool HelpWindow::Toggle(i32 category)
{
	this->category = Clamp(category, 0, MAX(0, MIN(scmd::GetCategoryCount(), KZ_HELP_CATEGORIES) - 1));
	const bool closing = this->player->uiService->GetTop() == this && this->layout.Get();
	this->player->uiService->Toggle(this);
	return closing || this->player->uiService->IsOpen(this);
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
	for (i32 i = 0; i < KZ_HELP_ROWS; i++)
	{
		const bool used = i < (i32)commands.size();
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
	const bool empty = commands.empty();
	this->layout.SetVar("help_empty", "empty", empty ? language->PrepareMessage("Help - Empty").c_str() : "");
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

static_global const char *columnKeys[] = {"Command List Header - Name", "Command List Header - Description"};

static_global void PrintCategoryCommands(KZPlayer *player, i32 category, bool printEmpty)
{
	char tableName[64];
	V_snprintf(tableName, sizeof(tableName), "Command List - %s", scmd::GetCategoryName(category));
	CUtlString headers[KZ_ARRAYSIZE(columnKeys)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeys); i++)
	{
		headers[i] = player->languageService->PrepareMessage(columnKeys[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(columnKeys)> table(player->languageService->PrepareMessage(tableName).c_str(), headers);

	const auto &commands = scmd::GetCategoryCommands(category);
	for (u32 i = 0; i < commands.size(); i++)
	{
		table.SetRow(i, commands[i].names.c_str(), player->languageService->PrepareMessage(commands[i].descriptionKey.c_str()).c_str());
	}
	if (!printEmpty && commands.empty())
	{
		return;
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
	player->PrintConsole(false, false, table.GetTitle());
	player->PrintConsole(false, false, table.GetHeader());

	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, table.GetLine(i));
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
}

SCMD(kz_help, SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	std::vector<i32> categories;
	if (args->ArgC() >= 2)
	{
		for (i32 i = 1; i < args->ArgC(); i++)
		{
			for (i32 j = 0; j < scmd::GetCategoryCount(); j++)
			{
				if (!V_stricmp(args->Arg(i), scmd::GetCategoryName(j)))
				{
					categories.push_back(j);
				}
			}
		}
	}

	const bool chat = args->Arg(0)[0] == SCMD_CHAT_TRIGGER || args->Arg(0)[0] == SCMD_CHAT_SILENT_TRIGGER;
	if (chat && player->uiService->GetWindow<HelpWindow>()->Toggle(categories.empty() ? 0 : categories.front()))
	{
		return true;
	}
	player->languageService->PrintChat(true, false, "Command Help Response (Chat)");
	player->languageService->PrintConsole(false, false, "Command Help Response (Console)");
	for (i32 category : categories)
	{
		PrintCategoryCommands(player, category, true);
	}
	if (categories.empty())
	{
		player->languageService->PrintConsole(false, false, "Command Help Response Category Hint (Console)");
		for (i32 i = 0; i < scmd::GetCategoryCount(); i++)
		{
			PrintCategoryCommands(player, i, false);
		}
	}
	return true;
}
