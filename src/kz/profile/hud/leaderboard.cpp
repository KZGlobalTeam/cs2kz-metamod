#include "kz/profile/hud/leaderboard.h"
#include "kz/global/kz_global.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/profile/kz_profile.h"
#include "kz/profile/hud/profile.h"
#include "kz/timer/hud/record_format.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/http.h"
#include "utils/json.h"
#include "utils/simplecmds.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

#define SLOT_ID(fn, fmt) \
	static_function const char *fn(i32 i) \
	{ \
		static_persist char buf[48]; \
		V_snprintf(buf, sizeof(buf), fmt, i); \
		return buf; \
	}

SLOT_ID(ModeButton, "leaderboard_mode_%i")
SLOT_ID(RowPanel, "leaderboard_row%i")
SLOT_ID(RowNumber, "leaderboard_row%i_num")
SLOT_ID(RowProfile, "leaderboard_row%i_profile")
SLOT_ID(RowPlayer, "leaderboard_row%i_player")
SLOT_ID(RowRating, "leaderboard_row%i_rating")
SLOT_ID(RowTitle, "leaderboard_row%i_title")
#undef SLOT_ID

bool LeaderboardWindow::Entry::FromJson(const Json &json)
{
	std::string id;
	return json.Get("id", id) && utils::ParseSteamID2(id, this->steamId) && json.Get("name", this->name) && json.Get("ckz_rating", this->rating[0])
		   && json.Get("vnl_rating", this->rating[1]);
}

bool LeaderboardWindow::Show(KZ::api::Mode mode)
{
	this->mode = mode;
	this->offset = 0;
	this->Request();
	if (this->player->uiService->GetTop() == this && this->layout.Get())
	{
		this->Render();
		return true;
	}
	return this->player->uiService->Open(this);
}

bool LeaderboardWindow::CanOpen()
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

void LeaderboardWindow::Request()
{
	const u32 serial = ++this->serial;
	this->state = State::Loading;
	this->entries.clear();
	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	HTTP::Request request(HTTP::Method::GET, KZ::api::GetBaseUrl() + "/players");
	request.SetQuery("sort_by", this->mode == KZ::api::Mode::Vanilla ? "vnl-rating" : "ckz-rating");
	request.SetQuery("limit", std::to_string(KZ_LEADERBOARD_ROWS));
	request.SetQuery("offset", std::to_string(this->offset));
	auto onResponse = [userID, serial](HTTP::Response response)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		LeaderboardWindow *window = player ? player->uiService->GetWindow<LeaderboardWindow>() : nullptr;
		if (!window || window->serial != serial)
		{
			return;
		}
		Json json(response.Body().value_or(""));
		std::vector<Entry> entries;
		u64 total = 0;
		if (response.status != 200 || !json.IsValid() || !json.Get("values", entries) || !json.Get("total", total))
		{
			window->state = State::Failed;
			window->Render();
			return;
		}
		window->entries = std::move(entries);
		window->total = total;
		window->state = State::Loaded;
		window->Render();
	};
	auto onError = [userID, serial]()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		LeaderboardWindow *window = player ? player->uiService->GetWindow<LeaderboardWindow>() : nullptr;
		if (window && window->serial == serial)
		{
			window->state = State::Failed;
			window->Render();
		}
	};
	request.Send(onResponse, onError);
}

void LeaderboardWindow::OnOpen()
{
	this->Render();
}

void LeaderboardWindow::Hide()
{
	if (CCSCustomHudLayout *entity = this->layout.Get())
	{
		KZ::ui::SetBoolClass(entity, "leaderboard_root", "hidden", this->applied.rootHidden, true);
	}
}

void LeaderboardWindow::OnClose(KZ::ui::CloseReason reason)
{
	this->Hide();
}

void LeaderboardWindow::OnSuspend()
{
	this->Hide();
}

void LeaderboardWindow::OnResume()
{
	this->Render();
}

