#include "base_request.h"
#include "kz/timer/hud/course_top.h"
#include "kz/timer/kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/replays/kz_replaysystem.h"

#include "utils/simplecmds.h"

#include "vendor/sql_mm/src/public/sql_mm.h"
// clang-format off
#define COURSE_TOP_TABLE_KEY "Course Top - Table Name (Server Overall)"
static_global const char *columnKeysLocal[] = {
	"#",
	"Course Top Header - Player Alias",
	"Course Top Header - Time",
	"Course Top Header - Teleports",
	"Course Top Header - SteamID64",
	"Course Top Header - Run ID"
};

#define COURSE_TOP_PRO_TABLE_KEY "Course Top - Table Name (Server Pro)"
static_global const char *columnKeysLocalPro[] = {
	"#",
	"Course Top Header - Player Alias",
	"Course Top Header - Time",
	"Course Top Header - SteamID64",
	"Course Top Header - Run ID"
};

#define COURSE_TOP_TABLE_KEY_GLOBAL "Course Top - Table Name (Global Overall)"
static_global const char *columnKeysGlobal[] = {
	"#",
	"Course Top Header - Player Alias",
	"Course Top Header - Time",
	"Course Top Header - Teleports",
	"Course Top Header - SteamID64",
	"Course Top Header - Points",
	"Course Top Header - Run ID"
};

#define COURSE_TOP_PRO_TABLE_KEY_GLOBAL "Course Top - Table Name (Global Pro)"
static_global const char *columnKeysGlobalPro[] = {
	"#",
	"Course Top Header - Player Alias",
	"Course Top Header - Time",
	"Course Top Header - SteamID64",
	"Course Top Header - Points",
	"Course Top Header - Run ID",
};

// clang-format on

struct CourseTopRequest : public BaseRequest
{
	using BaseRequest::BaseRequest;

	static constexpr u64 ctopFeatures =
		RequestFeature::Course | RequestFeature::Map | RequestFeature::Mode | RequestFeature::Offset | RequestFeature::Limit;

	// "!ctop kz_grotto" asks for that map.
	virtual const char *GetBareKey() override
	{
		return "map";
	}

	struct RunStats
	{
		std::string runID;
		std::string name;
		u64 teleportsUsed;
		f64 time;
		u64 steamid64;
		u64 points {}; // 0 for local database
		u32 maxRank {};

		std::string GetName()
		{
			return name;
		}

		std::string GetRunID()
		{
			return runID;
		}

		std::string GetTeleportCount()
		{
			std::string fmt = tfm::format("%llu", teleportsUsed);
			return fmt;
		}

		std::string GetTime()
		{
			return utils::FormatTime(time);
		}

		std::string GetSteamID64()
		{
			std::string fmt = tfm::format("%llu", steamid64);
			return fmt;
		}

		std::string GetPoints()
		{
			std::string fmt = tfm::format("%llu", points);
			return fmt;
		}
	};

	struct CourseTopData
	{
		std::vector<RunStats> overallData;
		std::vector<RunStats> proData;
	} srData, wrData;

	// Set if the reply goes to the player's CourseTopWindow instead of the console.
	bool toWindow {};
	u32 windowSerial {};

	CourseTopWindow::Query GetWindowQuery()
	{
		return {this->mapName,
				this->courseName,
				this->modeName,
				this->offset,
				this->localStatus != ResponseStatus::DISABLED,
				this->globalStatus != ResponseStatus::DISABLED};
	}

	// Sends the reply to the player's leaderboard window instead of printing tables if the window can open.
	void UseWindow(KZPlayer *player, bool queryGlobal)
	{
		// The window shows CKZ for any other current mode unless a mode was requested because the API only rates VNL and CKZ.
		KZ::api::Mode apiMode;
		if (!this->modeGiven && queryGlobal && KZGlobalService::IsAvailable() && !KZ::api::DecodeModeString(this->modeName, apiMode))
		{
			this->SetupMode("CKZ");
			this->globalStatus = ResponseStatus::ENABLED;
		}
		this->windowSerial = player->uiService->GetWindow<CourseTopWindow>()->Begin(this->GetWindowQuery());
		if (this->windowSerial)
		{
			this->toWindow = true;
			this->limit = KZ_COURSE_TOP_ROWS;
		}
	}

