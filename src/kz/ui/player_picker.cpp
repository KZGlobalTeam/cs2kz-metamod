#include "kz/ui/player_picker.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"

#include <algorithm>

#include "tier0/memdbgon.h"

static_function const char *RowButton(i32 i)
{
	static_persist char buf[32];
	V_snprintf(buf, sizeof(buf), "picker_row%i", i);
	return buf;
}

static_function const char *RowName(i32 i)
{
	static_persist char buf[32];
	V_snprintf(buf, sizeof(buf), "picker_row%i_name", i);
	return buf;
}

namespace KZ::ui
{
	bool PlayerPicker::Show(const char *titlePhrase, const std::vector<KZPlayer *> &candidates, KZPlayer *selected, PickCallback onPick)
	{
		this->titlePhrase = titlePhrase;
		this->onPick = std::move(onPick);
		this->page = 0;
		this->rows.clear();
		for (KZPlayer *candidate : candidates)
		{
			this->rows.push_back({candidate->GetClient()->GetUserID(), candidate->GetName(), candidate == selected});
		}
		std::sort(this->rows.begin(), this->rows.end(), [](const Row &a, const Row &b) { return V_stricmp(a.name.c_str(), b.name.c_str()) < 0; });
		if (this->player->uiService->GetTop() == this && this->layout.Get())
		{
			this->Render();
			return true;
		}
		return this->player->uiService->Open(this);
	}

	bool PlayerPicker::CanOpen()
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

	void PlayerPicker::OnOpen()
	{
		this->Render();
	}

	void PlayerPicker::Hide()
	{
		if (CCSCustomHudLayout *entity = this->layout.Get())
		{
			SetBoolClass(entity, "picker_root", "hidden", this->applied.rootHidden, true);
		}
	}

	void PlayerPicker::OnClose(CloseReason reason)
	{
		this->Hide();
	}

	void PlayerPicker::OnSuspend()
	{
		this->Hide();
	}

	void PlayerPicker::OnResume()
	{
		this->Render();
	}

	void PlayerPicker::Render()
	{
		CCSCustomHudLayout *entity = this->layout.Get();
		if (!entity || this->player->uiService->GetTop() != this)
		{
			return;
		}
		KZLanguageService *language = this->player->languageService;
		SetBoolClass(entity, "picker_root", "hidden", this->applied.rootHidden, false);
		ApplyWindowStyle(this->player, entity, "picker_root", this->applied.style);
		this->layout.SetVar("picker_title", "picker_title", language->PrepareMessage(this->titlePhrase).c_str());

		const i32 pages = MAX(1, ((i32)this->rows.size() + KZ_PICKER_ROWS - 1) / KZ_PICKER_ROWS);
		this->page = MIN(MAX(this->page, 0), pages - 1);
		for (i32 i = 0; i < KZ_PICKER_ROWS; i++)
		{
			const i32 index = this->page * KZ_PICKER_ROWS + i;
			const bool shown = index < (i32)this->rows.size();
			SetBoolClass(entity, RowButton(i), "hidden", this->applied.rowHidden[i], !shown);
			if (!shown)
			{
				continue;
			}
			this->layout.SetVar(RowName(i), RowName(i), this->rows[index].name.c_str());
			SetBoolClass(entity, RowButton(i), "selected", this->applied.rowSelected[i], this->rows[index].selected);
		}

		SetBoolClass(entity, "picker_nav", "hidden", this->applied.navHidden, pages < 2);
		char pageText[16];
		V_snprintf(pageText, sizeof(pageText), "%i/%i", this->page + 1, pages);
		this->layout.SetVar("picker_page", "picker_page", pageText);

		SetBoolClass(entity, "picker_note", "hidden", this->applied.noteHidden, !this->rows.empty());
		this->layout.SetVar("picker_note", "picker_note", language->PrepareMessage("Picker - No Players").c_str());
	}

	void PlayerPicker::OnClick(const char *buttonId)
	{
		if (KZ_STREQ(buttonId, "picker_close"))
		{
			this->player->uiService->Close(this);
			return;
		}
		if (KZ_STREQ(buttonId, "picker_prev") || KZ_STREQ(buttonId, "picker_next"))
		{
			this->page += KZ_STREQ(buttonId, "picker_next") ? 1 : -1;
			this->Render();
			return;
		}
		for (i32 i = 0; i < KZ_PICKER_ROWS; i++)
		{
			if (!KZ_STREQ(buttonId, RowButton(i)))
			{
				continue;
			}
			const i32 index = this->page * KZ_PICKER_ROWS + i;
			if (index >= (i32)this->rows.size())
			{
				return;
			}
			KZPlayer *target = g_pKZPlayerManager->ToPlayer(this->rows[index].userID);
			// Copied because the callback may show the picker again.
			const PickCallback onPick = this->onPick;
			const std::string name = this->rows[index].name;
			this->player->uiService->Close(this);
			if (!target)
			{
				this->player->languageService->PrintChat(true, false, "Picker - Player Left", name.c_str());
				return;
			}
			if (onPick)
			{
				onPick(this->player, target);
			}
			return;
		}
	}
} // namespace KZ::ui
