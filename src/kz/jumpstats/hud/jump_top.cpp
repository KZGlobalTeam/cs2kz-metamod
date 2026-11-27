#include "kz/jumpstats/hud/jump_top.h"
#include "kz/db/kz_db.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/profile/hud/profile.h"
#include "kz/replays/commands.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/replays/watcher.h"
#include "kz/timer/hud/record_format.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "tier0/memdbgon.h"

extern ReplayWatcher g_ReplayWatcher;

#define SLOT_ID(fn, fmt) \
	static_function const char *fn(i32 i) \
	{ \
		static_persist char buf[48]; \
		V_snprintf(buf, sizeof(buf), fmt, i); \
		return buf; \
	}

SLOT_ID(ModeButton, "jumptop_mode_%i")
SLOT_ID(ListButton, "jumptop_list_%i")
SLOT_ID(ListLabel, "jumptop_list_%i_label")
SLOT_ID(TypeButton, "jumptop_jt_%i")
SLOT_ID(RowPanel, "jumptop_row%i")
SLOT_ID(RowNumber, "jumptop_row%i_num")
SLOT_ID(RowProfile, "jumptop_row%i_profile")
SLOT_ID(RowPlayer, "jumptop_row%i_player")
SLOT_ID(RowPlay, "jumptop_row%i_play")
SLOT_ID(RowPlayTip, "jumptop_row%i_play_tip")
#undef SLOT_ID

static_function const char *RowCell(i32 row, const char *cell)
{
	static_persist char buf[48];
	V_snprintf(buf, sizeof(buf), "jumptop_row%i_%s", row, cell);
	return buf;
}

// The columns after the player in jump_top.xml order and the phrase of each header.
static_global const char *const cells[][2] = {
	{"distance", "Replay Table Header - Distance"},
	{"block", "Replay Table Header - Block"},
	{"strafes", "Replay Table Header - Strafes"},
	{"sync", "Jump Top - Sync"},
	{"pre", "Jump Top - Pre"},
	{"max", "Jump Top - Max"},
	{"airtime", "Jump Top - Airtime"},
	{"map", "Replay Table Header - Map"},
};

namespace KZ::jumpstats::hud
{
	bool JumpTopWindow::CanOpen()
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

	bool JumpTopWindow::Show(i32 typeIndex, i32 modeIndex, bool block)
	{
		this->typeIndex = MIN(MAX(typeIndex, 0), KZ_JUMP_TOP_TYPES - 1);
		this->modeIndex = MIN(MAX(modeIndex, 0), 1);
		this->block = block;
		this->offset = 0;
		this->Query();
		if (this->player->uiService->GetTop() == this && this->layout.Get())
		{
			this->Render();
			return true;
		}
		return this->player->uiService->Open(this);
	}

