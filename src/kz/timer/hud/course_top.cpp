#include "kz/timer/hud/course_top.h"
#include "kz/timer/hud/record_format.h"
#include "kz/global/kz_global.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/db/kz_db.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/profile/hud/profile.h"
#include "kz/replays/commands.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "tier0/memdbgon.h"

using KZ::records::FormatAgo;
using KZ::records::FormatDate;
using KZ::records::FormatRankingTime;
using KZ::records::FormatThousands;

#define SLOT_ID(fn, fmt) \
	static_function const char *fn(i32 i) \
	{ \
		static_persist char buf[48]; \
		V_snprintf(buf, sizeof(buf), fmt, i); \
		return buf; \
	}

SLOT_ID(ModeButton, "maptop_mode_%i")
SLOT_ID(TypeButton, "maptop_type_%i")
SLOT_ID(ScopeButton, "maptop_scope_%i")
SLOT_ID(Card, "maptop_card%i")
SLOT_ID(CardProfile, "maptop_card%i_profile")
SLOT_ID(Row, "maptop_row%i")
SLOT_ID(RowRank, "maptop_row%i_rank")
SLOT_ID(RowPlayer, "maptop_row%i_player")
SLOT_ID(RowTime, "maptop_row%i_time")
SLOT_ID(RowBadge, "maptop_row%i_badge")
SLOT_ID(RowPoints, "maptop_row%i_points")
SLOT_ID(RowDate, "maptop_row%i_date")
SLOT_ID(RowDateTip, "maptop_row%i_date_tip")
SLOT_ID(RowReplay, "maptop_row%i_replay")
SLOT_ID(RowReplayTip, "maptop_row%i_replay_tip")
SLOT_ID(RowCopy, "maptop_row%i_copy")
SLOT_ID(RowCopyTip, "maptop_row%i_copy_tip")
SLOT_ID(RowProfile, "maptop_row%i_profile")
#undef SLOT_ID

bool CourseTopWindow::CanOpen()
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
		this->chips.Reset();
	}
	return entity && entity->GetPlayerLayoutState(this->player->GetPlayerSlot());
}

u32 CourseTopWindow::Begin(const Query &query, KZ::ui::OpenMode openMode)
{
	this->query = query;
	this->hasResult = false;
	this->loading = true;
	this->type = Overall;
	// Try global first and let OnResult fall back to the server if the map has no global leaderboard.
	this->scope = Global;
	const u32 serial = ++this->serial;
	if (!this->player->uiService->Open(this, openMode))
	{
		this->loading = false;
		return 0;
	}
	this->Render();
	return serial;
}

void CourseTopWindow::ShowCourse(const std::string &map, const std::string &course, KZ::api::Mode mode, bool pro, bool server)
{
	Query query;
	query.map = map;
	query.course = course;
	query.mode = KZ::api::GetModeShortName(mode);
	query.local = KZDatabaseService::IsReady();
	query.global = KZGlobalService::IsAvailable();
	const u32 serial = this->Begin(query, KZ::ui::OpenMode::Push);
	if (!serial)
	{
		return;
	}
	this->type = pro ? Pro : Overall;
	this->scope = server ? Server : Global;
	SendQuery(this->player, query, serial);
	this->Render();
}

void CourseTopWindow::OnResult(u32 serial, const Result &result)
{
	if (serial != this->serial || !this->player->uiService->IsOpen(this))
	{
		return;
	}
	this->result = result;
	this->hasResult = true;
	this->loading = false;
	this->query.map = result.map;
	this->query.course = result.course;
	this->query.mode = result.mode;
	this->query.offset = result.offset;
	if (!result.received[this->scope])
	{
		this->scope = result.received[Global] ? Global : Server;
	}
	if (result.received[Global])
	{
		this->QueryCards();
	}
	this->QueryMapInfo();
	this->Render();
}