	static void CreateForWindow(KZPlayer *player, const CourseTopWindow::Query &query, u32 serial)
	{
		CourseTopRequest *req = new CourseTopRequest(idCount++, player);
		req->features = ctopFeatures;
		req->localStatus = query.local && KZDatabaseService::IsReady() ? ResponseStatus::ENABLED : ResponseStatus::DISABLED;
		req->globalStatus = query.global && KZGlobalService::IsAvailable() ? ResponseStatus::ENABLED : ResponseStatus::DISABLED;
		req->mapName = query.map;
		req->courseName = query.course;
		req->SetupMode(query.mode);
		req->offset = query.offset;
		req->limit = KZ_COURSE_TOP_ROWS;
		req->toWindow = true;
		req->windowSerial = serial;
		instances.push_back(req);
	}

	virtual void PrintInstructions() override
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
		if (!player)
		{
			return;
		}
		player->languageService->PrintChat(true, false, "Course Top Command Usage");
		player->languageService->PrintConsole(false, false, "Course Top Command Usage - Console");
	}

	virtual void QueryLocal()
	{
		if (this->requestingFirstCourse)
		{
			return;
		}
		if (this->localStatus == ResponseStatus::ENABLED)
		{
			this->localStatus = ResponseStatus::PENDING;

			KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
			if (!callingPlayer)
			{
				this->Invalidate();
				return;
			}

			this->localStatus = ResponseStatus::PENDING;

			u64 uid = this->uid;

			auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
			{
				CourseTopRequest *req = (CourseTopRequest *)CourseTopRequest::Find(uid);
				if (!req)
				{
					return;
				}

				req->localStatus = ResponseStatus::RECEIVED;
				ISQLResult *result = queries[0]->GetResultSet();
				if (result && result->GetRowCount() > 0)
				{
					while (result->FetchRow())
					{
						req->srData.overallData.push_back({utils::OrEmpty(result->GetString(0)), utils::OrEmpty(result->GetString(2)),
														   (u64)result->GetInt64(4), result->GetFloat(3), (u64)result->GetInt64(1)});
					}
				}

				if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
				{
					while (result->FetchRow())
					{
						req->srData.proData.push_back({utils::OrEmpty(result->GetString(0)), utils::OrEmpty(result->GetString(2)), 0,
													   result->GetFloat(3), (u64)result->GetInt64(1)});
					}
				}

				// Each run carries the size of its leaderboard like the API's records.
				std::vector<RunStats> *boards[] = {&req->srData.overallData, &req->srData.proData};
				for (u32 i = 0; i < KZ_ARRAYSIZE(boards) && 2 + i < queries.size(); i++)
				{
					if (!(result = queries[2 + i]->GetResultSet()) || !result->FetchRow())
					{
						continue;
					}
					const u32 total = (u32)result->GetInt64(0);
					for (RunStats &run : *boards[i])
					{
						run.maxRank = total;
					}
				}
			};

			auto onQueryFailure = [uid](std::string, int)
			{
				CourseTopRequest *req = (CourseTopRequest *)CourseTopRequest::Find(uid);
				if (req)
				{
					req->localStatus = ResponseStatus::DISABLED;
				}
			};

			KZDatabaseService::QueryRecords(this->mapName, this->courseName, this->localModeID, this->limit, this->offset, onQuerySuccess,
											onQueryFailure, this->toWindow);
		}
	}

	virtual void QueryGlobal()
	{
		if (this->requestingFirstCourse)
		{
			return;
		}

		if (this->globalStatus == ResponseStatus::ENABLED)
		{
			this->globalStatus = ResponseStatus::PENDING;

			KZGlobalService::QueryCourseTopParams params;
			params.map = this->mapName;
			params.course = this->courseName;
			params.mode = this->apiMode;
			params.limit = this->limit;
			params.offset = this->offset;

			KZGlobalService::MessageCallback<KZ::api::messages::CourseTop> callback(CourseTopRequest::OnGlobalCourseTopQueried, this->uid);

			KZGlobalService::QueryCourseTop(std::move(params), std::move(callback));
		}
	}

	static void OnGlobalCourseTopQueried(const KZ::api::messages::CourseTop &ctop, u64 uid)
	{
		CourseTopRequest *req = (CourseTopRequest *)CourseTopRequest::Find(uid);
		if (!req)
		{
			return;
		}

		if (ctop.map.has_value() && ctop.course.has_value())
		{
			req->mapName = ctop.map->name.c_str();
			req->courseName = ctop.course->name.c_str();
			req->globalStatus = ResponseStatus::RECEIVED;
		}
		else
		{
			req->globalStatus = ResponseStatus::DISABLED;
			return;
		}
		for (const auto &record : ctop.overall)
		{
			req->wrData.overallData.push_back(
				{record.id, record.player.name, record.teleports, record.time, record.player.id, (u64)floor(record.nubPoints), record.nubMaxRank});
		}
		for (const auto &record : ctop.pro)
		{
			req->wrData.proData.push_back(
				{record.id, record.player.name, 0, record.time, record.player.id, (u64)floor(record.proPoints), record.proMaxRank});
		}
	}

	virtual void Reply()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}
		if (this->toWindow)
		{
			this->ReplyWindow(player);
			return;
		}

		if (localStatus != ResponseStatus::RECEIVED && globalStatus != ResponseStatus::RECEIVED)
		{
			player->languageService->PrintChat(true, false, "Course Top Request - Failed (Generic)");
			return;
		}

		player->languageService->PrintChat(true, false, "Course Top - Check Console");
		if (this->localStatus == ResponseStatus::RECEIVED)
		{
			this->ReplyLocal();
		}
		if (this->globalStatus == ResponseStatus::RECEIVED)
		{
			this->ReplyGlobal();
		}
	}

	void ReplyWindow(KZPlayer *player)
	{
		CourseTopWindow::Result result;
		result.received[CourseTopWindow::Global] = this->globalStatus == ResponseStatus::RECEIVED;
		result.received[CourseTopWindow::Server] = this->localStatus == ResponseStatus::RECEIVED;
		result.map = this->mapName;
		result.course = this->courseName;
		result.mode = this->modeName;
		result.offset = this->offset;
		// A server run's ID is its UUID but its replay only exists if this server kept the file.
		auto copyRuns = [](const std::vector<RunStats> &from, std::vector<CourseTopWindow::Run> &to, bool global)
		{
			for (const RunStats &stats : from)
			{
				std::string path;
				const bool replay = global || (!stats.runID.empty() && KZ::replaysystem::FindReplayPath(stats.runID.c_str(), path));
				to.push_back({stats.runID, replay, stats.name, stats.time, stats.teleportsUsed, global ? (f64)stats.points : -1, stats.maxRank,
							  stats.steamid64});
			}
		};
		copyRuns(this->wrData.overallData, result.runs[CourseTopWindow::Global][CourseTopWindow::Overall], true);
		copyRuns(this->wrData.proData, result.runs[CourseTopWindow::Global][CourseTopWindow::Pro], true);
		copyRuns(this->srData.overallData, result.runs[CourseTopWindow::Server][CourseTopWindow::Overall], false);
		copyRuns(this->srData.proData, result.runs[CourseTopWindow::Server][CourseTopWindow::Pro], false);
		player->uiService->GetWindow<CourseTopWindow>()->OnResult(this->windowSerial, result);

		// Server runs whose replays aren't on disk may be in the replay store.
		std::vector<std::pair<UUID_t, std::string>> missing;
		for (const auto &runs : result.runs[CourseTopWindow::Server])
		{
			for (const CourseTopWindow::Run &run : runs)
			{
				UUID_t uuid(false);
				if (!run.replay && UUID_t::FromString(run.id.c_str(), &uuid))
				{
					missing.push_back({uuid, this->mapName});
				}
			}
		}
		if (missing.empty())
		{
			return;
		}
		auto onLocated =
			[userID = this->userID, serial = this->windowSerial](std::unordered_map<UUID_t, KZ::replaysystem::ReplayAvailability> &&replays)
		{
			std::unordered_set<UUID_t> found;
			for (const auto &[uuid, availability] : replays)
			{
				if (availability.found)
				{
					found.insert(uuid);
				}
			}
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			if (player && !found.empty())
			{
				player->uiService->GetWindow<CourseTopWindow>()->OnReplaysLocated(serial, found);
			}
		};
		KZ::replaysystem::LocateReplays(missing, onLocated);
	}

	void ReplyGlobal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		std::string rank;

		// Overall table
		std::string headers[KZ_ARRAYSIZE(columnKeysGlobal)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeysGlobal); i++)
		{
			headers[i] = player->languageService->PrepareMessage(columnKeysGlobal[i]).c_str();
		}
		utils::Table<KZ_ARRAYSIZE(columnKeysGlobal)> table(
			player->languageService->PrepareMessage(COURSE_TOP_TABLE_KEY_GLOBAL, mapName.c_str(), courseName.c_str(), modeName.c_str()).c_str(),
			headers);
		for (size_t i = 0; i < wrData.overallData.size(); i++)
		{
			rank = tfm::format("%llu", this->offset + i + 1);
			RunStats stats = wrData.overallData[i];
			table.SetRow(i, rank, stats.GetName(), stats.GetTime(), stats.GetTeleportCount(), stats.GetSteamID64(), stats.GetPoints(),
						 stats.GetRunID());
		}
		player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
		player->PrintConsole(false, false, "%s", table.GetTitle().c_str());
		player->PrintConsole(false, false, "%s", table.GetHeader().c_str());

		for (u32 i = 0; i < table.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, "%s", table.GetLine(i).c_str());
		}
		player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());

		// Pro table
		std::string headersPro[KZ_ARRAYSIZE(columnKeysGlobalPro)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeysGlobalPro); i++)
		{
			headersPro[i] = player->languageService->PrepareMessage(columnKeysGlobalPro[i]).c_str();
		}
		utils::Table<KZ_ARRAYSIZE(columnKeysGlobalPro)> tablePro(
			player->languageService->PrepareMessage(COURSE_TOP_PRO_TABLE_KEY_GLOBAL, mapName.c_str(), courseName.c_str(), modeName.c_str()).c_str(),
			headersPro);
		for (size_t i = 0; i < wrData.proData.size(); i++)
		{
			rank = tfm::format("%llu", this->offset + i + 1);
			RunStats stats = wrData.proData[i];
			tablePro.SetRow(i, rank, stats.GetName(), stats.GetTime(), stats.GetSteamID64(), stats.GetPoints(), stats.GetRunID());
		}
		player->PrintConsole(false, false, "%s", tablePro.GetSeparator("=").c_str());
		player->PrintConsole(false, false, "%s", tablePro.GetTitle().c_str());
		player->PrintConsole(false, false, "%s", tablePro.GetHeader().c_str());
		for (u32 i = 0; i < tablePro.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, "%s", tablePro.GetLine(i).c_str());
		}
		player->PrintConsole(false, false, "%s", tablePro.GetSeparator("=").c_str());
	}

	void ReplyLocal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		std::string rank;

		// Overall table
		std::string headers[KZ_ARRAYSIZE(columnKeysLocal)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeysLocal); i++)
		{
			headers[i] = player->languageService->PrepareMessage(columnKeysLocal[i]).c_str();
		}
		utils::Table<KZ_ARRAYSIZE(columnKeysLocal)> table(
			player->languageService->PrepareMessage(COURSE_TOP_TABLE_KEY, mapName.c_str(), courseName.c_str(), modeName.c_str()).c_str(), headers);
		for (size_t i = 0; i < srData.overallData.size(); i++)
		{
			rank = tfm::format("%llu", this->offset + i + 1);
			RunStats stats = srData.overallData[i];
			table.SetRow(i, rank, stats.GetName(), stats.GetTime(), stats.GetTeleportCount(), stats.GetSteamID64(), stats.GetRunID());
		}
		player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
		player->PrintConsole(false, false, "%s", table.GetTitle().c_str());
		player->PrintConsole(false, false, "%s", table.GetHeader().c_str());

		for (u32 i = 0; i < table.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, "%s", table.GetLine(i).c_str());
		}
		player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
		// Pro table
		std::string headersPro[KZ_ARRAYSIZE(columnKeysLocalPro)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeysLocalPro); i++)
		{
			headersPro[i] = player->languageService->PrepareMessage(columnKeysLocalPro[i]).c_str();
		}
		utils::Table<KZ_ARRAYSIZE(columnKeysLocalPro)> tablePro(
			player->languageService->PrepareMessage(COURSE_TOP_PRO_TABLE_KEY, mapName.c_str(), courseName.c_str(), modeName.c_str()).c_str(),
			headersPro);
		for (size_t i = 0; i < srData.proData.size(); i++)
		{
			rank = tfm::format("%llu", this->offset + i + 1);
			RunStats stats = srData.proData[i];
			tablePro.SetRow(i, rank, stats.GetName(), stats.GetTime(), stats.GetSteamID64(), stats.GetRunID());
		}
		player->PrintConsole(false, false, "%s", tablePro.GetSeparator("=").c_str());
		player->PrintConsole(false, false, "%s", tablePro.GetTitle().c_str());
		player->PrintConsole(false, false, "%s", tablePro.GetHeader().c_str());
		for (u32 i = 0; i < tablePro.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, "%s", tablePro.GetLine(i).c_str());
		}
		player->PrintConsole(false, false, "%s", tablePro.GetSeparator("=").c_str());
	}
};

