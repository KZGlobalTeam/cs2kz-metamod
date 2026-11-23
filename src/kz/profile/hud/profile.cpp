#include "kz/profile/hud/profile.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/profile/kz_profile.h"
#include "kz/replays/commands.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/timer/hud/course_top.h"
#include "kz/timer/hud/record_format.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/http.h"
#include "utils/json.h"
#include "utils/utils.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "tier0/memdbgon.h"

#define SLOT_ID(fn, fmt) \
	static_function const char *fn(i32 i) \
	{ \
		static_persist char buf[48]; \
		V_snprintf(buf, sizeof(buf), fmt, i); \
		return buf; \
	}

SLOT_ID(ModeButton, "profile_mode_%i")
SLOT_ID(TypeButton, "profile_type_%i")
SLOT_ID(ScopeButton, "profile_scope_%i")
SLOT_ID(Row, "profile_row%i")
SLOT_ID(RowMap, "profile_row%i_map")
SLOT_ID(RowMapButton, "profile_row%i_map_btn")
SLOT_ID(RowCourse, "profile_row%i_course")
SLOT_ID(RowCourseButton, "profile_row%i_course_btn")
SLOT_ID(RowTier, "profile_row%i_tier")
SLOT_ID(RowTime, "profile_row%i_time")
SLOT_ID(RowBadge, "profile_row%i_badge")
SLOT_ID(RowPoints, "profile_row%i_points")
SLOT_ID(RowRank, "profile_row%i_rank")
SLOT_ID(RowDate, "profile_row%i_date")
SLOT_ID(RowDateTip, "profile_row%i_date_tip")
SLOT_ID(RowReplay, "profile_row%i_replay")
SLOT_ID(RowReplayTip, "profile_row%i_replay_tip")
SLOT_ID(RowCopy, "profile_row%i_copy")
SLOT_ID(RowCopyTip, "profile_row%i_copy_tip")
#undef SLOT_ID

// The modes' names in API queries in the order of KZ::api::modes.
static_global const char *const modeQuery[] = {"classic", "vanilla"};

static_global const char *const tierClasses[] = {
	"tier-1", "tier-2", "tier-3", "tier-4", "tier-5", "tier-6", "tier-7", "tier-8", "tier-9", "tier-10",
};

// Percent-encodes a player name for a URL query.
static_function std::string EncodeQueryValue(const std::string &text)
{
	std::string out;
	for (unsigned char c : text)
	{
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			out += (char)c;
		}
		else
		{
			out += tfm::format("%%%02X", (u32)c);
		}
	}
	return out;
}

// Parses a SteamID64, STEAM_X:Y:Z or [U:1:N] into a SteamID64.
// Returns 0 if the text is none of them.
static_function u64 ParseSteamId(const std::string &text)
{
	if (text.size() >= 15 && std::all_of(text.begin(), text.end(), [](char c) { return isdigit((unsigned char)c); }))
	{
		return strtoull(text.c_str(), nullptr, 10);
	}
	u64 id = 0;
	if (V_strnicmp(text.c_str(), "STEAM_", 6) == 0 && utils::ParseSteamID2(text, id))
	{
		return id;
	}
	const char *account = nullptr;
	if (V_strnicmp(text.c_str(), "[U:1:", 5) == 0)
	{
		account = text.c_str() + 5;
	}
	else if (V_strnicmp(text.c_str(), "U:1:", 4) == 0)
	{
		account = text.c_str() + 4;
	}
	return account && isdigit((unsigned char)account[0]) ? 76561197960265728ull + strtoull(account, nullptr, 10) : 0;
}

bool ProfileWindow::Profile::FromJson(const Json &json)
{
	std::string id;
	if (!(json.Get("id", id) && utils::ParseSteamID2(id, this->steamId) && json.Get("name", this->name) && json.Get("ckz_rating", this->rating[0])
		  && json.Get("vnl_rating", this->rating[1])))
	{
		return false;
	}
	// The rank stays 0 for unranked players because they have no rank field.
	json.Get("ckz_rank", this->globalRank[0]);
	json.Get("vnl_rank", this->globalRank[1]);
	// Only the date part of the timestamp is shown.
	std::string joined;
	json.Get("first_joined_at", joined);
	this->joined = joined.substr(0, 10);
	return true;
}