void CourseTopWindow::OnReplaysLocated(u32 serial, const std::unordered_set<UUID_t> &found)
{
	if (serial != this->serial || !this->hasResult || !this->player->uiService->IsOpen(this))
	{
		return;
	}
	for (std::vector<Run> &runs : this->result.runs[Server])
	{
		for (Run &run : runs)
		{
			UUID_t uuid(false);
			if (!run.replay && UUID_t::FromString(run.id.c_str(), &uuid) && found.count(uuid))
			{
				run.replay = true;
			}
		}
	}
	this->Render();
}

void CourseTopWindow::Navigate(const std::string &course, const std::string &mode, u64 offset)
{
	this->query.course = course;
	this->query.mode = mode;
	this->query.offset = offset;
	this->loading = true;
	SendQuery(this->player, this->query, ++this->serial);
	this->Render();
}

void CourseTopWindow::QueryCards()
{
	KZ::api::Mode mode;
	const std::string key = this->result.map + "/" + this->result.course + "/" + this->result.mode;
	if (key == this->cardKey || !KZ::api::DecodeModeString(this->result.mode, mode))
	{
		return;
	}
	this->cardKey = key;
	const u32 serial = ++this->cardSerial;
	for (i32 type = 0; type < TYPE_COUNT; type++)
	{
		this->worldRecord[type].reset();
		this->ownRecord[type].reset();
	}

	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	KZGlobalService::MessageCallback<KZ::api::messages::WorldRecords> onWorldRecords(
		[userID, serial](const KZ::api::messages::WorldRecords &records)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			CourseTopWindow *window = player ? player->uiService->GetWindow<CourseTopWindow>() : nullptr;
			if (!window || window->cardSerial != serial)
			{
				return;
			}
			window->worldRecord[Overall] = records.overall;
			window->worldRecord[Pro] = records.pro;
			window->Render();
		});
	KZGlobalService::QueryWorldRecords(this->result.map, this->result.course, mode, std::move(onWorldRecords));

	// The SteamID claimed by the client is good enough before Steam validates it because records are public.
	const u64 steamId = this->player->GetSteamId64(false);
	if (!steamId)
	{
		return;
	}
	KZGlobalService::QueryPBParams params;
	params.player = KZGlobalService::PlayerIdentifier::SteamID(steamId);
	params.map = this->result.map;
	params.course = this->result.course;
	params.mode = mode;
	KZGlobalService::MessageCallback<KZ::api::messages::PersonalBest> onPersonalBest(
		[userID, serial](const KZ::api::messages::PersonalBest &pb)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			CourseTopWindow *window = player ? player->uiService->GetWindow<CourseTopWindow>() : nullptr;
			if (!window || window->cardSerial != serial)
			{
				return;
			}
			window->ownRecord[Overall] = pb.overall;
			window->ownRecord[Pro] = pb.pro;
			window->Render();
		});
	KZGlobalService::QueryPB(params, std::move(onPersonalBest));
}

bool CourseTopWindow::IsCurrentMap()
{
	return KZ_STREQI(this->query.map.c_str(), g_pKZUtils->GetCurrentMapName().c_str());
}

void CourseTopWindow::QueryMapInfo()
{
	if (this->IsCurrentMap() || KZ_STREQI(this->mapInfoKey.c_str(), this->query.map.c_str()))
	{
		return;
	}
	this->mapInfoKey = this->query.map;
	this->mapInfo.reset();
	this->mapCourseNames.clear();
	const u32 serial = ++this->mapInfoSerial;
	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	if (KZDatabaseService::IsReady())
	{
		auto onCourses = [userID, serial](std::vector<ISQLQuery *> queries)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			CourseTopWindow *window = player ? player->uiService->GetWindow<CourseTopWindow>() : nullptr;
			if (!window || window->mapInfoSerial != serial)
			{
				return;
			}
			ISQLResult *result = queries[0]->GetResultSet();
			while (result && result->FetchRow())
			{
				window->mapCourseNames.push_back(utils::OrEmpty(result->GetString(0)));
			}
			window->Render();
		};
		KZDatabaseService::FindCoursesByMapName(this->query.map, onCourses, [](std::string, int) {});
	}
	if (!KZGlobalService::IsAvailable())
	{
		return;
	}
	KZGlobalService::MessageCallback<std::optional<KZ::api::Map>> onMapInfo(
		[userID, serial](const std::optional<KZ::api::Map> &map)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			CourseTopWindow *window = player ? player->uiService->GetWindow<CourseTopWindow>() : nullptr;
			if (!window || window->mapInfoSerial != serial)
			{
				return;
			}
			window->mapInfo = map;
			window->Render();
		});
	KZGlobalService::QueryMapInfo(this->query.map, std::move(onMapInfo));
}