void CourseTopWindow::SendQuery(KZPlayer *player, const Query &query, u32 serial)
{
	CourseTopRequest::CreateForWindow(player, query, serial);
}

static void RequestCourseTop(KZPlayer *player, const CCommand *args, bool queryLocal, bool queryGlobal)
{
	CourseTopRequest *req = CourseTopRequest::Create<CourseTopRequest>(player, CourseTopRequest::ctopFeatures, queryLocal, queryGlobal, args);
	// Prefer using the custom HUD if it's a chat command unless the addon is unavailable.
	const bool chat = args->Arg(0)[0] == SCMD_CHAT_TRIGGER || args->Arg(0)[0] == SCMD_CHAT_SILENT_TRIGGER;
	if (!chat || !req->isValid)
	{
		return;
	}
	req->UseWindow(player, queryGlobal);
}

SCMD(kz_ctop, SCFL_RECORD | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	RequestCourseTop(player, args, true, true);
	return true;
}

SCMD_LINK(kz_coursetop, kz_ctop);
SCMD_LINK(kz_maptop, kz_ctop);

SCMD(kz_gctop, SCFL_RECORD | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	RequestCourseTop(player, args, false, true);
	return true;
}

SCMD_LINK(kz_gcoursetop, kz_gctop);
SCMD_LINK(kz_gmaptop, kz_gctop);

SCMD(kz_sctop, SCFL_TIMER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	RequestCourseTop(player, args, true, false);
	return true;
}

SCMD_LINK(kz_scoursetop, kz_sctop);
SCMD_LINK(kz_smaptop, kz_sctop);