	void JumpTopWindow::Query()
	{
		const u32 serial = ++this->serial;
		this->state = State::Loading;
		this->rows.clear();
		const i32 modeID = KZ::mode::GetModeInfo(KZ::api::GetModeShortName(KZ::api::modes[this->modeIndex])).databaseID;
		if (!KZDatabaseService::IsReady() || modeID < 0)
		{
			// A mode the server doesn't have has no jumps saved in it.
			this->total = 0;
			this->state = KZDatabaseService::IsReady() ? State::Loaded : State::Failed;
			return;
		}
		const CPlayerUserId userID = this->player->GetClient()->GetUserID();
		auto onSuccess = [userID, serial](std::vector<ISQLQuery *> queries)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			JumpTopWindow *window = player ? player->uiService->GetWindow<JumpTopWindow>() : nullptr;
			if (!window || window->serial != serial)
			{
				return;
			}
			const std::string currentMap = g_pKZUtils->GetCurrentMapName();
			ISQLResult *result = queries[0]->GetResultSet();
			while (result && result->FetchRow())
			{
				Row row;
				row.steamId = (u64)result->GetInt64(1);
				row.name = utils::OrEmpty(result->GetString(2));
				row.distance = result->GetInt64(3) / KZ_JUMPSTATS_DISTANCE_SCALE;
				row.block = result->GetInt64(4) ? (u32)result->GetInt64(5) : 0;
				row.strafes = (u32)result->GetInt64(6);
				row.sync = result->GetInt64(7) / KZ_JUMPSTATS_SYNC_SCALE;
				row.pre = result->GetInt64(8) / KZ_JUMPSTATS_SPEED_SCALE;
				row.max = result->GetInt64(9) / KZ_JUMPSTATS_SPEED_SCALE;
				row.airtime = result->GetInt64(10) / KZ_JUMPSTATS_AIRTIME_SCALE;
				row.uuid = result->IsNull(11) ? "" : utils::OrEmpty(result->GetString(11));
				const std::string replayMap = row.uuid.empty() ? std::string() : g_ReplayWatcher.GetJumpReplayMap(UUID_t(row.uuid.c_str()));
				row.map = replayMap;
				row.hasReplay = !replayMap.empty();
				row.playable = row.hasReplay && replayMap == currentMap;
				window->rows.push_back(row);
			}
			result = queries.size() > 1 ? queries[1]->GetResultSet() : nullptr;
			window->total = result && result->FetchRow() ? (u64)result->GetInt64(0) : window->rows.size();
			window->state = State::Loaded;
			window->Render();

			// Replays that aren't on disk may be in the replay store.
			std::vector<std::pair<UUID_t, std::string>> missing;
			for (const Row &row : window->rows)
			{
				UUID_t uuid(false);
				if (!row.hasReplay && UUID_t::FromString(row.uuid.c_str(), &uuid))
				{
					missing.push_back({uuid, std::string()});
				}
			}
			auto onLocated = [userID, serial](std::unordered_map<UUID_t, KZ::replaysystem::ReplayAvailability> &&replays)
			{
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
				JumpTopWindow *window = player ? player->uiService->GetWindow<JumpTopWindow>() : nullptr;
				if (!window || window->serial != serial)
				{
					return;
				}
				const std::string currentMap = g_pKZUtils->GetCurrentMapName().c_str();
				bool changed = false;
				for (Row &row : window->rows)
				{
					UUID_t uuid(false);
					auto it = !row.hasReplay && UUID_t::FromString(row.uuid.c_str(), &uuid) ? replays.find(uuid) : replays.end();
					if (it == replays.end() || !it->second.found)
					{
						continue;
					}
					// The store only said that the replay exists and it was checked under the current map.
					row.map = it->second.map.empty() ? currentMap : it->second.map;
					row.hasReplay = true;
					row.playable = row.map == currentMap;
					changed = true;
				}
				if (changed)
				{
					window->Render();
				}
			};
			if (!missing.empty())
			{
				KZ::replaysystem::LocateReplays(missing, onLocated);
			}
		};
		auto onFailure = [userID, serial](std::string, int)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			JumpTopWindow *window = player ? player->uiService->GetWindow<JumpTopWindow>() : nullptr;
			if (window && window->serial == serial)
			{
				window->state = State::Failed;
				window->Render();
			}
		};
		KZDatabaseService::QueryJumpTop(JumpType_LongJump + this->typeIndex, modeID, this->block, KZ_JUMP_TOP_ROWS, this->offset, onSuccess,
										onFailure);
	}

	void JumpTopWindow::OnOpen()
	{
		this->Render();
	}

	void JumpTopWindow::Hide()
	{
		if (CCSCustomHudLayout *entity = this->layout.Get())
		{
			KZ::ui::SetBoolClass(entity, "jumptop_root", "hidden", this->applied.rootHidden, true);
		}
	}

	void JumpTopWindow::OnClose(KZ::ui::CloseReason reason)
	{
		this->Hide();
	}

	void JumpTopWindow::OnSuspend()
	{
		this->Hide();
	}

	void JumpTopWindow::OnResume()
	{
		this->Render();
	}

	void JumpTopWindow::Render()
	{
		CCSCustomHudLayout *entity = this->layout.Get();
		if (!entity || this->player->uiService->GetTop() != this)
		{
			return;
		}
		auto *language = this->player->languageService;
		KZ::ui::SetBoolClass(entity, "jumptop_root", "hidden", this->applied.rootHidden, false);
		KZ::ui::ApplyWindowStyle(this->player, entity, "jumptop_root", this->applied.style);
		this->layout.SetVar("jumptop_window_title", "jumptop_window_title", language->PrepareMessage("Jump Top - Window Title").c_str());

		for (i32 i = 0; i < 2; i++)
		{
			KZ::ui::SetBoolClass(entity, ModeButton(i), "selected", this->applied.modeSelected[i], i == this->modeIndex);
			KZ::ui::SetBoolClass(entity, ListButton(i), "selected", this->applied.listSelected[i], i == (i32)this->block);
		}
		KZ::ui::SetSwitchPick(entity, "jumptop_mode", this->applied.modePick, this->modeIndex);
		KZ::ui::SetSwitchPick(entity, "jumptop_list", this->applied.listPick, this->block);
		this->layout.SetVar(ListLabel(0), ListLabel(0), language->PrepareMessage("Replay Table Header - Distance").c_str());
		this->layout.SetVar(ListLabel(1), ListLabel(1), language->PrepareMessage("Replay Table Header - Block").c_str());
		for (i32 i = 0; i < KZ_JUMP_TOP_TYPES; i++)
		{
			KZ::ui::SetBoolClass(entity, TypeButton(i), "selected", this->applied.typeSelected[i], i == this->typeIndex);
		}
		KZ::ui::SetSwitchPick(entity, "jumptop_jt", this->applied.typePick, this->typeIndex);

		this->layout.SetVar("jumptop_th_player", "jumptop_th_player", language->PrepareMessage("Replay Table Header - Player").c_str());
		for (const auto &cell : cells)
		{
			const std::string id = std::string("jumptop_th_") + cell[0];
			this->layout.SetVar(id.c_str(), id.c_str(), language->PrepareMessage(cell[1]).c_str());
		}
		this->layout.SetVar("jumptop_th_replay", "jumptop_th_replay", language->PrepareMessage("Course Top - Window Replay").c_str());

		const std::string playTip = language->PrepareMessage("Course Top - Window Watch Replay");
		const std::string noReplayTip = language->PrepareMessage("Course Top - Window No Replay");
		const std::string otherMapTip = language->PrepareMessage("Course Top - Window Other Map");
		const bool profiles = ProfileWindow::IsAvailable();
		for (i32 i = 0; i < KZ_JUMP_TOP_ROWS; i++)
		{
			const bool shown = i < (i32)this->rows.size();
			KZ::ui::SetBoolClass(entity, RowPanel(i), "hidden", this->applied.rowHidden[i], !shown);
			if (!shown)
			{
				continue;
			}
			const Row &row = this->rows[i];
			this->layout.SetVar(RowNumber(i), RowNumber(i), tfm::format("#%llu", this->offset + i + 1).c_str());
			this->layout.SetVar(RowPlayer(i), RowPlayer(i), row.name.c_str());
			KZ::ui::SetBoolClass(entity, RowProfile(i), "static", this->applied.rowProfileStatic[i], !profiles);
			const std::string values[] = {
				tfm::format("%.4f", row.distance), row.block > 0 ? std::to_string(row.block) : "-",
				std::to_string(row.strafes),       tfm::format("%.1f%%", row.sync),
				tfm::format("%.2f", row.pre),      tfm::format("%.2f", row.max),
				tfm::format("%.3f", row.airtime),  row.hasReplay ? row.map : "-",
			};
			for (u32 c = 0; c < KZ_ARRAYSIZE(cells); c++)
			{
				const char *id = RowCell(i, cells[c][0]);
				this->layout.SetVar(id, id, values[c].c_str());
			}
			KZ::ui::SetBoolClass(entity, RowPlay(i), "disabled", this->applied.rowPlayDisabled[i], !row.playable);
			const std::string &tip = row.playable ? playTip : (row.hasReplay ? otherMapTip : noReplayTip);
			this->layout.SetVar(RowPlayTip(i), RowPlayTip(i), tip.c_str());
		}

		const char *status = nullptr;
		if (this->state == State::Loading)
		{
			status = "Course Top - Window Loading";
		}
		else if (this->state == State::Failed)
		{
			status = "Jump Top - Window Unavailable";
		}
		else if (this->rows.empty())
		{
			status = "Jump Top - Window None";
		}
		KZ::ui::SetBoolClass(entity, "jumptop_status", "hidden", this->applied.statusHidden, !status);
		this->layout.SetVar("jumptop_status", "jumptop_status", status ? language->PrepareMessage(status).c_str() : "");

		std::string count;
		if (this->state == State::Loaded && !this->rows.empty())
		{
			const std::string first = KZ::records::FormatThousands((f64)(this->offset + 1));
			const std::string last = KZ::records::FormatThousands((f64)(this->offset + this->rows.size()));
			count = language->PrepareMessage("Course Top - Window Count", first.c_str(), last.c_str(),
											 KZ::records::FormatThousands((f64)this->total).c_str());
		}
		this->layout.SetVar("jumptop_count", "jumptop_count", count.c_str());
		this->layout.SetVar("jumptop_prev_label", "jumptop_prev_label", language->PrepareMessage("Course Top - Window Previous").c_str());
		this->layout.SetVar("jumptop_next_label", "jumptop_next_label", language->PrepareMessage("Course Top - Window Next").c_str());
		KZ::ui::SetBoolClass(entity, "jumptop_prev", "disabled", this->applied.prevDisabled, this->offset == 0);
		KZ::ui::SetBoolClass(entity, "jumptop_next", "disabled", this->applied.nextDisabled, this->offset + KZ_JUMP_TOP_ROWS >= this->total);
	}

	void JumpTopWindow::OnClick(const char *buttonId)
	{
		if (KZ_STREQ(buttonId, "jumptop_window_close"))
		{
			this->player->uiService->Close(this);
			return;
		}
		for (i32 i = 0; i < 2; i++)
		{
			if (KZ_STREQ(buttonId, ModeButton(i)) || KZ_STREQ(buttonId, ListButton(i)))
			{
				const bool mode = KZ_STREQ(buttonId, ModeButton(i));
				this->Show(this->typeIndex, mode ? i : this->modeIndex, mode ? this->block : i == 1);
				return;
			}
		}
		for (i32 i = 0; i < KZ_JUMP_TOP_TYPES; i++)
		{
			if (KZ_STREQ(buttonId, TypeButton(i)))
			{
				this->Show(i, this->modeIndex, this->block);
				return;
			}
		}
		const bool next = KZ_STREQ(buttonId, "jumptop_next");
		if (next || KZ_STREQ(buttonId, "jumptop_prev"))
		{
			if (next ? this->offset + KZ_JUMP_TOP_ROWS >= this->total : this->offset == 0)
			{
				return;
			}
			this->offset = next ? this->offset + KZ_JUMP_TOP_ROWS : (this->offset > KZ_JUMP_TOP_ROWS ? this->offset - KZ_JUMP_TOP_ROWS : 0);
			this->Query();
			this->Render();
			return;
		}
		for (i32 i = 0; i < (i32)this->rows.size(); i++)
		{
			const Row &row = this->rows[i];
			if (KZ_STREQ(buttonId, RowProfile(i)))
			{
				if (ProfileWindow::IsAvailable())
				{
					this->player->uiService->GetWindow<ProfileWindow>()->ShowSteamId(row.steamId, KZ::api::modes[this->modeIndex], true);
				}
				return;
			}
			if (KZ_STREQ(buttonId, RowPlay(i)))
			{
				if (row.playable)
				{
					const std::string uuid = row.uuid;
					this->player->uiService->CloseAll(KZ::ui::CloseReason::Closed);
					KZ::replaysystem::commands::LoadReplay(this->player, uuid.c_str());
				}
				return;
			}
		}
	}
} // namespace KZ::jumpstats::hud