static_function bool IsGlobalAvailable()
{
	return KZGlobalService::IsAvailable();
}

static_function bool IsServerAvailable()
{
	return KZDatabaseService::IsReady();
}

bool ProfileWindow::IsAvailable()
{
	return IsGlobalAvailable() || IsServerAvailable();
}

// The SteamID claimed by the client is good enough before Steam validates it because profiles are public.
// Returns an empty string if the client has no SteamID.
static_function std::string IdentifierOf(KZPlayer *player)
{
	const u64 steamId = player->GetSteamId64(false);
	return steamId ? std::to_string(steamId) : "";
}

bool ProfileWindow::CanOpen()
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

void ProfileWindow::ShowSteamId(u64 steamId, KZ::api::Mode mode, bool server)
{
	const bool global = server ? !IsServerAvailable() : IsGlobalAvailable();
	this->Begin(std::to_string(steamId), mode, KZ::ui::OpenMode::Push, global ? Global : Server);
}

bool ProfileWindow::ShowQuery(const char *query)
{
	KZ::api::Mode mode = KZ::api::Mode::Classic;
	KZ::api::DecodeModeString(KZ::mode::GetModeInfo(this->player->modeService).shortModeName, mode);

	// Prefer the global profile if the API is available.
	const Scope scope = IsGlobalAvailable() ? Global : Server;
	std::string text = query ? query : "";
	text.erase(0, text.find_first_not_of(" \t\""));
	text.erase(text.find_last_not_of(" \t\"") + 1);
	if (text.empty())
	{
		return this->Begin(IdentifierOf(this->player), mode, KZ::ui::OpenMode::Replace, scope);
	}
	if (const u64 steamId = ParseSteamId(text))
	{
		return this->Begin(std::to_string(steamId), mode, KZ::ui::OpenMode::Replace, scope);
	}

	// Look for an exact name among the players in the server first and then for the first name containing the text.
	KZPlayer *partial = nullptr;
	for (KZPlayer *other : g_pKZPlayerManager->GetInGamePlayers())
	{
		const char *name = other->GetName();
		if (KZ_STREQI(name, text.c_str()))
		{
			return this->Begin(IdentifierOf(other), mode, KZ::ui::OpenMode::Replace, scope);
		}
		if (!partial && V_stristr(name, text.c_str()))
		{
			partial = other;
		}
	}
	if (partial)
	{
		return this->Begin(IdentifierOf(partial), mode, KZ::ui::OpenMode::Replace, scope);
	}
	return this->Begin(text, mode, KZ::ui::OpenMode::Replace, scope);
}

bool ProfileWindow::Begin(const std::string &identifier, KZ::api::Mode mode, KZ::ui::OpenMode openMode, Scope scope)
{
	this->identifier = identifier;
	this->mode = mode;
	this->type = Overall;
	this->scope = scope;
	this->offset = 0;
	this->profile.reset();
	this->runs.clear();
	this->total = 0;
	this->playerState = State::Loading;
	this->runsState = State::Loading;
	if (!this->player->uiService->Open(this, openMode))
	{
		return false;
	}
	this->RequestPlayer();
	this->Render();
	return true;
}