std::vector<std::string> CourseTopWindow::GetMapCourseNames()
{
	if (KZ_STREQI(this->mapInfoKey.c_str(), this->query.map.c_str()) && !this->mapCourseNames.empty())
	{
		return this->mapCourseNames;
	}
	if (this->query.course.empty())
	{
		return {};
	}
	return {this->query.course};
}

u64 CourseTopWindow::GetWorkshopId()
{
	if (!this->IsCurrentMap())
	{
		return this->mapInfo ? this->mapInfo->workshopId : 0;
	}
	u64 id = g_pKZUtils->GetCurrentMapWorkshopID();
	if (!id)
	{
		KZGlobalService::WithCurrentMap(
			[&](const std::optional<KZ::api::Map> &currentMap)
			{
				if (currentMap)
				{
					id = currentMap->workshopId;
				}
			});
	}
	return id;
}

void CourseTopWindow::OnOpen()
{
	this->Render();
}

void CourseTopWindow::Hide()
{
	if (CCSCustomHudLayout *entity = this->layout.Get())
	{
		KZ::ui::SetBoolClass(entity, "maptop_root", "hidden", this->applied.rootHidden, true);
	}
}

void CourseTopWindow::OnClose(KZ::ui::CloseReason reason)
{
	this->Hide();
	// Query the cards again next time in case the player has set a new record by then.
	this->cardKey.clear();
	this->cardSerial++;
}

void CourseTopWindow::OnSuspend()
{
	this->Hide();
}

void CourseTopWindow::OnResume()
{
	this->Render();
}

void CourseTopWindow::RenderCard(CCSCustomHudLayout *entity, i32 card, const std::optional<KZ::api::Record> &record)
{
	KZ::ui::SetBoolClass(entity, Card(card), "hidden", this->applied.cardHidden[card], !record.has_value());
	if (!record)
	{
		return;
	}
	const u32 rank = this->type == Pro ? record->proRank : record->nubRank;
	const u32 total = this->type == Pro ? record->proMaxRank : record->nubMaxRank;
	KZ::ui::SetBoolClass(entity, Card(card), "wr", this->applied.cardWr[card], rank == 1);

	char id[48];
	auto set = [&](const char *suffix, const std::string &value)
	{
		V_snprintf(id, sizeof(id), "maptop_card%i_%s", card, suffix);
		this->layout.SetVar(id, id, value.c_str());
	};
	set("name", record->player.name);
	KZ::ui::SetBoolClass(entity, CardProfile(card), "static", this->applied.cardProfileStatic[card],
						 !ProfileWindow::IsAvailable() || record->player.id == 0);
	set("rank", tfm::format("#%u", rank));
	set("total", FormatThousands(total));
	set("time", FormatRankingTime(record->time));
	set("date", FormatAgo(this->player, record->id));
	set("time_label", this->player->languageService->PrepareMessage("Course Top - Window Time"));
	set("date_label", this->player->languageService->PrepareMessage("Course Top - Window Date"));
}

