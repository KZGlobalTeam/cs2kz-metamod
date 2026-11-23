#include "kz/replays/hud/replays.h"
#include "kz/replays/commands.h"
#include "kz/replays/watcher.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/profile/hud/profile.h"
#include "kz/timer/hud/record_format.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"

#include <algorithm>

#include "tier0/memdbgon.h"

extern ReplayWatcher g_ReplayWatcher;

using KZ::records::FormatRankingTime;
using KZ::records::FormatThousands;

#define SLOT_ID(fn, fmt) \
	static_function const char *fn(i32 i) \
	{ \
		static_persist char buf[48]; \
		V_snprintf(buf, sizeof(buf), fmt, i); \
		return buf; \
	}

SLOT_ID(TypeButton, "replays_type_%i")
SLOT_ID(TypeLabel, "replays_type_%i_label")
SLOT_ID(ModeButton, "replays_mode_%i")
SLOT_ID(JumpTypeButton, "replays_jt_%i")
SLOT_ID(HeadCell, "replays_th_c%i")
SLOT_ID(RowPanel, "replays_row%i")
SLOT_ID(RowNumber, "replays_row%i_num")
SLOT_ID(RowPlayer, "replays_row%i_player")
SLOT_ID(RowProfile, "replays_row%i_profile")
SLOT_ID(RowMap, "replays_row%i_map")
SLOT_ID(RowTime, "replays_row%i_time")
SLOT_ID(RowBadge, "replays_row%i_badge")
SLOT_ID(RowPlay, "replays_row%i_play")
SLOT_ID(RowPlayTip, "replays_row%i_play_tip")
SLOT_ID(RowCopy, "replays_row%i_copy")
SLOT_ID(RowCopyTip, "replays_row%i_copy_tip")
#undef SLOT_ID

static_function const char *RowCell(i32 row, i32 cell)
{
	static_persist char buf[48];
	V_snprintf(buf, sizeof(buf), "replays_row%i_c%i", row, cell);
	return buf;
}

namespace KZ::replaysystem::hud
{
	// The lists in switch order with the header phrase of each generic cell.
	// Runs also show the time column.
	struct ListDef
	{
		ReplayType type;
		const char *listClass;
		const char *label;
		const char *cells[KZ_REPLAYS_CELLS];
	};

	// The mode and jump type get no column because they have switches.
	static_global const ListDef lists[] = {
		{RP_RUN,
		 "list-run",
		 "Replays - Window Runs",
		 {"Replay Table Header - Course", "Replay Table Header - Styles", nullptr, "Replays - Window TPs"}},
		{RP_JUMPSTATS,
		 "list-jump",
		 "Replays - Window Jumps",
		 {"Replay Table Header - Distance", "Replay Table Header - Block", "Replay Table Header - Strafes"}},
		{RP_MANUAL, "list-manual", "Replays - Window Manual", {"Replay Table Header - Saved By"}},
	};

	// Returns -1 for modes missing from the switch.
	static_function i32 GetModeIndex(const std::string &mode)
	{
		KZ::api::Mode apiMode {};
		if (!KZ::api::DecodeModeString(mode, apiMode))
		{
			return -1;
		}
		return KZ::api::GetModeIndex(apiMode);
	}