void ProfileWindow::RequestPlayer()
{
	if (this->identifier.empty())
	{
		this->playerSerial++;
		this->runsSerial++;
		this->playerState = State::NotFound;
		return;
	}
	if (this->scope == Server)
	{
		this->RequestServerPlayer();
		return;
	}
	const u32 serial = ++this->playerSerial;
	this->runsSerial++;
	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	// A SteamID requests that player directly.
	// A name is searched for and the highest-rated match in the shown mode is used.
	const u64 steamId = ParseSteamId(this->identifier);
	const bool search = steamId == 0;
	HTTP::Request request(HTTP::Method::GET, KZ::api::GetBaseUrl() + "/players" + (search ? "" : "/" + std::to_string(steamId)));
	if (search)
	{
		request.SetQuery("name", EncodeQueryValue(this->identifier));
		request.SetQuery("sort_by", this->mode == KZ::api::Mode::Vanilla ? "vnl-rating" : "ckz-rating");
		request.SetQuery("limit", "1");
	}
	auto onResponse = [userID, serial, search](HTTP::Response response)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		ProfileWindow *window = player ? player->uiService->GetWindow<ProfileWindow>() : nullptr;
		if (!window || window->playerSerial != serial)
		{
			return;
		}
		Json json(response.Body().value_or(""));
		Profile profile;
		std::vector<Profile> found;
		bool ok = response.status == 200 && json.IsValid();
		if (ok && search)
		{
			ok = json.Get("values", found) && !found.empty();
			if (ok)
			{
				profile = found[0];
			}
		}
		else if (ok)
		{
			ok = profile.FromJson(json);
		}
		if (!ok)
		{
			const bool notFound = response.status == 404 || (search && response.status == 200 && found.empty());
			window->playerState = notFound ? State::NotFound : State::Failed;
			window->Render();
			return;
		}
		window->profile = profile;
		window->playerState = State::Loaded;
		window->RequestRuns();
		window->Render();
	};
	auto onError = [userID, serial]()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		ProfileWindow *window = player ? player->uiService->GetWindow<ProfileWindow>() : nullptr;
		if (window && window->playerSerial == serial)
		{
			window->playerState = State::Failed;
			window->Render();
		}
	};
	request.Send(onResponse, onError);
}

// Requests the player's global personal bests from newest to oldest.
void ProfileWindow::RequestRuns()
{
	if (!this->profile)
	{
		return;
	}
	if (this->scope == Server)
	{
		this->RequestServerRuns();
		return;
	}
	const u32 serial = ++this->runsSerial;
	this->runsState = State::Loading;
	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	HTTP::Request request(HTTP::Method::GET, KZ::api::GetBaseUrl() + "/records");
	request.SetQuery("player", std::to_string(this->profile->steamId));
	request.SetQuery("mode", modeQuery[KZ::api::GetModeIndex(this->mode)]);
	request.SetQuery("top", "true");
	if (this->type == Pro)
	{
		request.SetQuery("has_teleports", "false");
	}
	request.SetQuery("sort_by", "submission-date");
	request.SetQuery("sort_order", "descending");
	request.SetQuery("limit", std::to_string(KZ_PROFILE_ROWS));
	request.SetQuery("offset", std::to_string(this->offset));
	auto onResponse = [userID, serial](HTTP::Response response)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		ProfileWindow *window = player ? player->uiService->GetWindow<ProfileWindow>() : nullptr;
		if (!window || window->runsSerial != serial)
		{
			return;
		}
		Json json(response.Body().value_or(""));
		std::vector<KZ::api::Record> runs;
		u64 total = 0;
		const bool ok = response.status == 200 && json.IsValid() && json.Get("total", total) && json.Get("values", runs);
		window->runs = ok ? std::move(runs) : std::vector<KZ::api::Record>();
		window->total = ok ? total : 0;
		window->runsState = ok ? State::Loaded : State::Failed;
		window->Render();
	};
	auto onError = [userID, serial]()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		ProfileWindow *window = player ? player->uiService->GetWindow<ProfileWindow>() : nullptr;
		if (window && window->runsSerial == serial)
		{
			window->runs.clear();
			window->runsState = State::Failed;
			window->Render();
		}
	};
	request.Send(onResponse, onError);
}

// Requests the player's name and first visit from the server's database.
void ProfileWindow::RequestServerPlayer()
{
	const u32 serial = ++this->playerSerial;
	this->runsSerial++;
	if (!KZDatabaseService::IsReady())
	{
		this->playerState = State::Failed;
		return;
	}
	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	auto onSuccess = [userID, serial](std::vector<ISQLQuery *> queries)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		ProfileWindow *window = player ? player->uiService->GetWindow<ProfileWindow>() : nullptr;
		if (!window || window->playerSerial != serial)
		{
			return;
		}
		ISQLResult *result = queries[0]->GetResultSet();
		if (!result || !result->FetchRow())
		{
			window->playerState = State::NotFound;
			window->Render();
			return;
		}
		Profile profile;
		profile.steamId = (u64)result->GetInt64(0);
		profile.name = utils::OrEmpty(result->GetString(1));
		// SQLite and MySQL both return "YYYY-MM-DD hh:mm:ss" and only the date is shown.
		profile.joined = std::string(utils::OrEmpty(result->GetString(2))).substr(0, 10);
		window->profile = profile;
		window->playerState = State::Loaded;
		window->RequestRuns();
		window->Render();
	};
	auto onFailure = [userID, serial](std::string, int)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		ProfileWindow *window = player ? player->uiService->GetWindow<ProfileWindow>() : nullptr;
		if (window && window->playerSerial == serial)
		{
			window->playerState = State::Failed;
			window->Render();
		}
	};
	const u64 steamId = ParseSteamId(this->identifier);
	KZDatabaseService::FindServerProfile(steamId, steamId ? "" : this->identifier, onSuccess, onFailure);
}