void CourseTopWindow::Render()
{
	CCSCustomHudLayout *entity = this->layout.Get();
	if (!entity || this->player->uiService->GetTop() != this)
	{
		return;
	}
	auto *language = this->player->languageService;
	KZ::ui::SetBoolClass(entity, "maptop_root", "hidden", this->applied.rootHidden, false);
	KZ::ui::ApplyWindowStyle(this->player, entity, "maptop_root", this->applied.style);
	// The close button goes back if the window was opened from another one.
	KZ::ui::SetBoolClass(entity, "maptop_window_close", "back", this->applied.closeBack, this->player->uiService->HasWindowBelow(this));

	KZ::api::Mode apiMode {};
	const bool globalMode = KZ::api::DecodeModeString(this->query.mode, apiMode);
	this->layout.SetVar("maptop_window_title", "maptop_window_title", this->query.map.c_str());

	// The scope switch only shows if both global and server records were requested or received.
	i32 modePick = -1;
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(KZ::api::modes); i++)
	{
		const bool selected = globalMode && apiMode == KZ::api::modes[i];
		KZ::ui::SetBoolClass(entity, ModeButton(i), "selected", this->applied.modeSelected[i], selected);
		modePick = selected ? i : modePick;
	}
	for (i32 i = 0; i < TYPE_COUNT; i++)
	{
		KZ::ui::SetBoolClass(entity, TypeButton(i), "selected", this->applied.typeSelected[i], this->type == i);
		KZ::ui::SetBoolClass(entity, ScopeButton(i), "selected", this->applied.scopeSelected[i], this->scope == i);
	}
	KZ::ui::SetSwitchPick(entity, "maptop_mode", this->applied.modePick, modePick);
	KZ::ui::SetSwitchPick(entity, "maptop_type", this->applied.typePick, this->type);
	KZ::ui::SetSwitchPick(entity, "maptop_scope", this->applied.scopePick, this->scope);
	const bool bothScopes =
		this->hasResult ? this->result.received[Global] && this->result.received[Server] : this->query.global && this->query.local;
	KZ::ui::SetBoolClass(entity, "maptop_scope", "hidden", this->applied.scopeHidden, !bothScopes);
	this->layout.SetVar("maptop_type_0_label", "maptop_type_0_label", language->PrepareMessage("Course Top - Window Overall").c_str());
	this->layout.SetVar("maptop_type_1_label", "maptop_type_1_label", language->PrepareMessage("Course Top - Window Pro").c_str());
	this->layout.SetVar("maptop_scope_0_label", "maptop_scope_0_label", language->PrepareMessage("Course Top - Window Global").c_str());
	this->layout.SetVar("maptop_scope_1_label", "maptop_scope_1_label", language->PrepareMessage("Course Top - Window Server").c_str());

	// The chips list the current map's courses or another map's once the API sent its details.
	// Without chips the heading names the course instead.
	const KZ::api::Mode chipMode = globalMode ? apiMode : KZ::api::Mode::Classic;
	const bool currentMap = this->IsCurrentMap();
	const bool otherMapChips = !currentMap && this->mapInfo && KZ_STREQI(this->mapInfo->name.c_str(), this->query.map.c_str());
	if (currentMap)
	{
		const KZCourseDescriptor *shown = KZ::course::GetCourse(this->query.course.c_str(), false);
		this->chips.Render(this->player, this->layout, entity, chipMode, this->type == Pro, shown);
	}
	else if (otherMapChips)
	{
		this->chips.RenderMap(this->player, this->layout, entity, *this->mapInfo, chipMode, this->type == Pro, this->query.course.c_str());
	}
	else
	{
		this->chips.RenderNames(this->player, this->layout, entity, this->GetMapCourseNames(), this->query.course.c_str());
	}
	this->layout.SetVar("maptop_heading", "maptop_heading", language->PrepareMessage("Course Top - Window Ranking").c_str());

	KZ::ui::SetBoolClass(entity, "maptop_workshop", "hidden", this->applied.workshopHidden, this->GetWorkshopId() == 0);
	this->layout.SetVar("maptop_workshop_tip", "maptop_workshop_tip", language->PrepareMessage("Course Top - Window Copy Workshop ID").c_str());

	// The own card is hidden if the player holds the record.
	// Server records get no cards because they have no ranks.
	const bool global = this->scope == Global;
	const std::optional<KZ::api::Record> none;
	const std::optional<KZ::api::Record> &wr = global ? this->worldRecord[this->type] : none;
	const std::optional<KZ::api::Record> &own = global ? this->ownRecord[this->type] : none;
	this->RenderCard(entity, 0, wr);
	this->RenderCard(entity, 1, own && !(wr && wr->player.id == own->player.id) ? own : none);

	this->layout.SetVar("maptop_th_rank", "maptop_th_rank", language->PrepareMessage("Course Top - Window Rank").c_str());
	this->layout.SetVar("maptop_th_player", "maptop_th_player", language->PrepareMessage("Course Top - Window Player").c_str());
	this->layout.SetVar("maptop_th_time", "maptop_th_time", language->PrepareMessage("Course Top - Window Time").c_str());
	this->layout.SetVar(
		"maptop_th_points", "maptop_th_points",
		language->PrepareMessage(this->type == Pro ? "Course Top - Window Pro Points" : "Course Top - Window Overall Points").c_str());
	this->layout.SetVar("maptop_th_date", "maptop_th_date", language->PrepareMessage("Course Top - Window Date").c_str());
	this->layout.SetVar("maptop_th_replay", "maptop_th_replay", language->PrepareMessage("Course Top - Window Replay").c_str());

	static_persist const std::vector<Run> noRuns;
	const std::vector<Run> &runs = this->hasResult ? this->result.runs[this->scope][this->type] : noRuns;
	const u64 offset = this->hasResult ? this->result.offset : this->query.offset;
	const std::string replayTip = language->PrepareMessage("Course Top - Window Watch Replay");
	const std::string noReplayTip = language->PrepareMessage("Course Top - Window No Replay");
	const std::string otherMapTip = language->PrepareMessage("Course Top - Window Other Map");
	const std::string copyTip = language->PrepareMessage("Course Top - Window Copy ID");
	// Names link to profiles if the API or the server's database can provide them.
	const bool profiles = ProfileWindow::IsAvailable();
	for (i32 i = 0; i < KZ_COURSE_TOP_ROWS; i++)
	{
		const bool shown = i < (i32)runs.size();
		KZ::ui::SetBoolClass(entity, Row(i), "hidden", this->applied.rowHidden[i], !shown);
		if (!shown)
		{
			continue;
		}
		const Run &run = runs[i];
		this->layout.SetVar(RowRank(i), RowRank(i), tfm::format("#%llu", offset + i + 1).c_str());
		this->layout.SetVar(RowPlayer(i), RowPlayer(i), run.name.c_str());
		this->layout.SetVar(RowTime(i), RowTime(i), FormatRankingTime(run.time).c_str());
		this->layout.SetVar(RowBadge(i), RowBadge(i), run.teleports > 0 ? "TP" : "PRO");
		KZ::ui::SetSwapClass(entity, RowBadge(i), this->applied.rowBadge[i], run.teleports > 0 ? "tone-tp" : "tone-pro");
		this->layout.SetVar(RowPoints(i), RowPoints(i), run.points > 0 ? FormatThousands(run.points).c_str() : "-");
		KZ::ui::SetSwapClass(entity, RowPoints(i), this->applied.rowPoints[i],
							 run.points >= 10000 ? "points-top" : (run.points >= 9000 ? "points-high" : nullptr));
		this->layout.SetVar(RowDate(i), RowDate(i), FormatAgo(this->player, run.id).c_str());
		this->layout.SetVar(RowDateTip(i), RowDateTip(i), FormatDate(run.id).c_str());
		const bool replay = !run.id.empty() && run.replay;
		KZ::ui::SetBoolClass(entity, RowReplay(i), "disabled", this->applied.rowReplayDisabled[i], !replay || !currentMap);
		KZ::ui::SetBoolClass(entity, RowCopy(i), "hidden", this->applied.rowCopyHidden[i], run.id.empty());
		this->layout.SetVar(RowReplayTip(i), RowReplayTip(i), !replay ? noReplayTip.c_str() : (currentMap ? replayTip.c_str() : otherMapTip.c_str()));
		this->layout.SetVar(RowCopyTip(i), RowCopyTip(i), copyTip.c_str());
		KZ::ui::SetBoolClass(entity, RowProfile(i), "static", this->applied.rowProfileStatic[i], !profiles || run.steamId == 0);
	}

	const char *status = nullptr;
	if (!this->hasResult)
	{
		status = this->loading ? "Course Top - Window Loading" : "Course Top - Window Failed";
	}
	else if (!this->result.received[Global] && !this->result.received[Server])
	{
		status = "Course Top - Window Failed";
	}
	else if (runs.empty())
	{
		status = "Course Top - Window No Data";
	}
	KZ::ui::SetBoolClass(entity, "maptop_status", "hidden", this->applied.statusHidden, !status);
	this->layout.SetVar("maptop_status", "maptop_status", status ? language->PrepareMessage(status).c_str() : "");

	// The list is paged because the server can't see it scroll.
	const u32 total = runs.empty() ? 0 : runs[0].maxRank;
	std::string count;
	if (this->loading && this->hasResult)
	{
		count = language->PrepareMessage("Course Top - Window Loading");
	}
	else if (!runs.empty())
	{
		const std::string first = FormatThousands((f64)offset + 1);
		const std::string last = FormatThousands((f64)(offset + runs.size()));
		count = total ? language->PrepareMessage("Course Top - Window Count", first.c_str(), last.c_str(), FormatThousands(total).c_str())
					  : language->PrepareMessage("Course Top - Window Count (No Total)", first.c_str(), last.c_str());
	}
	this->layout.SetVar("maptop_count", "maptop_count", count.c_str());
	this->layout.SetVar("maptop_prev_label", "maptop_prev_label", language->PrepareMessage("Course Top - Window Previous").c_str());
	this->layout.SetVar("maptop_next_label", "maptop_next_label", language->PrepareMessage("Course Top - Window Next").c_str());
	const bool more = runs.size() == KZ_COURSE_TOP_ROWS && (!total || offset + KZ_COURSE_TOP_ROWS < total);
	KZ::ui::SetBoolClass(entity, "maptop_prev", "disabled", this->applied.prevDisabled, offset == 0 || this->loading);
	KZ::ui::SetBoolClass(entity, "maptop_next", "disabled", this->applied.nextDisabled, !more || this->loading);
}