	// Returns -1 for types the window doesn't list.
	static_function i32 GetListIndex(ReplayType type)
	{
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(lists); i++)
		{
			if (lists[i].type == type)
			{
				return i;
			}
		}
		return -1;
	}

	static_function std::string Join(const std::vector<std::string> &parts, const char *separator)
	{
		std::string out;
		for (const std::string &part : parts)
		{
			out += out.empty() ? part : separator + part;
		}
		return out;
	}

	static_function std::string GetModeName(const cs2kz::replay::ModeStyleInfo &mode)
	{
		return mode.short_name().empty() ? mode.name() : mode.short_name();
	}

	// Writes the active filters as !replays arguments so the player knows what to type to change them.
	static_function std::string FormatFilters(const ReplayFilterCriteria &criteria)
	{
		std::vector<std::string> args;
		auto add = [&](const char *key, const std::string &value) { args.push_back(tfm::format("%s=%s", key, value)); };
		// The mode is only written out if the switch can't show it.
		auto addMode = [&]()
		{
			if (GetModeIndex(criteria.modeNameSubString) < 0)
			{
				add("mode", criteria.modeNameSubString);
			}
		};
		if (criteria.player.nameSubString)
		{
			add("player", *criteria.player.nameSubString);
		}
		if (criteria.player.steamID)
		{
			add("steamid", std::to_string(*criteria.player.steamID));
		}
		add("map", criteria.mapName);
		switch (criteria.type)
		{
			case RP_RUN:
				addMode();
				if (criteria.courseName)
				{
					add("course", *criteria.courseName);
				}
				if (criteria.maxTime >= 0.0f)
				{
					add("maxtime", tfm::format("%g", criteria.maxTime));
				}
				if (criteria.maxTeleports >= 0)
				{
					add("maxteleports", std::to_string(criteria.maxTeleports));
				}
				if (criteria.numStyles >= 0)
				{
					add("numstyles", std::to_string(criteria.numStyles));
				}
				break;
			// The jump type has its own switch.
			case RP_JUMPSTATS:
				addMode();
				if (criteria.minDistance > 0.0f)
				{
					add("mindistance", tfm::format("%g", criteria.minDistance));
				}
				break;
			case RP_MANUAL:
				if (criteria.savedBy.nameSubString)
				{
					add("saver", *criteria.savedBy.nameSubString);
				}
				if (criteria.savedBy.steamID)
				{
					add("ssid", std::to_string(*criteria.savedBy.steamID));
				}
				break;
			default:
				break;
		}
		return Join(args, "  ");
	}

	bool ReplaysWindow::CanOpen()
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

	bool ReplaysWindow::Show(const char *args)
	{
		this->args = args ? args : "";
		const ReplayFilterCriteria criteria = ReplayWatcher::ParseCriteria(this->args.c_str(), this->player);
		// Defaults to runs unless the arguments name another list the window has.
		this->type = GetListIndex(criteria.type) < 0 ? RP_RUN : criteria.type;
		this->jumpType = criteria.jumpType;
		// The mode is parsed with the run defaults because manual saves have no mode.
		this->mode = ReplayWatcher::ParseCriteria(this->args.c_str(), this->player, RP_RUN).modeNameSubString;
		this->offset = criteria.offset > 0 ? criteria.offset : 0;
		this->Query();
		return this->player->uiService->Open(this);
	}

	void ReplaysWindow::Query()
	{
		ReplayFilterCriteria criteria = ReplayWatcher::ParseCriteria(this->args.c_str(), this->player, this->type);
		criteria.jumpType = this->jumpType;
		if (this->type != RP_MANUAL)
		{
			criteria.modeNameSubString = this->mode;
		}
		this->filters = FormatFilters(criteria);
		const std::vector<std::pair<UUID_t, ReplayHeader>> matches = g_ReplayWatcher.CollectMatches(criteria);
		this->total = matches.size();
		// An offset past the end shows the last page.
		if (this->offset >= this->total)
		{
			this->offset = this->total ? (this->total - 1) / KZ_REPLAYS_ROWS * KZ_REPLAYS_ROWS : 0;
		}

		this->rows.clear();
		const std::string currentMap = g_pKZUtils->GetCurrentMapName();
		for (u64 i = this->offset; i < this->total && this->rows.size() < KZ_REPLAYS_ROWS; i++)
		{
			const ReplayHeader &header = matches[i].second;
			Row row;
			row.uuid = matches[i].first.ToString();
			row.name = header.player().name();
			row.steamId = header.player().steamid64();
			row.map = header.map().name();
			row.playable = KZ_STREQI(row.map.c_str(), currentMap.c_str());
			switch (this->type)
			{
				case RP_RUN:
				{
					const auto &run = header.run();
					row.cells[0] = run.course_name();
					std::vector<std::string> styles;
					for (const auto &style : run.styles())
					{
						const std::string &name = style.short_name().empty() ? style.name() : style.short_name();
						if (!name.empty())
						{
							styles.push_back(name);
						}
					}
					row.cells[1] = styles.empty() ? "-" : Join(styles, ", ");
					row.cells[3] = std::to_string(run.num_teleports());
					row.time = FormatRankingTime(run.time());
					row.badge = run.num_teleports() > 0 ? "tone-tp" : "tone-pro";
					KZ::api::DecodeModeString(GetModeName(run.mode()), row.mode);
					break;
				}
				case RP_JUMPSTATS:
				{
					const auto &jump = header.jump();
					row.cells[0] = tfm::format("%.4f", jump.distance());
					row.cells[1] = jump.block_distance() > 0 ? std::to_string(jump.block_distance()) : "-";
					row.cells[2] = std::to_string(jump.num_strafes());
					KZ::api::DecodeModeString(GetModeName(jump.mode()), row.mode);
					break;
				}
				case RP_MANUAL:
					row.cells[0] = header.manual().saved_by().name();
					break;
				default:
					break;
			}
			this->rows.push_back(std::move(row));
		}
	}

	void ReplaysWindow::OnOpen()
	{
		this->Render();
	}

	void ReplaysWindow::Hide()
	{
		if (CCSCustomHudLayout *entity = this->layout.Get())
		{
			KZ::ui::SetBoolClass(entity, "replays_root", "hidden", this->applied.rootHidden, true);
		}
	}

	void ReplaysWindow::OnClose(KZ::ui::CloseReason reason)
	{
		this->Hide();
	}

	void ReplaysWindow::OnSuspend()
	{
		this->Hide();
	}

	void ReplaysWindow::OnResume()
	{
		// The server may have saved replays since.
		this->Query();
		this->Render();
	}

	void ReplaysWindow::Render()
	{
		CCSCustomHudLayout *entity = this->layout.Get();
		if (!entity || this->player->uiService->GetTop() != this)
		{
			return;
		}
		auto *language = this->player->languageService;
		KZ::ui::SetBoolClass(entity, "replays_root", "hidden", this->applied.rootHidden, false);
		KZ::ui::ApplyWindowStyle(this->player, entity, "replays_root", this->applied.style);
		this->layout.SetVar("replays_window_title", "replays_window_title", language->PrepareMessage("Replays - Window Title").c_str());

		const i32 list = GetListIndex(this->type);
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(lists); i++)
		{
			KZ::ui::SetBoolClass(entity, TypeButton(i), "selected", this->applied.typeSelected[i], i == list);
			this->layout.SetVar(TypeLabel(i), TypeLabel(i), language->PrepareMessage(lists[i].label).c_str());
		}
		KZ::ui::SetSwitchPick(entity, "replays_type", this->applied.typePick, list);
		KZ::ui::SetSwapClass(entity, "replays_window", this->applied.listClass, lists[list].listClass);
		// replays.css hides the mode switch on the manual list.
		const i32 modePick = GetModeIndex(this->mode);
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(KZ::api::modes); i++)
		{
			KZ::ui::SetBoolClass(entity, ModeButton(i), "selected", this->applied.modeSelected[i], modePick == i);
		}
		KZ::ui::SetSwitchPick(entity, "replays_mode", this->applied.modePick, modePick);
		// replays.css only shows the jump type switch on the jump list.
		// A jumptype argument beyond the switch's options leaves nothing picked.
		for (i32 i = 0; i < KZ_REPLAYS_JUMP_TYPES; i++)
		{
			KZ::ui::SetBoolClass(entity, JumpTypeButton(i), "selected", this->applied.jumpTypeSelected[i], this->jumpType == i);
		}
		KZ::ui::SetSwitchPick(entity, "replays_jt", this->applied.jumpTypePick, this->jumpType < KZ_REPLAYS_JUMP_TYPES ? this->jumpType : -1);
		this->layout.SetVar("replays_filters", "replays_filters",
							language->PrepareMessage("Replays - Window Filters", this->filters.c_str()).c_str());

		this->layout.SetVar("replays_th_player", "replays_th_player", language->PrepareMessage("Replay Table Header - Player").c_str());
		this->layout.SetVar("replays_th_map", "replays_th_map", language->PrepareMessage("Replay Table Header - Map").c_str());
		this->layout.SetVar("replays_th_time", "replays_th_time", language->PrepareMessage("Replay Table Header - Time").c_str());
		this->layout.SetVar("replays_th_replay", "replays_th_replay", language->PrepareMessage("Course Top - Window Replay").c_str());
		for (i32 c = 0; c < KZ_REPLAYS_CELLS; c++)
		{
			const char *header = lists[list].cells[c];
			this->layout.SetVar(HeadCell(c), HeadCell(c), header ? language->PrepareMessage(header).c_str() : "");
		}

		const std::string playTip = language->PrepareMessage("Course Top - Window Watch Replay");
		const std::string otherMapTip = language->PrepareMessage("Course Top - Window Other Map");
		const std::string copyTip = language->PrepareMessage("Replays - Window Copy IDs");
		const bool profiles = ProfileWindow::IsAvailable();
		for (i32 i = 0; i < KZ_REPLAYS_ROWS; i++)
		{
			const bool shown = i < (i32)this->rows.size();
			KZ::ui::SetBoolClass(entity, RowPanel(i), "hidden", this->applied.rowHidden[i], !shown);
			if (!shown)
			{
				continue;
			}
			const Row &row = this->rows[i];
			this->layout.SetVar(RowNumber(i), RowNumber(i), FormatThousands((f64)(this->offset + i + 1)).c_str());
			this->layout.SetVar(RowPlayer(i), RowPlayer(i), row.name.c_str());
			KZ::ui::SetBoolClass(entity, RowProfile(i), "static", this->applied.rowProfileStatic[i], !profiles || row.steamId == 0);
			this->layout.SetVar(RowMap(i), RowMap(i), row.map.c_str());
			for (i32 c = 0; c < KZ_REPLAYS_CELLS; c++)
			{
				this->layout.SetVar(RowCell(i, c), RowCell(i, c), row.cells[c].c_str());
			}
			this->layout.SetVar(RowTime(i), RowTime(i), row.time.c_str());
			this->layout.SetVar(RowBadge(i), RowBadge(i), row.badge ? (KZ_STREQ(row.badge, "tone-tp") ? "TP" : "PRO") : "");
			KZ::ui::SetSwapClass(entity, RowBadge(i), this->applied.rowBadge[i], row.badge);
			KZ::ui::SetBoolClass(entity, RowPlay(i), "disabled", this->applied.rowPlayDisabled[i], !row.playable);
			this->layout.SetVar(RowPlayTip(i), RowPlayTip(i), row.playable ? playTip.c_str() : otherMapTip.c_str());
			this->layout.SetVar(RowCopyTip(i), RowCopyTip(i), copyTip.c_str());
		}
		KZ::ui::SetBoolClass(entity, "replays_status", "hidden", this->applied.statusHidden, !this->rows.empty());
		this->layout.SetVar("replays_status", "replays_status", language->PrepareMessage("Replays - Window None").c_str());

		std::string count;
		if (!this->rows.empty())
		{
			const std::string first = FormatThousands((f64)this->offset + 1);
			const std::string last = FormatThousands((f64)(this->offset + this->rows.size()));
			count = language->PrepareMessage("Course Top - Window Count", first.c_str(), last.c_str(), FormatThousands((f64)this->total).c_str());
		}
		this->layout.SetVar("replays_count", "replays_count", count.c_str());
		this->layout.SetVar("replays_prev_label", "replays_prev_label", language->PrepareMessage("Course Top - Window Previous").c_str());
		this->layout.SetVar("replays_next_label", "replays_next_label", language->PrepareMessage("Course Top - Window Next").c_str());
		KZ::ui::SetBoolClass(entity, "replays_prev", "disabled", this->applied.prevDisabled, this->offset == 0);
		KZ::ui::SetBoolClass(entity, "replays_next", "disabled", this->applied.nextDisabled, this->offset + KZ_REPLAYS_ROWS >= this->total);
	}

	void ReplaysWindow::OnClick(const char *buttonId)
	{
		if (KZ_STREQ(buttonId, "replays_window_close"))
		{
			this->player->uiService->Close(this);
			return;
		}
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(lists); i++)
		{
			if (KZ_STREQ(buttonId, TypeButton(i)))
			{
				if (lists[i].type != this->type)
				{
					this->type = lists[i].type;
					this->offset = 0;
					this->Query();
					this->Render();
				}
				return;
			}
		}
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(KZ::api::modes); i++)
		{
			if (KZ_STREQ(buttonId, ModeButton(i)))
			{
				if (GetModeIndex(this->mode) != i)
				{
					this->mode = KZ::api::GetModeShortName(KZ::api::modes[i]);
					this->offset = 0;
					this->Query();
					this->Render();
				}
				return;
			}
		}
		for (i32 i = 0; i < KZ_REPLAYS_JUMP_TYPES; i++)
		{
			if (KZ_STREQ(buttonId, JumpTypeButton(i)))
			{
				if (this->jumpType != i)
				{
					this->jumpType = (u8)i;
					this->offset = 0;
					this->Query();
					this->Render();
				}
				return;
			}
		}
		if (KZ_STREQ(buttonId, "replays_prev") || KZ_STREQ(buttonId, "replays_next"))
		{
			const bool next = KZ_STREQ(buttonId, "replays_next");
			if (next ? this->applied.nextDisabled : this->applied.prevDisabled)
			{
				return;
			}
			this->offset = next ? this->offset + KZ_REPLAYS_ROWS : (this->offset > KZ_REPLAYS_ROWS ? this->offset - KZ_REPLAYS_ROWS : 0);
			this->Query();
			this->Render();
			return;
		}
		for (i32 i = 0; i < (i32)this->rows.size(); i++)
		{
			const Row &row = this->rows[i];
			if (KZ_STREQ(buttonId, RowProfile(i)))
			{
				if (row.steamId && ProfileWindow::IsAvailable())
				{
					this->player->uiService->GetWindow<ProfileWindow>()->ShowSteamId(row.steamId, row.mode);
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
			if (KZ_STREQ(buttonId, RowCopy(i)))
			{
				// The IDs are printed to chat and the console for copying because the server can't access the clipboard.
				const std::string steamId = std::to_string(row.steamId);
				this->player->languageService->PrintChat(true, false, "Replays - IDs", row.name.c_str(), row.uuid.c_str(), steamId.c_str());
				this->player->languageService->PrintConsole(true, false, "Replays - IDs", row.name.c_str(), row.uuid.c_str(), steamId.c_str());
				return;
			}
		}
	}
} // namespace KZ::replaysystem::hud