// Requests the player's personal bests on this server with their server ranks from newest to oldest.
void ProfileWindow::RequestServerRuns()
{
	const u32 serial = ++this->runsSerial;
	this->runsState = State::Loading;
	const i32 modeID = KZ::mode::GetModeInfo(KZ::api::GetModeShortName(this->mode)).databaseID;
	if (!KZDatabaseService::IsReady() || modeID < 0)
	{
		// A mode the server doesn't have has no runs on it.
		this->runs.clear();
		this->total = 0;
		this->runsState = KZDatabaseService::IsReady() ? State::Loaded : State::Failed;
		return;
	}
	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	const bool pro = this->type == Pro;
	auto onSuccess = [userID, serial, pro](std::vector<ISQLQuery *> queries)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		ProfileWindow *window = player ? player->uiService->GetWindow<ProfileWindow>() : nullptr;
		if (!window || window->runsSerial != serial)
		{
			return;
		}
		window->runs.clear();
		window->runReplays.clear();
		ISQLResult *result = queries[0]->GetResultSet();
		while (result && result->FetchRow())
		{
			KZ::api::Record run {};
			run.id = utils::OrEmpty(result->GetString(0));
			run.map.name = utils::OrEmpty(result->GetString(1));
			run.course.name = utils::OrEmpty(result->GetString(2));
			run.time = result->GetFloat(3);
			run.teleports = (u32)result->GetInt64(4);
			(pro ? run.proRank : run.nubRank) = (u32)result->GetInt64(5);
			std::string path;
			window->runReplays.push_back(KZ::replaysystem::FindReplayPath(run.id.c_str(), path));
			window->runs.push_back(run);
		}
		result = queries.size() > 1 ? queries[1]->GetResultSet() : nullptr;
		window->total = result && result->FetchRow() ? (u64)result->GetInt64(0) : window->runs.size();
		window->runsState = State::Loaded;
		window->Render();
	};
	auto onFailure = [userID, serial](std::string, int)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		ProfileWindow *window = player ? player->uiService->GetWindow<ProfileWindow>() : nullptr;
		if (window && window->runsSerial == serial)
		{
			window->runs.clear();
			window->runsState = State::Failed;
			window->Render();
		}
	};
	KZDatabaseService::QueryPlayerPBs(this->profile->steamId, (u32)modeID, pro, KZ_PROFILE_ROWS, this->offset, onSuccess, onFailure);
}

void ProfileWindow::OnOpen()
{
	this->Render();
}

void ProfileWindow::Hide()
{
	if (CCSCustomHudLayout *entity = this->layout.Get())
	{
		KZ::ui::SetBoolClass(entity, "profile_root", "hidden", this->applied.rootHidden, true);
	}
}

void ProfileWindow::OnClose(KZ::ui::CloseReason reason)
{
	this->Hide();
	// Bumping the serials drops replies that arrive after closing.
	this->playerSerial++;
	this->runsSerial++;
}

void ProfileWindow::OnSuspend()
{
	this->Hide();
}

void ProfileWindow::OnResume()
{
	this->Render();
}

