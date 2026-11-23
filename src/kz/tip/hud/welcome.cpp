#include "kz/tip/hud/welcome.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "sdk/entity/ccscustomhudlayout.h"

#include "tier0/memdbgon.h"

using namespace KZ::tip::hud;

static_global const char *const pagePanels[KZ_WELCOME_PAGES] = {"welcome_page0", "welcome_page1", "welcome_page2", "welcome_page3"};
static_global const char *const dotPanels[KZ_WELCOME_PAGES] = {"welcome_dot0", "welcome_dot1", "welcome_dot2", "welcome_dot3"};
static_global const char *const pageTitles[KZ_WELCOME_PAGES] = {"Welcome - Title", "Welcome - Mode Title", "Welcome - Radio Title",
																"Welcome - Commands Title"};

// In the order of the cards in welcome.xml.
static_global const char *const modePanels[KZ_WELCOME_MODES] = {"welcome_mode0", "welcome_mode1"};

// Each label's id and phrase.
// Every label's dialog variable is named after its id.
static_global const char *const texts[][2] = {
	{"welcome_intro", "Welcome - Intro"},
	{"welcome_goal", "Welcome - Goal"},
	{"welcome_mode_lead", "Welcome - Mode Lead"},
	{"welcome_mode0_name", "Welcome - Mode Classic"},
	{"welcome_mode0_text", "Welcome - Mode Classic Text"},
	{"welcome_mode1_name", "Welcome - Mode Vanilla"},
	{"welcome_mode1_text", "Welcome - Mode Vanilla Text"},
	{"welcome_mode_type", "Welcome - More Type"},
	{"welcome_mode_later", "Welcome - Mode Later"},
	{"welcome_radio_press", "Welcome - Radio Press"},
	{"welcome_radio_open", "Welcome - Radio Open"},
	{"welcome_radio_save", "Welcome - Radio Save"},
	{"welcome_radio_runs", "Welcome - Radio Runs"},
	{"welcome_commands_lead", "Welcome - Commands Lead"},
	{"welcome_cmd0", "Welcome - Command Restart"},
	{"welcome_cmd1", "Welcome - Command Noclip"},
	{"welcome_cmd2", "Welcome - Command Courses"},
	{"welcome_cmd3", "Welcome - Command Options"},
	{"welcome_cmd4", "Welcome - Command PB"},
	{"welcome_cmd5", "Welcome - Command WR"},
	{"welcome_cmd6", "Welcome - Command Hide"},
	{"welcome_more_type", "Welcome - More Type"},
	{"welcome_more_help", "Welcome - More Help"},
	{"welcome_more_again", "Welcome - More Again"},
	{"welcome_prev_label", "Course Top - Window Previous"},
};

bool WelcomeWindow::CanOpen()
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

bool WelcomeWindow::Show()
{
	this->page = 0;
	if (this->player->uiService->GetTop() == this && this->layout.Get())
	{
		this->Render();
		return true;
	}
	return this->player->uiService->Open(this);
}

void WelcomeWindow::OnOpen()
{
	this->Render();
}

void WelcomeWindow::Hide()
{
	if (CCSCustomHudLayout *entity = this->layout.Get())
	{
		KZ::ui::SetBoolClass(entity, "welcome_root", "hidden", this->applied.rootHidden, true);
	}
}

void WelcomeWindow::OnClose(KZ::ui::CloseReason reason)
{
	this->Hide();
}

void WelcomeWindow::OnSuspend()
{
	this->Hide();
}

void WelcomeWindow::OnResume()
{
	this->Render();
}

void WelcomeWindow::Render()
{
	CCSCustomHudLayout *entity = this->layout.Get();
	if (!entity || this->player->uiService->GetTop() != this)
	{
		return;
	}
	auto *language = this->player->languageService;
	KZ::ui::SetBoolClass(entity, "welcome_root", "hidden", this->applied.rootHidden, false);
	KZ::ui::ApplyWindowStyle(this->player, entity, "welcome_root", this->applied.style);
	for (const auto &text : texts)
	{
		this->layout.SetVar(text[0], text[0], language->PrepareMessage(text[1]).c_str());
	}
	this->layout.SetVar("welcome_window_title", "welcome_window_title", language->PrepareMessage(pageTitles[this->page]).c_str());
	// The game's csgo_key formatter swaps %radio1% for the key the player has bound to it.
	this->layout.SetVar("welcome_radio_key", "welcomekey", "%radio1%");

	for (i32 i = 0; i < KZ_WELCOME_PAGES; i++)
	{
		KZ::ui::SetBoolClass(entity, pagePanels[i], "hidden", this->applied.pageHidden[i], i != this->page);
		KZ::ui::SetBoolClass(entity, dotPanels[i], "on", this->applied.dotOn[i], i == this->page);
	}
	const i32 currentMode = KZ::mode::GetModeInfo(this->player->modeService).id;
	for (i32 i = 0; i < KZ_WELCOME_MODES; i++)
	{
		const bool selected = KZ::mode::GetModeInfo(KZ::api::modes[i]).id == currentMode;
		KZ::ui::SetBoolClass(entity, modePanels[i], "selected", this->applied.modeSelected[i], selected);
	}
	KZ::ui::SetBoolClass(entity, "welcome_prev", "disabled", this->applied.prevDisabled, this->page == 0);
	const char *nextKey = this->page == KZ_WELCOME_PAGES - 1 ? "HUD Edit - Done" : "Course Top - Window Next";
	this->layout.SetVar("welcome_next_label", "welcome_next_label", language->PrepareMessage(nextKey).c_str());
}

void WelcomeWindow::OnClick(const char *buttonId)
{
	if (KZ_STREQ(buttonId, "welcome_window_close"))
	{
		this->player->uiService->Close(this);
		return;
	}
	if (KZ_STREQ(buttonId, "welcome_prev") && this->page > 0)
	{
		this->page--;
		this->Render();
		return;
	}
	if (KZ_STREQ(buttonId, "welcome_next"))
	{
		if (this->page >= KZ_WELCOME_PAGES - 1)
		{
			this->player->uiService->Close(this);
			return;
		}
		this->page++;
		this->Render();
		return;
	}
	for (i32 i = 0; i < KZ_WELCOME_MODES; i++)
	{
		if (KZ_STREQ(buttonId, modePanels[i]))
		{
			// Prints the result in chat like !mode does and explains why the switch failed.
			g_pKZModeManager->SwitchToMode(this->player, KZ::mode::GetModeInfo(KZ::api::modes[i]).shortModeName.c_str());
			this->Render();
			return;
		}
	}
}