void CourseTopWindow::OnClick(const char *buttonId)
{
	if (KZ_STREQ(buttonId, "maptop_window_close"))
	{
		this->player->uiService->Close(this);
		return;
	}
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(KZ::api::modes); i++)
	{
		if (KZ_STREQ(buttonId, ModeButton(i)))
		{
			const char *modeName = KZ::api::GetModeShortName(KZ::api::modes[i]);
			if (!KZ_STREQI(this->query.mode.c_str(), modeName))
			{
				this->Navigate(this->query.course, modeName, 0);
			}
			return;
		}
	}
	for (i32 i = 0; i < TYPE_COUNT; i++)
	{
		if (KZ_STREQ(buttonId, TypeButton(i)))
		{
			this->type = (Type)i;
			this->Render();
			return;
		}
		if (KZ_STREQ(buttonId, ScopeButton(i)))
		{
			this->scope = (Scope)i;
			this->Render();
			return;
		}
	}
	if (KZ_STREQ(buttonId, "maptop_prev") || KZ_STREQ(buttonId, "maptop_next"))
	{
		const bool next = KZ_STREQ(buttonId, "maptop_next");
		if (this->loading || (next ? this->applied.nextDisabled : this->applied.prevDisabled))
		{
			return;
		}
		const u64 offset = this->query.offset;
		this->Navigate(this->query.course, this->query.mode,
					   next ? offset + KZ_COURSE_TOP_ROWS : (offset > KZ_COURSE_TOP_ROWS ? offset - KZ_COURSE_TOP_ROWS : 0));
		return;
	}
	if (KZ_STREQ(buttonId, "maptop_workshop"))
	{
		if (const u64 id = this->GetWorkshopId())
		{
			const std::string idText = std::to_string(id);
			this->player->languageService->PrintChat(true, false, "Course Top - Workshop ID", this->query.map.c_str(), idText.c_str());
			this->player->languageService->PrintConsole(true, false, "Course Top - Workshop ID", this->query.map.c_str(), idText.c_str());
		}
		return;
	}
	for (i32 card = 0; card < 2; card++)
	{
		if (!KZ_STREQ(buttonId, CardProfile(card)))
		{
			continue;
		}
		const std::optional<KZ::api::Record> &record = card == 0 ? this->worldRecord[this->type] : this->ownRecord[this->type];
		KZ::api::Mode apiMode;
		if (this->scope == Global && record && record->player.id && ProfileWindow::IsAvailable()
			&& KZ::api::DecodeModeString(this->query.mode, apiMode))
		{
			this->player->uiService->GetWindow<ProfileWindow>()->ShowSteamId(record->player.id, apiMode);
		}
		return;
	}
	if (const i32 slot = this->chips.GetClickedSlot(buttonId); slot >= 0)
	{
		const KZCourseDescriptor *course = this->IsCurrentMap() ? KZ::course::GetCourseByIndex(slot) : nullptr;
		std::string name = course ? course->name : "";
		const bool otherMapChips = this->mapInfo && KZ_STREQI(this->mapInfo->name.c_str(), this->query.map.c_str());
		if (!this->IsCurrentMap() && otherMapChips && slot < (i32)this->mapInfo->courses.size())
		{
			name = this->mapInfo->courses[slot].name;
		}
		else if (!this->IsCurrentMap() && !otherMapChips)
		{
			const std::vector<std::string> names = this->GetMapCourseNames();
			name = slot < (i32)names.size() ? names[slot] : "";
		}
		if (!name.empty() && !KZ_STREQI(name.c_str(), this->query.course.c_str()))
		{
			this->Navigate(name, this->query.mode, 0);
		}
		return;
	}
	for (i32 i = 0; i < KZ_COURSE_TOP_ROWS; i++)
	{
		if (!KZ_STREQ(buttonId, RowProfile(i)))
		{
			continue;
		}
		const std::vector<Run> &runs = this->result.runs[this->scope][this->type];
		if (this->hasResult && i < (i32)runs.size() && runs[i].steamId && ProfileWindow::IsAvailable())
		{
			KZ::api::Mode apiMode;
			const bool globalMode = KZ::api::DecodeModeString(this->query.mode, apiMode);
			this->player->uiService->GetWindow<ProfileWindow>()->ShowSteamId(runs[i].steamId, globalMode ? apiMode : KZ::api::Mode::Classic,
																			 this->scope == Server);
		}
		return;
	}
	for (i32 i = 0; i < KZ_COURSE_TOP_ROWS; i++)
	{
		const bool replay = KZ_STREQ(buttonId, RowReplay(i));
		if (!replay && !KZ_STREQ(buttonId, RowCopy(i)))
		{
			continue;
		}
		const std::vector<Run> &runs = this->result.runs[this->scope][this->type];
		if (!this->hasResult || i >= (i32)runs.size() || runs[i].id.empty() || (replay && (!runs[i].replay || !this->IsCurrentMap())))
		{
			return;
		}
		const std::string id = runs[i].id;
		if (replay)
		{
			this->player->uiService->CloseAll(KZ::ui::CloseReason::Closed);
			KZ::replaysystem::commands::LoadReplay(this->player, id.c_str());
			return;
		}
		// The ID is printed to chat and the console for copying because the server can't access the clipboard.
		this->player->languageService->PrintChat(true, false, "Course Top - Record ID", runs[i].name.c_str(), id.c_str());
		this->player->languageService->PrintConsole(true, false, "Course Top - Record ID", runs[i].name.c_str(), id.c_str());
		return;
	}
}