void ProfileWindow::Render()
{
	CCSCustomHudLayout *entity = this->layout.Get();
	if (!entity || this->player->uiService->GetTop() != this)
	{
		return;
	}
	auto *language = this->player->languageService;
	KZ::ui::SetBoolClass(entity, "profile_root", "hidden", this->applied.rootHidden, false);
	KZ::ui::ApplyWindowStyle(this->player, entity, "profile_root", this->applied.style);
	// The close button goes back if the window was opened from another one.
	KZ::ui::SetBoolClass(entity, "profile_window_close", "back", this->applied.closeBack, this->player->uiService->HasWindowBelow(this));

	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(KZ::api::modes); i++)
	{
		KZ::ui::SetBoolClass(entity, ModeButton(i), "selected", this->applied.modeSelected[i], this->mode == KZ::api::modes[i]);
	}
	for (i32 i = 0; i < TYPE_COUNT; i++)
	{
		KZ::ui::SetBoolClass(entity, TypeButton(i), "selected", this->applied.typeSelected[i], this->type == i);
	}
	KZ::ui::SetSwitchPick(entity, "profile_mode", this->applied.modePick, KZ::api::GetModeIndex(this->mode));
	KZ::ui::SetSwitchPick(entity, "profile_type", this->applied.typePick, this->type);
	KZ::ui::SetSwitchPick(entity, "profile_scope", this->applied.scopePick, this->scope);
	this->layout.SetVar("profile_type_0_label", "profile_type_0_label", language->PrepareMessage("Course Top - Window Overall").c_str());
	this->layout.SetVar("profile_type_1_label", "profile_type_1_label", language->PrepareMessage("Course Top - Window Pro").c_str());
	// The scope switch only shows if both the API and the server's database are available.
	for (i32 i = 0; i < SCOPE_COUNT; i++)
	{
		KZ::ui::SetBoolClass(entity, ScopeButton(i), "selected", this->applied.scopeSelected[i], this->scope == i);
	}
	KZ::ui::SetBoolClass(entity, "profile_scope", "hidden", this->applied.scopeHidden, !IsGlobalAvailable() || !IsServerAvailable());
	this->layout.SetVar("profile_scope_0_label", "profile_scope_0_label", language->PrepareMessage("Course Top - Window Global").c_str());
	this->layout.SetVar("profile_scope_1_label", "profile_scope_1_label", language->PrepareMessage("Course Top - Window Server").c_str());

	// A status line replaces the player card until the player is loaded.
	const char *status = nullptr;
	switch (this->playerState)
	{
		case State::Loading:
			status = "Course Top - Window Loading";
			break;
		case State::NotFound:
			status = "Profile - Not Found";
			break;
		case State::Failed:
			status = "Profile - Failed";
			break;
		default:
			break;
	}
	KZ::ui::SetBoolClass(entity, "profile_status", "hidden", this->applied.statusHidden, !status);
	this->layout.SetVar("profile_status", "profile_status", status ? language->PrepareMessage(status).c_str() : "");
	KZ::ui::SetBoolClass(entity, "profile_card", "hidden", this->applied.cardHidden, !this->profile);
	KZ::ui::SetBoolClass(entity, "profile_runs", "hidden", this->applied.runsHidden, !this->profile);
	if (!this->profile)
	{
		return;
	}
	const f64 rating = this->profile->rating[KZ::api::GetModeIndex(this->mode)];
	const KZ::profile::RankBand &rank = KZ::profile::GetRankBand(rating);
	this->layout.SetVar("profile_name", "profile_name", this->profile->name.c_str());
	this->layout.SetVar("profile_steamid_tip", "profile_steamid_tip", language->PrepareMessage("Profile - Copy Steam ID").c_str());
	this->layout.SetVar("profile_joined_label", "profile_joined_label", language->PrepareMessage("Profile - Joined On").c_str());
	this->layout.SetVar("profile_joined", "profile_joined", this->profile->joined.empty() ? "-" : this->profile->joined.c_str());
	// Server profiles have no global rank or rating.
	// Unranked players have no rank line.
	const u32 globalRank = this->scope == Global ? this->profile->globalRank[KZ::api::GetModeIndex(this->mode)] : 0;
	KZ::ui::SetBoolClass(entity, "profile_global_line", "hidden", this->applied.globalLineHidden, globalRank == 0);
	this->layout.SetVar("profile_global_label", "profile_global_label", language->PrepareMessage("Profile - Global Rank").c_str());
	this->layout.SetVar("profile_global_rank", "profile_global_rank", tfm::format("#%u", globalRank).c_str());
	const char *globalRankClass = nullptr;
	if (globalRank == 1)
	{
		globalRankClass = "global-rank-1";
	}
	else if (globalRank < 20)
	{
		globalRankClass = "global-rank-20";
	}
	else if (globalRank < 100)
	{
		globalRankClass = "global-rank-100";
	}
	else if (globalRank < 500)
	{
		globalRankClass = "global-rank-500";
	}
	KZ::ui::SetSwapClass(entity, "profile_global_rank", this->applied.globalRankClass, globalRankClass);
	KZ::ui::SetBoolClass(entity, "profile_rank_line", "hidden", this->applied.rankLineHidden, this->scope == Server);
	this->layout.SetVar("profile_rank_label", "profile_rank_label", language->PrepareMessage("Profile - Rating").c_str());
	this->layout.SetVar("profile_rank", "profile_rank", rank.name);
	KZ::ui::SetSwapClass(entity, "profile_rank", this->applied.rankClass, rank.className);
	this->layout.SetVar("profile_rating", "profile_rating", KZ::records::FormatThousands(rating).c_str());

	const bool pro = this->type == Pro;
	KZ::ui::SetBoolClass(entity, "profile_runs", "server", this->applied.runsServer, this->scope == Server);
	this->layout.SetVar("profile_runs_title", "profile_runs_title", language->PrepareMessage("Profile - Runs").c_str());
	this->layout.SetVar("profile_th_map", "profile_th_map", language->PrepareMessage("Profile - Map").c_str());
	this->layout.SetVar("profile_th_course", "profile_th_course", language->PrepareMessage("Profile - Course").c_str());
	this->layout.SetVar("profile_th_tier", "profile_th_tier", language->PrepareMessage(pro ? "Profile - Pro Tier" : "Profile - Tier").c_str());
	this->layout.SetVar("profile_th_time", "profile_th_time", language->PrepareMessage("Course Top - Window Time").c_str());
	this->layout.SetVar("profile_th_points", "profile_th_points",
						language->PrepareMessage(pro ? "Course Top - Window Pro Points" : "Course Top - Window Overall Points").c_str());
	this->layout.SetVar("profile_th_rank", "profile_th_rank",
						language->PrepareMessage(pro ? "Profile - Pro Rank" : "Profile - Overall Rank").c_str());
	this->layout.SetVar("profile_th_date", "profile_th_date", language->PrepareMessage("Course Top - Window Date").c_str());
	this->layout.SetVar("profile_th_replay", "profile_th_replay", language->PrepareMessage("Course Top - Window Replay").c_str());
	const std::string replayTip = language->PrepareMessage("Course Top - Window Watch Replay");
	const std::string noReplayTip = language->PrepareMessage("Course Top - Window No Replay");
	const std::string otherMapTip = language->PrepareMessage("Course Top - Window Other Map");
	const std::string copyTip = language->PrepareMessage("Course Top - Window Copy ID");
	const std::string currentMap = g_pKZUtils->GetCurrentMapName();
	for (i32 i = 0; i < KZ_PROFILE_ROWS; i++)
	{
		const bool shown = i < (i32)this->runs.size();
		KZ::ui::SetBoolClass(entity, Row(i), "hidden", this->applied.rowHidden[i], !shown);
		if (!shown)
		{
			continue;
		}
		const KZ::api::Record &run = this->runs[i];
		const auto &tier = pro ? run.course.proTier : run.course.nubTier;
		const i32 tierNumber = tier ? (i32)*tier : 0;
		const u32 runRank = pro ? run.proRank : run.nubRank;
		this->layout.SetVar(RowMap(i), RowMap(i), run.map.name.c_str());
		this->layout.SetVar(RowCourse(i), RowCourse(i), run.course.name.c_str());
		this->layout.SetVar(RowTier(i), RowTier(i), tierNumber ? std::to_string(tierNumber).c_str() : "-");
		KZ::ui::SetSwapClass(entity, RowTier(i), this->applied.rowTier[i],
							 tierNumber >= 1 && tierNumber <= (i32)KZ_ARRAYSIZE(tierClasses) ? tierClasses[tierNumber - 1] : nullptr);
		this->layout.SetVar(RowTime(i), RowTime(i), KZ::records::FormatRankingTime(run.time).c_str());
		this->layout.SetVar(RowBadge(i), RowBadge(i), run.teleports > 0 ? "TP" : "PRO");
		KZ::ui::SetSwapClass(entity, RowBadge(i), this->applied.rowBadge[i], run.teleports > 0 ? "tone-tp" : "tone-pro");
		// Points are yellow at the maximum and red from 9,000 up.
		const f64 points = pro ? run.proPoints : run.nubPoints;
		this->layout.SetVar(RowPoints(i), RowPoints(i), points > 0 ? KZ::records::FormatThousands(points).c_str() : "-");
		KZ::ui::SetSwapClass(entity, RowPoints(i), this->applied.rowPoints[i],
							 points >= 10000 ? "points-top" : (points >= 9000 ? "points-high" : nullptr));
		this->layout.SetVar(RowRank(i), RowRank(i), runRank ? std::to_string(runRank).c_str() : "-");
		KZ::ui::SetSwapClass(entity, RowRank(i), this->applied.rowRank[i],
							 runRank == 1 ? "rank-first" : (runRank && runRank <= 10 ? "rank-top" : nullptr));
		this->layout.SetVar(RowDate(i), RowDate(i), KZ::records::FormatAgo(this->player, run.id).c_str());
		this->layout.SetVar(RowDateTip(i), RowDateTip(i), KZ::records::FormatDate(run.id).c_str());
		// Old server runs have no UUID to copy.
		const bool replay = this->HasReplay(i);
		const bool onMap = KZ_STREQI(run.map.name.c_str(), currentMap.c_str());
		KZ::ui::SetBoolClass(entity, RowReplay(i), "disabled", this->applied.rowReplayDisabled[i], !replay || !onMap);
		KZ::ui::SetBoolClass(entity, RowCopy(i), "hidden", this->applied.rowCopyHidden[i], run.id.empty());
		this->layout.SetVar(RowReplayTip(i), RowReplayTip(i), !replay ? noReplayTip.c_str() : (onMap ? replayTip.c_str() : otherMapTip.c_str()));
		this->layout.SetVar(RowCopyTip(i), RowCopyTip(i), copyTip.c_str());
	}

	const char *runsStatus = nullptr;
	if (this->runsState == State::Loading && this->runs.empty())
	{
		runsStatus = "Course Top - Window Loading";
	}
	else if (this->runsState == State::Failed)
	{
		runsStatus = "Course Top - Window Failed";
	}
	else if (this->runs.empty())
	{
		runsStatus = "Course Top - Window No Data";
	}
	KZ::ui::SetBoolClass(entity, "profile_runs_status", "hidden", this->applied.runsStatusHidden, !runsStatus);
	this->layout.SetVar("profile_runs_status", "profile_runs_status", runsStatus ? language->PrepareMessage(runsStatus).c_str() : "");

	const bool loading = this->runsState == State::Loading;
	std::string count;
	if (loading && !this->runs.empty())
	{
		count = language->PrepareMessage("Course Top - Window Loading");
	}
	else if (!this->runs.empty())
	{
		const std::string first = KZ::records::FormatThousands((f64)this->offset + 1);
		const std::string last = KZ::records::FormatThousands((f64)(this->offset + this->runs.size()));
		count = language->PrepareMessage("Course Top - Window Count", first.c_str(), last.c_str(),
										 KZ::records::FormatThousands((f64)this->total).c_str());
	}
	this->layout.SetVar("profile_count", "profile_count", count.c_str());
	this->layout.SetVar("profile_prev_label", "profile_prev_label", language->PrepareMessage("Course Top - Window Previous").c_str());
	this->layout.SetVar("profile_next_label", "profile_next_label", language->PrepareMessage("Course Top - Window Next").c_str());
	KZ::ui::SetBoolClass(entity, "profile_prev", "disabled", this->applied.prevDisabled, this->offset == 0 || loading);
	KZ::ui::SetBoolClass(entity, "profile_next", "disabled", this->applied.nextDisabled, this->offset + KZ_PROFILE_ROWS >= this->total || loading);
}