void LeaderboardWindow::Render()
{
	CCSCustomHudLayout *entity = this->layout.Get();
	if (!entity || this->player->uiService->GetTop() != this)
	{
		return;
	}
	KZLanguageService *language = this->player->languageService;
	KZ::ui::SetBoolClass(entity, "leaderboard_root", "hidden", this->applied.rootHidden, false);
	KZ::ui::ApplyWindowStyle(this->player, entity, "leaderboard_root", this->applied.style);
	this->layout.SetVar("leaderboard_window_title", "leaderboard_window_title", language->PrepareMessage("Leaderboard - Window Title").c_str());

	const i32 modeIndex = KZ::api::GetModeIndex(this->mode);
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(KZ::api::modes); i++)
	{
		KZ::ui::SetBoolClass(entity, ModeButton(i), "selected", this->applied.modeSelected[i], i == modeIndex);
	}
	KZ::ui::SetSwitchPick(entity, "leaderboard_mode", this->applied.modePick, modeIndex);

	this->layout.SetVar("leaderboard_th_player", "leaderboard_th_player", language->PrepareMessage("Course Top - Window Player").c_str());
	this->layout.SetVar("leaderboard_th_rating", "leaderboard_th_rating", language->PrepareMessage("Leaderboard - Rating").c_str());
	this->layout.SetVar("leaderboard_th_title", "leaderboard_th_title", language->PrepareMessage("Leaderboard - Title").c_str());

	for (i32 i = 0; i < KZ_LEADERBOARD_ROWS; i++)
	{
		const bool shown = i < (i32)this->entries.size();
		KZ::ui::SetBoolClass(entity, RowPanel(i), "hidden", this->applied.rowHidden[i], !shown);
		if (!shown)
		{
			continue;
		}
		const Entry &entry = this->entries[i];
		const f64 rating = entry.rating[modeIndex];
		const KZ::profile::RankBand &band = KZ::profile::GetRankBand(rating);
		this->layout.SetVar(RowNumber(i), RowNumber(i), tfm::format("#%llu", this->offset + i + 1).c_str());
		this->layout.SetVar(RowPlayer(i), RowPlayer(i), entry.name.c_str());
		this->layout.SetVar(RowRating(i), RowRating(i), KZ::records::FormatThousands(rating).c_str());
		KZ::ui::SetSwapClass(entity, RowRating(i), this->applied.ratingClass[i], band.className);
		this->layout.SetVar(RowTitle(i), RowTitle(i), band.name);
		KZ::ui::SetSwapClass(entity, RowTitle(i), this->applied.titleClass[i], band.className);
	}

	const char *status = nullptr;
	if (this->state == State::Loading)
	{
		status = "Course Top - Window Loading";
	}
	else if (this->state == State::Failed)
	{
		status = "Leaderboard - Window Failed";
	}
	else if (this->entries.empty())
	{
		status = "Leaderboard - Window None";
	}
	KZ::ui::SetBoolClass(entity, "leaderboard_status", "hidden", this->applied.statusHidden, !status);
	this->layout.SetVar("leaderboard_status", "leaderboard_status", status ? language->PrepareMessage(status).c_str() : "");

	std::string count;
	if (this->state == State::Loaded && !this->entries.empty())
	{
		const std::string first = KZ::records::FormatThousands((f64)(this->offset + 1));
		const std::string last = KZ::records::FormatThousands((f64)(this->offset + this->entries.size()));
		count = language->PrepareMessage("Course Top - Window Count", first.c_str(), last.c_str(),
										 KZ::records::FormatThousands((f64)this->total).c_str());
	}
	this->layout.SetVar("leaderboard_count", "leaderboard_count", count.c_str());
	this->layout.SetVar("leaderboard_prev_label", "leaderboard_prev_label", language->PrepareMessage("Course Top - Window Previous").c_str());
	this->layout.SetVar("leaderboard_next_label", "leaderboard_next_label", language->PrepareMessage("Course Top - Window Next").c_str());
	KZ::ui::SetBoolClass(entity, "leaderboard_prev", "disabled", this->applied.prevDisabled, this->offset == 0);
	KZ::ui::SetBoolClass(entity, "leaderboard_next", "disabled", this->applied.nextDisabled, this->offset + KZ_LEADERBOARD_ROWS >= this->total);
}

void LeaderboardWindow::OnClick(const char *buttonId)
{
	if (KZ_STREQ(buttonId, "leaderboard_window_close"))
	{
		this->player->uiService->Close(this);
		return;
	}
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(KZ::api::modes); i++)
	{
		if (KZ_STREQ(buttonId, ModeButton(i)))
		{
			this->Show(KZ::api::modes[i]);
			return;
		}
	}
	const bool next = KZ_STREQ(buttonId, "leaderboard_next");
	if (next || KZ_STREQ(buttonId, "leaderboard_prev"))
	{
		if (next ? this->offset + KZ_LEADERBOARD_ROWS >= this->total : this->offset == 0)
		{
			return;
		}
		this->offset = next ? this->offset + KZ_LEADERBOARD_ROWS : (this->offset > KZ_LEADERBOARD_ROWS ? this->offset - KZ_LEADERBOARD_ROWS : 0);
		this->Request();
		this->Render();
		return;
	}
	for (i32 i = 0; i < (i32)this->entries.size(); i++)
	{
		if (KZ_STREQ(buttonId, RowProfile(i)))
		{
			this->player->uiService->GetWindow<ProfileWindow>()->ShowSteamId(this->entries[i].steamId, this->mode);
			return;
		}
	}
}

SCMD(kz_top, SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!KZGlobalService::IsAvailable())
	{
		player->languageService->PrintChat(true, false, "Leaderboard - Unavailable");
		return true;
	}
	// The shown mode is the caller's unless they name one.
	KZ::api::Mode mode = KZ::api::Mode::Classic;
	KZ::api::DecodeModeString(KZ::mode::GetModeInfo(player->modeService).shortModeName, mode);
	if (args->ArgC() >= 2)
	{
		KZ::api::DecodeModeString(args->Arg(1), mode);
	}
	if (!player->uiService->GetWindow<LeaderboardWindow>()->Show(mode))
	{
		player->languageService->PrintChat(true, false, "Leaderboard - Window Unavailable");
	}
	return true;
}