bool ProfileWindow::HasReplay(i32 row)
{
	return this->scope == Global || (row < (i32)this->runReplays.size() && this->runReplays[row]);
}

void ProfileWindow::OnClick(const char *buttonId)
{
	if (KZ_STREQ(buttonId, "profile_window_close"))
	{
		this->player->uiService->Close(this);
		return;
	}
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(KZ::api::modes); i++)
	{
		if (KZ_STREQ(buttonId, ModeButton(i)) && this->mode != KZ::api::modes[i])
		{
			this->mode = KZ::api::modes[i];
			this->offset = 0;
			this->runs.clear();
			this->RequestRuns();
			this->Render();
			return;
		}
	}
	for (i32 i = 0; i < TYPE_COUNT; i++)
	{
		if (KZ_STREQ(buttonId, TypeButton(i)) && this->type != i)
		{
			this->type = (Type)i;
			this->offset = 0;
			this->runs.clear();
			this->RequestRuns();
			this->Render();
			return;
		}
	}
	for (i32 i = 0; i < SCOPE_COUNT; i++)
	{
		if (KZ_STREQ(buttonId, ScopeButton(i)) && this->scope != i)
		{
			// Show the same player in the other scope by SteamID if known and by the searched name otherwise.
			if (this->profile)
			{
				this->identifier = std::to_string(this->profile->steamId);
			}
			this->scope = (Scope)i;
			this->offset = 0;
			this->profile.reset();
			this->runs.clear();
			this->total = 0;
			this->playerState = State::Loading;
			this->runsState = State::Loading;
			this->RequestPlayer();
			this->Render();
			return;
		}
	}
	if (!this->profile)
	{
		return;
	}
	if (KZ_STREQ(buttonId, "profile_steamid"))
	{
		const u64 steamId = this->profile->steamId;
		const u64 account = steamId - 76561197960265728ull;
		const std::string id64 = std::to_string(steamId);
		const std::string id = tfm::format("STEAM_1:%llu:%llu", account & 1, account >> 1);
		const char *name = this->profile->name.c_str();
		this->player->languageService->PrintChat(true, false, "Profile - Steam ID", name, id64.c_str(), id.c_str());
		this->player->languageService->PrintConsole(true, false, "Profile - Steam ID", name, id64.c_str(), id.c_str());
		return;
	}
	if (KZ_STREQ(buttonId, "profile_prev") || KZ_STREQ(buttonId, "profile_next"))
	{
		const bool next = KZ_STREQ(buttonId, "profile_next");
		if (this->runsState == State::Loading || (next ? this->applied.nextDisabled : this->applied.prevDisabled))
		{
			return;
		}
		this->offset = next ? this->offset + KZ_PROFILE_ROWS : (this->offset > KZ_PROFILE_ROWS ? this->offset - KZ_PROFILE_ROWS : 0);
		this->RequestRuns();
		this->Render();
		return;
	}
	for (i32 i = 0; i < KZ_PROFILE_ROWS && i < (i32)this->runs.size(); i++)
	{
		const std::string id = this->runs[i].id;
		if (KZ_STREQ(buttonId, RowMapButton(i)) || KZ_STREQ(buttonId, RowCourseButton(i)))
		{
			// Opens the course leaderboard in the shown mode and leaderboard type.
			const KZ::api::Record &run = this->runs[i];
			this->player->uiService->GetWindow<CourseTopWindow>()->ShowCourse(run.map.name, run.course.name, this->mode, this->type == Pro,
																			  this->scope == Server);
			return;
		}
		if (KZ_STREQ(buttonId, RowReplay(i)))
		{
			if (!this->HasReplay(i) || !KZ_STREQI(this->runs[i].map.name.c_str(), g_pKZUtils->GetCurrentMapName().c_str()))
			{
				return;
			}
			this->player->uiService->CloseAll(KZ::ui::CloseReason::Closed);
			KZ::replaysystem::commands::LoadReplay(this->player, id.c_str());
			return;
		}
		if (KZ_STREQ(buttonId, RowCopy(i)))
		{
			const char *name = this->profile->name.c_str();
			this->player->languageService->PrintChat(true, false, "Course Top - Record ID", name, id.c_str());
			this->player->languageService->PrintConsole(true, false, "Course Top - Record ID", name, id.c_str());
			return;
		}
	}
}
