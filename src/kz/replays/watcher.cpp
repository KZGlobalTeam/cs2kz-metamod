#include <unordered_set>
#include "watcher.h"
#include "ledger.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/db/kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "kz/mode/kz_mode.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/timer/kz_timer.h"
#include "filesystem/ifilesystem.h"
#include "utils/tables.h"

namespace ledger = KZ::replaysystem::ledger;

#define CHEATER_REPLAY_TABLE_KEY "Cheater Replays - Table Name"
#define RUN_REPLAY_TABLE_KEY     "Run Replays - Table Name"
#define JUMP_REPLAY_TABLE_KEY    "Jumpstat Replays - Table Name"
#define MANUAL_REPLAY_TABLE_KEY  "Manual Replays - Table Name"

static_global const char *cheaterReplayTableHeaders[] = {"#.",
														 "Replay Table Header - Player",
														 "Replay Table Header - Map",
														 "Replay Table Header - Reason",
														 "Replay Table Header - SteamID",
														 "Replay Table Header - UUID"};
static_global const char *runReplayTableHeaders[] = {"#.",
													 "Replay Table Header - Player",
													 "Replay Table Header - Map",
													 "Replay Table Header - Course",
													 "Replay Table Header - Mode",
													 "Replay Table Header - Styles",
													 "Replay Table Header - Time",
													 "Replay Table Header - Teleports",
													 "Replay Table Header - SteamID",
													 "Replay Table Header - UUID"};
static_global const char *jumpReplayTableHeaders[] = {"#.",
													  "Replay Table Header - Player",
													  "Replay Table Header - Map",
													  "Replay Table Header - Mode",
													  "Replay Table Header - Jump Type",
													  "Replay Table Header - Distance",
													  "Replay Table Header - Block",
													  "Replay Table Header - Strafes",
													  "Replay Table Header - SteamID",
													  "Replay Table Header - UUID"};
static_global const char *manualReplayTableHeaders[] = {"#.",
														"Replay Table Header - Player",
														"Replay Table Header - Map",
														"Replay Table Header - Saved By",
														"Replay Table Header - SteamID",
														"Replay Table Header - UUID"};

struct PageRange
{
	size_t start;
	size_t end;
};

static_function PageRange GetPageRange(const ReplayFilterCriteria &criteria, size_t total)
{
	size_t startIndex = 0;
	if (criteria.offset > 0)
	{
		startIndex = static_cast<size_t>(criteria.offset);
	}
	if (startIndex > total)
	{
		startIndex = total;
	}
	const size_t limit = criteria.limit > 0 ? static_cast<size_t>(criteria.limit) : 0;
	const size_t endIndex = limit > 0 ? MIN(startIndex + limit, total) : startIndex;
	return {startIndex, endIndex};
}

// Grouping keys compare names case-insensitively.
// They are lowercased up front to keep the hash consistent with equality.
static_function std::string LowercaseKey(std::string str)
{
	V_strlower(str.data());
	return str;
}

template<typename MapT>
static_function std::vector<std::pair<UUID_t, ReplayHeader>> CollectMatchesWithMapFallback(const MapT &replayMap,
																						   const ReplayFilterCriteria &criteria, std::mutex &mutex)
{
	auto collectMatches = [&](bool exactMapMatch)
	{
		std::vector<std::pair<UUID_t, ReplayHeader>> matches;
		std::lock_guard<std::mutex> lock(mutex);
		for (auto &replay : replayMap)
		{
			if (criteria.PassFilters(replay.second, exactMapMatch))
			{
				matches.push_back(replay);
			}
		}
		return matches;
	};

	const bool preferExactMap = !criteria.mapName.empty() && criteria.mapName != "*";
	auto matches = collectMatches(preferExactMap);
	if (preferExactMap && matches.empty())
	{
		matches = collectMatches(false);
	}
	return matches;
}

void ReplayWatcher::FilterAndPrintMatchingCheaterReplays(ReplayFilterCriteria &criteria, KZPlayer *player)
{
	std::vector<std::pair<UUID_t, ReplayHeader>> matchingReplays =
		CollectMatchesWithMapFallback(this->cheaterReplays, criteria, this->replayMapsMutex);
	PageRange page = GetPageRange(criteria, matchingReplays.size());
	std::string headers[KZ_ARRAYSIZE(cheaterReplayTableHeaders)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(cheaterReplayTableHeaders); i++)
	{
		headers[i] = player->languageService->PrepareMessage(cheaterReplayTableHeaders[i]).c_str();
	}

	utils::Table<KZ_ARRAYSIZE(cheaterReplayTableHeaders)> table(player->languageService->PrepareMessage(CHEATER_REPLAY_TABLE_KEY).c_str(), headers);
	for (size_t i = page.start; i < page.end; i++)
	{
		auto &pair = matchingReplays[i];
		const ReplayHeader &hdr = pair.second;

		std::string rowNumber = std::to_string((i - page.start) + 1);
		std::string steamID = hdr.has_player() ? std::to_string(hdr.player().steamid64()) : "0";
		const char *reason = hdr.has_cheater() ? hdr.cheater().reason().c_str() : "";
		// clang-format off
		table.SetRow(i - page.start, rowNumber.c_str(),
					hdr.player().name().c_str(), 
					hdr.map().name().c_str(), 
					reason,
					steamID.c_str(),
					pair.first.ToString().c_str());
		// clang-format on
	}
	if (table.GetNumEntries() == 0)
	{
		player->languageService->PrintChat(true, false, "Replay List - No Matching Replays");
		return;
	}
	player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
	player->PrintConsole(false, false, "%s", table.GetTitle().c_str());
	player->PrintConsole(false, false, "%s", table.GetHeader().c_str());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, "%s", table.GetLine(i).c_str());
	}
	player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
}

void ReplayWatcher::FilterAndPrintMatchingRunReplays(ReplayFilterCriteria &criteria, KZPlayer *player)
{
	std::vector<std::pair<UUID_t, ReplayHeader>> matchingReplays = CollectMatchesWithMapFallback(this->runReplays, criteria, this->replayMapsMutex);

	std::sort(matchingReplays.begin(), matchingReplays.end(),
			  [](const auto &a, const auto &b) { return a.second.run().time() < b.second.run().time(); });

	PageRange page = GetPageRange(criteria, matchingReplays.size());

	std::string headers[KZ_ARRAYSIZE(runReplayTableHeaders)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(runReplayTableHeaders); i++)
	{
		headers[i] = player->languageService->PrepareMessage(runReplayTableHeaders[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(runReplayTableHeaders)> table(player->languageService->PrepareMessage(RUN_REPLAY_TABLE_KEY).c_str(), headers);
	for (size_t i = page.start; i < page.end; i++)
	{
		auto &pair = matchingReplays[i];
		const ReplayHeader &hdr = pair.second;
		if (!hdr.has_run())
		{
			continue;
		}
		auto &run = hdr.run();
		std::string modeName = run.mode().name();
		std::string stylesList;
		for (int styleIndex = 0; styleIndex < run.styles_size(); styleIndex++)
		{
			const auto &style = run.styles(styleIndex);
			const std::string &styleName = !style.short_name().empty() ? style.short_name() : style.name();
			if (styleName.empty())
			{
				continue;
			}
			if (!stylesList.empty())
			{
				stylesList += ", ";
			}
			stylesList += styleName;
		}
		if (stylesList.empty())
		{
			stylesList = "-";
		}
		std::string rowNumber = std::to_string((i - page.start) + 1);
		std::string teleports = std::to_string(run.num_teleports());
		std::string steamID = std::to_string(hdr.player().steamid64());
		// clang-format off
		table.SetRow(i - page.start, rowNumber.c_str(),
                        hdr.player().name().c_str(), 
                        hdr.map().name().c_str(),
                        run.course_name().c_str(), 
                        modeName.c_str(),
						stylesList.c_str(),
                        utils::FormatTime(run.time()).c_str(), teleports.c_str(), steamID.c_str(), pair.first.ToString().c_str());
		// clang-format on
	}
	if (table.GetNumEntries() == 0)
	{
		player->languageService->PrintChat(true, false, "Replay List - No Matching Replays");
		return;
	}
	player->languageService->PrintChat(true, false, "Replay List - Check Console");
	player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
	player->PrintConsole(false, false, "%s", table.GetTitle().c_str());
	player->PrintConsole(false, false, "%s", table.GetHeader().c_str());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, "%s", table.GetLine(i).c_str());
	}
	player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
}

void ReplayWatcher::FilterAndPrintMatchingJumpReplays(ReplayFilterCriteria &criteria, KZPlayer *player)
{
	std::vector<std::pair<UUID_t, ReplayHeader>> matchingReplays = CollectMatchesWithMapFallback(this->jumpReplays, criteria, this->replayMapsMutex);
	PageRange page = GetPageRange(criteria, matchingReplays.size());
	std::string headers[KZ_ARRAYSIZE(jumpReplayTableHeaders)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(jumpReplayTableHeaders); i++)
	{
		headers[i] = player->languageService->PrepareMessage(jumpReplayTableHeaders[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(jumpReplayTableHeaders)> table(player->languageService->PrepareMessage(JUMP_REPLAY_TABLE_KEY).c_str(), headers);
	for (size_t i = page.start; i < page.end; i++)
	{
		auto &pair = matchingReplays[i];
		const ReplayHeader &hdr = pair.second;
		if (!hdr.has_jump())
		{
			continue;
		}
		auto &jump = hdr.jump();
		u8 jt = (u8)jump.jump_type();
		if (jt >= JUMPTYPE_COUNT)
		{
			continue;
		}
		std::string rowNumber = std::to_string((i - page.start) + 1);
		std::string steamID = std::to_string(hdr.player().steamid64());
		// clang-format off
		table.SetRow(i - page.start, rowNumber.c_str(), 
                        hdr.player().name().c_str(), 
                        hdr.map().name().c_str(), 
                        jump.mode().name().c_str(), 
                        jumpTypeStr[jt],
                                
                        std::to_string(jump.distance()).c_str(), 
                        std::to_string(jump.block_distance()).c_str(),
                                
                        std::to_string(jump.num_strafes()).c_str(), 
                        steamID.c_str(), 
                        pair.first.ToString().c_str());
		// clang-format on
	}

	if (table.GetNumEntries() == 0)
	{
		player->languageService->PrintChat(true, false, "Replay List - No Matching Replays");
		return;
	}
	player->languageService->PrintChat(true, false, "Replay List - Check Console");
	player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
	player->PrintConsole(false, false, "%s", table.GetTitle().c_str());
	player->PrintConsole(false, false, "%s", table.GetHeader().c_str());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, "%s", table.GetLine(i).c_str());
	}
	player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
}

void ReplayWatcher::FilterAndPrintMatchingManualReplays(ReplayFilterCriteria &criteria, KZPlayer *player)
{
	std::vector<std::pair<UUID_t, ReplayHeader>> matchingReplays =
		CollectMatchesWithMapFallback(this->manualReplays, criteria, this->replayMapsMutex);
	PageRange page = GetPageRange(criteria, matchingReplays.size());
	std::string headers[KZ_ARRAYSIZE(manualReplayTableHeaders)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(manualReplayTableHeaders); i++)
	{
		headers[i] = player->languageService->PrepareMessage(manualReplayTableHeaders[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(manualReplayTableHeaders)> table(player->languageService->PrepareMessage(MANUAL_REPLAY_TABLE_KEY).c_str(), headers);
	for (size_t i = page.start; i < page.end; i++)
	{
		auto &pair = matchingReplays[i];
		const ReplayHeader &hdr = pair.second;
		if (!hdr.has_manual())
		{
			continue;
		}
		std::string rowNumber = std::to_string((i - page.start) + 1);
		std::string steamID = std::to_string(hdr.player().steamid64());
		const char *savedBy = (hdr.manual().has_saved_by() ? hdr.manual().saved_by().name().c_str() : "");
		// clang-format off
		table.SetRow(i - page.start, rowNumber.c_str(), 
                        hdr.player().name().c_str(), 
                        hdr.map().name().c_str(), 
                        savedBy, 
                        steamID.c_str(),
		                pair.first.ToString().c_str());
		// clang-format on
	}

	if (table.GetNumEntries() == 0)
	{
		player->languageService->PrintChat(true, false, "Replay List - No Matching Replays");
		return;
	}
	player->languageService->PrintChat(true, false, "Replay List - Check Console");
	player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
	player->PrintConsole(false, false, "%s", table.GetTitle().c_str());
	player->PrintConsole(false, false, "%s", table.GetHeader().c_str());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, "%s", table.GetLine(i).c_str());
	}
	player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
}

void ReplayWatcher::PrintUsage(KZPlayer *player)
{
	if (!player)
	{
		return;
	}
	player->languageService->PrintConsole(false, false, "Replay List - Usage Title");
	player->languageService->PrintConsole(false, false, "Replay List - Usage General Options");
	player->languageService->PrintConsole(false, false, "Replay List - Usage Type Options");
}

void ReplayWatcher::FindReplaysMatchingCriteria(const char *inputs, KZPlayer *player)
{
	ReplayFilterCriteria criteria = ParseCriteria(inputs, player);
	switch (criteria.type)
	{
		case RP_CHEATER:
			this->FilterAndPrintMatchingCheaterReplays(criteria, player);
			break;
		case RP_RUN:
			this->FilterAndPrintMatchingRunReplays(criteria, player);
			break;
		case RP_JUMPSTATS:
			this->FilterAndPrintMatchingJumpReplays(criteria, player);
			break;
		case RP_MANUAL:
			this->FilterAndPrintMatchingManualReplays(criteria, player);
			break;
		default:
			break;
	}
}

std::vector<std::pair<UUID_t, ReplayHeader>> ReplayWatcher::CollectMatches(const ReplayFilterCriteria &criteria)
{
	switch (criteria.type)
	{
		case RP_CHEATER:
			return CollectMatchesWithMapFallback(this->cheaterReplays, criteria, this->replayMapsMutex);
		case RP_RUN:
		{
			auto matches = CollectMatchesWithMapFallback(this->runReplays, criteria, this->replayMapsMutex);
			std::sort(matches.begin(), matches.end(), [](const auto &a, const auto &b) { return a.second.run().time() < b.second.run().time(); });
			return matches;
		}
		case RP_JUMPSTATS:
			return CollectMatchesWithMapFallback(this->jumpReplays, criteria, this->replayMapsMutex);
		case RP_MANUAL:
			return CollectMatchesWithMapFallback(this->manualReplays, criteria, this->replayMapsMutex);
		default:
			return {};
	}
}

ReplayFilterCriteria ReplayWatcher::ParseCriteria(const char *inputs, KZPlayer *player, std::optional<ReplayType> type)
{
	KeyValues3 params;
	utils::ParseArgsToKV3(inputs, params, const_cast<const char **>(generalParameters), KZ_ARRAYSIZE(generalParameters));
	ReplayFilterCriteria criteria;
	// General parameters
	KeyValues3 *kv = nullptr;
	// Type filter
	kv = params.FindMember("type");
	if (kv)
	{
		const char *typeStr = kv->GetString();
		if (!V_stricmp(typeStr, "cheater") || !V_stricmp(typeStr, "c"))
		{
			criteria.type = RP_CHEATER;
		}
		else if (!V_stricmp(typeStr, "run") || !V_stricmp(typeStr, "r"))
		{
			criteria.type = RP_RUN;
		}
		else if (!V_stricmp(typeStr, "jump") || !V_stricmp(typeStr, "j"))
		{
			criteria.type = RP_JUMPSTATS;
		}
		else if (!V_stricmp(typeStr, "manual") || !V_stricmp(typeStr, "m"))
		{
			criteria.type = RP_MANUAL;
		}
	}
	if (type)
	{
		criteria.type = *type;
	}
	// Player filter
	kv = params.FindMember("player");
	if (kv)
	{
		criteria.player.nameSubString = kv->GetString();
	}
	kv = params.FindMember("steamid");
	if (kv)
	{
		criteria.player.steamID = kv->GetUInt64();
	}
	// Map filter
	kv = params.FindMember("map");
	if (!kv)
	{
		kv = params.FindMember("m");
	}
	criteria.mapName = kv ? kv->GetString() : g_pKZUtils->GetCurrentMapName().c_str();

	// Offset filter
	kv = params.FindMember("offset");
	if (kv)
	{
		criteria.offset = kv->GetInt();
	}
	// Limit filter
	kv = params.FindMember("limit");
	if (kv)
	{
		criteria.limit = MIN(kv->GetInt(), 20);
	}
	switch (criteria.type)
	{
		case RP_CHEATER:
		{
			utils::ParseArgsToKV3(inputs, params, const_cast<const char **>(cheaterParameters), KZ_ARRAYSIZE(cheaterParameters));
			kv = params.FindMember("reason");
			if (kv)
			{
				criteria.reasonSubString = kv->GetString();
			}
			break;
		}
		case RP_RUN:
		{
			utils::ParseArgsToKV3(inputs, params, const_cast<const char **>(runParameters), KZ_ARRAYSIZE(runParameters));
			kv = params.FindMember("mode");
			criteria.modeNameSubString = kv ? kv->GetString() : player->modeService->GetModeName();
			kv = params.FindMember("maxtime");
			if (kv)
			{
				criteria.maxTime = kv->GetFloat();
			}
			kv = params.FindMember("maxteleports");
			if (kv)
			{
				criteria.maxTeleports = kv->GetInt();
			}
			kv = params.FindMember("numstyles");
			if (kv)
			{
				criteria.numStyles = kv->GetInt();
			}
			kv = params.FindMember("course");
			if (!kv)
			{
				kv = params.FindMember("c");
			}
			if (kv)
			{
				criteria.courseName = kv->GetString();
			}
			else if (player->timerService->GetCourse())
			{
				criteria.courseName = player->timerService->GetCourse()->name;
			}
			break;
		}
		case RP_JUMPSTATS:
		{
			utils::ParseArgsToKV3(inputs, params, const_cast<const char **>(jumpParameters), KZ_ARRAYSIZE(jumpParameters));
			kv = params.FindMember("mode");
			criteria.modeNameSubString = kv ? kv->GetString() : player->modeService->GetModeName();
			kv = params.FindMember("mindistance");
			if (!kv)
			{
				kv = params.FindMember("mindist");
			}
			if (kv)
			{
				criteria.minDistance = kv->GetFloat();
			}
			kv = params.FindMember("jumptype");
			if (!kv)
			{
				kv = params.FindMember("jt");
			}
			if (kv)
			{
				criteria.jumpType = (u8)kv->GetInt();
			}
			break;
		}
		case RP_MANUAL:
		{
			utils::ParseArgsToKV3(inputs, params, const_cast<const char **>(manualParameters), KZ_ARRAYSIZE(manualParameters));
			kv = params.FindMember("saver");
			if (!kv)
			{
				kv = params.FindMember("s");
			}
			if (kv)
			{
				criteria.savedBy.nameSubString = kv->GetString();
			}
			kv = params.FindMember("ssid");
			if (!kv)
			{
				kv = params.FindMember("saversteamid");
			}
			if (kv)
			{
				criteria.savedBy.steamID = kv->GetUInt64();
			}
			break;
		}
		default:
			break;
	}
	return criteria;
}

bool ReplayFilterCriteria::PassGeneralFilters(const ReplayHeader &header, bool exactMapMatch) const
{
	if (player.nameSubString.has_value()
		&& (!header.has_player() || !V_stristr(header.player().name().c_str(), player.nameSubString.value().c_str())))
	{
		return false;
	}
	if (player.steamID.has_value() && (!header.has_player() || header.player().steamid64() != player.steamID.value()))
	{
		return false;
	}
	if (mapName != "*")
	{
		if (!header.has_map())
		{
			return false;
		}
		if (exactMapMatch)
		{
			if (V_stricmp(header.map().name().c_str(), mapName.c_str()) != 0)
			{
				return false;
			}
		}
		else if (!V_stristr(header.map().name().c_str(), mapName.c_str()))
		{
			return false;
		}
	}
	return true;
}

bool ReplayFilterCriteria::PassCheaterFilters(const ReplayHeader &header) const
{
	if (!header.has_cheater())
	{
		return false;
	}
	if (reasonSubString.has_value() && !V_stristr(header.cheater().reason().c_str(), reasonSubString.value().c_str()))
	{
		return false;
	}
	return true;
}

bool ReplayFilterCriteria::PassRunFilters(const ReplayHeader &header) const
{
	if (!header.has_run())
	{
		return false;
	}
	auto &run = header.run();
	if (!V_stristr(run.mode().name().c_str(), modeNameSubString.c_str()) && !V_stristr(run.mode().short_name().c_str(), modeNameSubString.c_str()))
	{
		return false;
	}
	if (maxTime >= 0.0f && run.time() > maxTime)
	{
		return false;
	}
	if (maxTeleports >= 0 && run.num_teleports() > maxTeleports)
	{
		return false;
	}
	if (numStyles >= 0 && run.styles_size() != numStyles)
	{
		return false;
	}
	if (courseName.has_value() && !V_stristr(run.course_name().c_str(), courseName.value().c_str()))
	{
		return false;
	}
	return true;
}

bool ReplayFilterCriteria::PassJumpFilters(const ReplayHeader &header) const
{
	if (!header.has_jump())
	{
		return false;
	}
	auto &jump = header.jump();
	if (!V_stristr(jump.mode().name().c_str(), modeNameSubString.c_str()) && !V_stristr(jump.mode().short_name().c_str(), modeNameSubString.c_str()))
	{
		return false;
	}
	if (jumpType != static_cast<u8>(-1) && jump.jump_type() != jumpType)
	{
		return false;
	}
	if (jump.distance() < minDistance)
	{
		return false;
	}
	return true;
}

bool ReplayFilterCriteria::PassManualFilters(const ReplayHeader &header) const
{
	if (!header.has_manual())
	{
		return false;
	}
	auto &manual = header.manual();
	if (!manual.has_saved_by())
	{
		return false;
	}
	if (savedBy.nameSubString.has_value() && !V_stristr(manual.saved_by().name().c_str(), savedBy.nameSubString.value().c_str()))
	{
		return false;
	}
	if (savedBy.steamID.has_value() && manual.saved_by().steamid64() != savedBy.steamID.value())
	{
		return false;
	}
	return true;
}

bool ReplayFilterCriteria::PassFilters(const ReplayHeader &header, bool exactMapMatch) const
{
	if (!PassGeneralFilters(header, exactMapMatch))
	{
		return false;
	}
	switch (static_cast<ReplayType>(header.type()))
	{
		case RP_CHEATER:
			return PassCheaterFilters(header);
		case RP_RUN:
			return PassRunFilters(header);
		case RP_JUMPSTATS:
			return PassJumpFilters(header);
		case RP_MANUAL:
			return PassManualFilters(header);
		default:
			return false;
	}
}

// The decision of the retention rules for one replay in one scan.
enum class Verdict : u8
{
	Keep,
	Drop
};

using RankedReplays = std::vector<std::pair<UUID_t, const ReplayHeader *>>;

// Keeps the fastest runs and the fastest runs without teleports of each player, map, course and mode among the runs without styles.
static_function void RankRunReplays(const RankedReplays &runs, std::unordered_map<UUID_t, Verdict> &verdicts)
{
	struct Key
	{
		u64 steamid64;
		std::string mapName;
		std::string modeName;
		std::string courseName;

		bool operator==(const Key &o) const
		{
			return steamid64 == o.steamid64 && mapName == o.mapName && modeName == o.modeName && courseName == o.courseName;
		}
	};

	struct KeyHasher
	{
		size_t operator()(const Key &k) const
		{
			return std::hash<u64> {}(k.steamid64) ^ (std::hash<std::string> {}(k.mapName) << 1) ^ (std::hash<std::string> {}(k.modeName) << 2)
				   ^ (std::hash<std::string> {}(k.courseName) << 3);
		}
	};

	std::unordered_map<Key, RankedReplays, KeyHasher> groups;
	for (const auto &[uuid, hdr] : runs)
	{
		if (!hdr->has_run())
		{
			continue;
		}
		const auto &run = hdr->run();
		if (run.styles_size() > 0)
		{
			verdicts[uuid] = Verdict::Drop;
			continue;
		}
		Key key {hdr->player().steamid64(), LowercaseKey(hdr->map().name()), LowercaseKey(run.mode().name()), LowercaseKey(run.course_name())};
		groups[key].push_back({uuid, hdr});
	}
	const size_t maxPer = (size_t)MAX(KZOptionService::GetOptionInt("replays/disk/maxRunsPerGroup", 3), 1);
	for (auto &[key, group] : groups)
	{
		std::sort(group.begin(), group.end(), [](const auto &a, const auto &b) { return a.second->run().time() < b.second->run().time(); });
		size_t pro = 0;
		for (size_t i = 0; i < group.size(); i++)
		{
			const bool isPro = group[i].second->run().num_teleports() == 0;
			const bool keep = i < maxPer || (isPro && pro < maxPer);
			pro += isPro;
			verdicts[group[i].first] = keep ? Verdict::Keep : Verdict::Drop;
		}
	}
}

// Keeps the longest jumps and the best block jumps of each player, jump type and mode.
static_function void RankJumpReplays(const RankedReplays &jumps, std::unordered_map<UUID_t, Verdict> &verdicts)
{
	struct Key
	{
		u64 steamid64;
		u8 jumpType;
		std::string modeName;

		bool operator==(const Key &o) const
		{
			return steamid64 == o.steamid64 && jumpType == o.jumpType && modeName == o.modeName;
		}
	};

	struct KeyHasher
	{
		size_t operator()(const Key &k) const
		{
			return std::hash<u64> {}(k.steamid64) ^ (std::hash<u8> {}(k.jumpType) << 1) ^ (std::hash<std::string> {}(k.modeName) << 2);
		}
	};

	std::unordered_map<Key, RankedReplays, KeyHasher> groups;
	for (const auto &[uuid, hdr] : jumps)
	{
		if (hdr->has_jump())
		{
			const auto &jump = hdr->jump();
			groups[{hdr->player().steamid64(), (u8)jump.jump_type(), LowercaseKey(jump.mode().name())}].push_back({uuid, hdr});
		}
	}
	const size_t maxPer = (size_t)MAX(KZOptionService::GetOptionInt("replays/disk/maxJumpsPerCategory", 3), 0);
	for (auto &[key, group] : groups)
	{
		std::unordered_set<UUID_t> keep;
		// Ranked like the block list of the jump top.
		std::sort(group.begin(), group.end(),
				  [](const auto &a, const auto &b)
				  {
					  const auto &jumpA = a.second->jump();
					  const auto &jumpB = b.second->jump();
					  if (jumpA.block_distance() != jumpB.block_distance())
					  {
						  return jumpA.block_distance() > jumpB.block_distance();
					  }
					  return jumpA.distance() > jumpB.distance();
				  });
		for (size_t i = 0; i < MIN(maxPer, group.size()); i++)
		{
			if (group[i].second->jump().block_distance() > 0)
			{
				keep.insert(group[i].first);
			}
		}
		std::sort(group.begin(), group.end(), [](const auto &a, const auto &b) { return a.second->jump().distance() > b.second->jump().distance(); });
		for (size_t i = 0; i < MIN(maxPer, group.size()); i++)
		{
			keep.insert(group[i].first);
		}
		for (const auto &[uuid, hdr] : group)
		{
			verdicts[uuid] = keep.count(uuid) ? Verdict::Keep : Verdict::Drop;
		}
	}
}

// Keeps the newest manual saves of each player.
static_function void RankManualReplays(const RankedReplays &saves, std::unordered_map<UUID_t, Verdict> &verdicts)
{
	std::unordered_map<u64, RankedReplays> byPlayer;
	for (const auto &save : saves)
	{
		byPlayer[save.second->player().steamid64()].push_back(save);
	}
	const size_t maxPer = (size_t)MAX(KZOptionService::GetOptionInt("replays/maxManualSaves", 2), 0);
	for (auto &[steamID, group] : byPlayer)
	{
		std::sort(group.begin(), group.end(), [](const auto &a, const auto &b) { return a.second->timestamp() > b.second->timestamp(); });
		for (size_t i = 0; i < group.size(); i++)
		{
			verdicts[group[i].first] = i < maxPer ? Verdict::Keep : Verdict::Drop;
		}
	}
}

void ReplayWatcher::WatchLoop()
{
	// Long-lived background scanner.
	// The replay index maps it builds are the plugin's main steady-state allocator outside of recording.
	ScanReplays();
	while (this->running)
	{
		std::this_thread::sleep_for(std::chrono::seconds(5));
		if (!this->running)
		{
			break;
		}
		ScanReplays();
	}
}

// Recorders delete their own chunks when they finish.
// Anything left behind is therefore from a process that died mid-recording.
// A live recording only flushes a chunk every 15 minutes of recorded time.
// A full day of staleness cannot belong to one that is still running.
void ReplayWatcher::SweepOrphanedChunks(u64 currentTime)
{
	static constexpr u64 chunkMaxAgeSeconds = 24 * 60 * 60;

	char searchPath[MAX_PATH];
	V_snprintf(searchPath, sizeof(searchPath), "%s/*.chunk", KZ_REPLAY_PATH);

	FileFindHandle_t findHandle = {};
	for (const char *pFileName = g_pFullFileSystem->FindFirstEx(searchPath, "GAME", &findHandle); pFileName;
		 pFileName = g_pFullFileSystem->FindNext(findHandle))
	{
		if (g_pFullFileSystem->FindIsDirectory(findHandle))
		{
			continue;
		}

		char fullPath[MAX_PATH];
		V_snprintf(fullPath, sizeof(fullPath), "%s/%s", KZ_REPLAY_PATH, pFileName);
		long fileTime = g_pFullFileSystem->GetFileTime(fullPath, "GAME");
		if (fileTime <= 0 || currentTime < (u64)fileTime + chunkMaxAgeSeconds)
		{
			continue;
		}

		KZ_LOG_INFO(LogChannel::Replays, "Removing orphaned replay chunk %s\n", fullPath);
		utils::RemoveFile(fullPath);
	}
	g_pFullFileSystem->FindClose(findHandle);
}

void ReplayWatcher::ScanReplays()
{
	char searchPath[MAX_PATH];
	V_snprintf(searchPath, sizeof(searchPath), "%s/*.replay", KZ_REPLAY_PATH);

	FileFindHandle_t findHandle = {};
	const char *pFileName = g_pFullFileSystem->FindFirstEx(searchPath, "GAME", &findHandle);

	time_t currentUnixTime = 0;
	time(&currentUnixTime);
	const u64 now = (u64)currentUnixTime;
	const u64 retentionSeconds = (u64)MAX(KZOptionService::GetOptionInt("replays/disk/archiveRetentionMinutes", 2880), 0) * 60;

	SweepOrphanedChunks(now);

	// Files still present on disk this scan.
	// Anything cached but not in this set has been deleted externally and must be dropped from the cache.
	std::unordered_set<UUID_t> seen;

	while (pFileName)
	{
		if (!g_pFullFileSystem->FindIsDirectory(findHandle))
		{
			// Skip temporary files that are still being written
			if (V_strstr(pFileName, ".replay.tmp"))
			{
				pFileName = g_pFullFileSystem->FindNext(findHandle);
				continue;
			}

			char uuidStr[64];
			V_strncpy(uuidStr, pFileName, sizeof(uuidStr));
			char *ext = V_strstr(uuidStr, ".replay");
			if (ext)
			{
				*ext = '\0';
			}
			UUID_t uuid(false);
			if (UUID_t::FromString(uuidStr, &uuid))
			{
				char fullPath[MAX_PATH];
				V_snprintf(fullPath, sizeof(fullPath), "%s/%s", KZ_REPLAY_PATH, pFileName);
				seen.insert(uuid);

				long fileTime = g_pFullFileSystem->GetFileTime(fullPath, "GAME");
				auto cacheIt = this->replayCache.find(uuid);
				if (cacheIt != this->replayCache.end() && cacheIt->second.fileTime == fileTime)
				{
					// Unchanged since last scan - reuse the cached header instead of re-reading/re-parsing.
					pFileName = g_pFullFileSystem->FindNext(findHandle);
					continue;
				}

				FileHandle_t file = g_pFullFileSystem->Open(fullPath, "rb", "GAME");
				if (file)
				{
					u32 size = 0;
					if (g_pFullFileSystem->Read(&size, sizeof(size), file) == sizeof(size) && size > 0 && size < 5 * 1024 * 1024)
					{
						std::string buf;
						buf.resize(size);
						if (g_pFullFileSystem->Read(buf.data(), size, file) == size)
						{
							ReplayHeader hdr;
							if (hdr.ParseFromString(buf))
							{
								this->replayCache[uuid] = {std::move(hdr), fileTime};
							}
						}
					}
					g_pFullFileSystem->Close(file);
				}
			}
		}
		pFileName = g_pFullFileSystem->FindNext(findHandle);
	}

	g_pFullFileSystem->FindClose(findHandle);

	// Drop cache entries for files that no longer exist on disk.
	for (auto it = this->replayCache.begin(); it != this->replayCache.end();)
	{
		if (!seen.count(it->first))
		{
			it = this->replayCache.erase(it);
		}
		else
		{
			++it;
		}
	}

	std::unordered_map<UUID_t, bool> states;
	{
		std::lock_guard<std::mutex> lock(this->databaseMutex);
		for (auto it = this->databaseStates.begin(); it != this->databaseStates.end();)
		{
			it = seen.count(it->first) ? std::next(it) : this->databaseStates.erase(it);
		}
		states = this->databaseStates;
	}

	std::unordered_map<UUID_t, Verdict> verdicts;
	RankedReplays rankedRuns;
	RankedReplays rankedJumps;
	RankedReplays manualSaves;
	for (auto &[uuid, cached] : this->replayCache)
	{
		if (this->databaseMode)
		{
			auto stateIt = states.find(uuid);
			if (stateIt != states.end())
			{
				verdicts[uuid] = stateIt->second ? Verdict::Keep : Verdict::Drop;
			}
			continue;
		}
		const ReplayHeader &hdr = cached.header;
		const ReplayType type = static_cast<ReplayType>(hdr.type());
		if (type == RP_RUN)
		{
			rankedRuns.push_back({uuid, &hdr});
		}
		else if (type == RP_JUMPSTATS)
		{
			rankedJumps.push_back({uuid, &hdr});
		}
		else if (type == RP_MANUAL && hdr.has_player())
		{
			manualSaves.push_back({uuid, &hdr});
		}
	}
	RankRunReplays(rankedRuns, verdicts);
	RankJumpReplays(rankedJumps, verdicts);
	RankManualReplays(manualSaves, verdicts);
	for (const auto &[uuid, verdict] : verdicts)
	{
		if (verdict == Verdict::Keep)
		{
			ledger::UpdateEntry(uuid, [](ledger::Entry &entry) { entry.archivedAt = 0; });
		}
		else if (verdict == Verdict::Drop)
		{
			ledger::UpdateEntry(uuid,
								[now](ledger::Entry &entry)
								{
									if (!entry.archivedAt)
									{
										entry.archivedAt = now;
									}
								});
		}
	}

	// A file waiting for an upload stays even if the retention rules archived it.
	const auto entries = ledger::GetEntries();
	for (const auto &[uuid, entry] : entries)
	{
		if (!seen.count(uuid))
		{
			if (!entry.IsPending())
			{
				ledger::RemoveEntry(uuid);
			}
			continue;
		}
		auto verdictIt = verdicts.find(uuid);
		const bool expired =
			verdictIt != verdicts.end() && verdictIt->second == Verdict::Drop && entry.archivedAt && now >= entry.archivedAt + retentionSeconds;
		if (entry.IsPending() || (!entry.offloaded && !expired))
		{
			continue;
		}
		char fullPath[MAX_PATH];
		V_snprintf(fullPath, sizeof(fullPath), "%s/%s.replay", KZ_REPLAY_PATH, uuid.ToString().c_str());
		utils::RemoveFile(fullPath);
		ledger::RemoveEntry(uuid);
		this->replayCache.erase(uuid);
	}

	// Rebuild the typed maps from the (mostly cached, in-memory) header set.
	// No file I/O here.
	std::unordered_map<UUID_t, ReplayHeader> newCheater;
	std::map<UUID_t, ReplayHeader> newRun;
	std::map<UUID_t, ReplayHeader> newJump;
	std::unordered_map<UUID_t, ReplayHeader> newManual;
	for (auto &[uuid, cached] : this->replayCache)
	{
		const ReplayHeader &hdr = cached.header;
		switch (static_cast<ReplayType>(hdr.type()))
		{
			case RP_CHEATER:
				newCheater[uuid] = hdr;
				break;
			case RP_RUN:
				newRun[uuid] = hdr;
				break;
			case RP_JUMPSTATS:
				newJump[uuid] = hdr;
				break;
			case RP_MANUAL:
				newManual[uuid] = hdr;
				break;
			default:
				break;
		}
	}
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		this->cheaterReplays = std::move(newCheater);
		this->runReplays = std::move(newRun);
		this->jumpReplays = std::move(newJump);
		this->manualReplays = std::move(newManual);
	}

	ScanDownloadedReplays(now);
	QueueDatabaseChecks();
	ledger::Save();
}

void ReplayWatcher::QueueDatabaseChecks()
{
	if (!this->recheckRequested.exchange(false) || !this->databaseMode)
	{
		return;
	}
	std::map<ReplayType, std::vector<UUID_t>> checks;
	for (const auto &[uuid, cached] : this->replayCache)
	{
		checks[static_cast<ReplayType>(cached.header.type())].push_back(uuid);
	}
	std::lock_guard<std::mutex> lock(this->databaseMutex);
	this->databaseChecks = std::move(checks);
}

void ReplayWatcher::RequestDatabaseChecks()
{
	this->recheckRequested = true;
}

std::vector<UUID_t> ReplayWatcher::TakeDatabaseChecks(ReplayType type, u32 count)
{
	std::lock_guard<std::mutex> lock(this->databaseMutex);
	std::vector<UUID_t> &queue = this->databaseChecks[type];
	const size_t taken = MIN((size_t)count, queue.size());
	std::vector<UUID_t> batch(queue.end() - taken, queue.end());
	queue.resize(queue.size() - taken);
	return batch;
}

void ReplayWatcher::SetDatabaseStates(const std::vector<std::pair<UUID_t, bool>> &states)
{
	std::lock_guard<std::mutex> lock(this->databaseMutex);
	for (const auto &[uuid, state] : states)
	{
		this->databaseStates[uuid] = state;
	}
}

void ReplayWatcher::ScanDownloadedReplays(u64 currentTime)
{
	char searchPath[MAX_PATH];
	V_snprintf(searchPath, sizeof(searchPath), "%s/*.replay", KZ_REPLAY_DOWNLOADS_PATH);

	FileFindHandle_t findHandle = {};
	const char *pFileName = g_pFullFileSystem->FindFirstEx(searchPath, "GAME", &findHandle);
	// retentionDays == 0 means keep forever (no eviction).
	u32 retentionDays = (u32)MAX(KZOptionService::GetOptionInt("replays/downloadRetentionDays", 7), 0);
	u64 retentionSeconds = (u64)retentionDays * 24 * 3600;

	std::unordered_set<UUID_t> seen;

	while (pFileName)
	{
		if (!g_pFullFileSystem->FindIsDirectory(findHandle) && !V_strstr(pFileName, ".replay.tmp"))
		{
			char uuidStr[64];
			V_strncpy(uuidStr, pFileName, sizeof(uuidStr));
			char *ext = V_strstr(uuidStr, ".replay");
			if (ext)
			{
				*ext = '\0';
			}
			UUID_t uuid(false);
			if (UUID_t::FromString(uuidStr, &uuid))
			{
				char fullPath[MAX_PATH];
				V_snprintf(fullPath, sizeof(fullPath), "%s/%s", KZ_REPLAY_DOWNLOADS_PATH, pFileName);

				long fileTime = g_pFullFileSystem->GetFileTime(fullPath, "GAME");

				// Evict files whose download time (file mtime) is older than the retention period.
				if (retentionDays > 0 && fileTime > 0 && (u64)currentTime >= (u64)fileTime + retentionSeconds)
				{
					utils::RemoveFile(fullPath);
					this->downloadedReplayCache.erase(uuid);
					pFileName = g_pFullFileSystem->FindNext(findHandle);
					continue;
				}

				seen.insert(uuid);

				auto cacheIt = this->downloadedReplayCache.find(uuid);
				if (cacheIt != this->downloadedReplayCache.end() && cacheIt->second.fileTime == fileTime)
				{
					// Unchanged since last scan - reuse the cached header instead of re-reading/re-parsing.
					pFileName = g_pFullFileSystem->FindNext(findHandle);
					continue;
				}

				FileHandle_t file = g_pFullFileSystem->Open(fullPath, "rb", "GAME");
				if (file)
				{
					u32 size = 0;
					if (g_pFullFileSystem->Read(&size, sizeof(size), file) == sizeof(size) && size > 0 && size < 5 * 1024 * 1024)
					{
						std::string buf;
						buf.resize(size);
						if (g_pFullFileSystem->Read(buf.data(), size, file) == size)
						{
							ReplayHeader hdr;
							if (hdr.ParseFromString(buf))
							{
								this->downloadedReplayCache[uuid] = {std::move(hdr), fileTime};
							}
						}
					}
					g_pFullFileSystem->Close(file);
				}
			}
		}
		pFileName = g_pFullFileSystem->FindNext(findHandle);
	}

	g_pFullFileSystem->FindClose(findHandle);

	// Drop cache entries for files that no longer exist on disk.
	for (auto it = this->downloadedReplayCache.begin(); it != this->downloadedReplayCache.end();)
	{
		if (!seen.count(it->first))
		{
			it = this->downloadedReplayCache.erase(it);
		}
		else
		{
			++it;
		}
	}

	std::unordered_map<UUID_t, ReplayHeader> newDownloaded;
	newDownloaded.reserve(this->downloadedReplayCache.size());
	for (auto &[uuid, cached] : this->downloadedReplayCache)
	{
		newDownloaded[uuid] = cached.header;
	}

	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		this->downloadedReplays = std::move(newDownloaded);
	}
}

std::string ReplayWatcher::GetJumpReplayMap(const UUID_t &uuid)
{
	std::lock_guard<std::mutex> lock(this->replayMapsMutex);
	auto it = this->jumpReplays.find(uuid);
	return it == this->jumpReplays.end() ? std::string() : it->second.map().name();
}

std::optional<ReplayHeader> ReplayWatcher::GetReplayHeader(const UUID_t &uuid)
{
	std::lock_guard<std::mutex> lock(this->replayMapsMutex);
	std::optional<ReplayHeader> found;
	auto find = [&](const auto &replayMap)
	{
		auto it = replayMap.find(uuid);
		if (!found && it != replayMap.end())
		{
			found = it->second;
		}
	};
	find(this->runReplays);
	find(this->jumpReplays);
	find(this->manualReplays);
	find(this->cheaterReplays);
	find(this->downloadedReplays);
	return found;
}

std::vector<UUID_t> ReplayWatcher::FindReplaysByUUIDSubstring(const char *uuidSubstring)
{
	std::vector<UUID_t> matches;
	if (!uuidSubstring || !uuidSubstring[0])
	{
		return matches;
	}

	std::lock_guard<std::mutex> lock(this->replayMapsMutex);

	auto checkMap = [&](const auto &replayMap)
	{
		for (const auto &[uuid, header] : replayMap)
		{
			if (V_stristr(uuid.ToString().c_str(), uuidSubstring))
			{
				matches.push_back(uuid);
			}
		}
	};

	checkMap(this->cheaterReplays);
	checkMap(this->runReplays);
	checkMap(this->jumpReplays);
	checkMap(this->manualReplays);
	checkMap(this->downloadedReplays);

	return matches;
}

ReplayWatcher g_ReplayWatcher;

static_global class KZDatabaseServiceEventListener_Replays : public KZDatabaseServiceEventListener
{
public:
	virtual void OnDatabaseSetup()
	{
		g_ReplayWatcher.RequestDatabaseChecks();
	}
} databaseEventListener;

void KZ::replaysystem::InitWatcher()
{
	ledger::Load();
	// An upload to the API can't resume because its key only lived in memory.
	// A run still waiting for the API keeps the UUID it has.
	for (const auto &[uuid, entry] : ledger::GetEntries())
	{
		if (!entry.IsPending())
		{
			continue;
		}
		ledger::UpdateEntry(uuid,
							[](ledger::Entry &pending)
							{
								pending.waitingForUUID = false;
								pending.uploadToAPI = false;
							});
	}
	KZDatabaseService::RegisterEventListener(&databaseEventListener);
	g_ReplayWatcher.Start(KZDatabaseService::GetDatabaseConnection() != nullptr);
}

void KZ::replaysystem::CleanupWatcher()
{
	KZDatabaseService::UnregisterEventListener(&databaseEventListener);
	g_ReplayWatcher.Stop();
	ledger::Save();
}

void KZ::replaysystem::RequestDatabaseChecks()
{
	g_ReplayWatcher.RequestDatabaseChecks();
}

static_global constexpr u32 databaseCheckBatchSize = 100;
static_global u32 databaseChecksInFlight;

void KZ::replaysystem::ProcessDatabaseChecks()
{
	if (!KZDatabaseService::IsReady() || databaseChecksInFlight > 0)
	{
		return;
	}
	for (ReplayType type : {RP_RUN, RP_JUMPSTATS, RP_MANUAL, RP_CHEATER})
	{
		std::vector<UUID_t> batch = g_ReplayWatcher.TakeDatabaseChecks(type, databaseCheckBatchSize);
		if (batch.empty())
		{
			continue;
		}
		std::vector<std::string> uuids;
		for (const UUID_t &uuid : batch)
		{
			uuids.push_back(uuid.ToString());
		}
		auto onSuccess = [batch, type](std::vector<ISQLQuery *> queries)
		{
			databaseChecksInFlight--;
			std::unordered_map<UUID_t, bool> found;
			ISQLResult *result = queries[0]->GetResultSet();
			while (result && result->FetchRow())
			{
				UUID_t uuid(false);
				if (!result->IsNull(0) && UUID_t::FromString(utils::OrEmpty(result->GetString(0)), &uuid))
				{
					found[uuid] = type != RP_RUN || result->GetInt64(1) != 0;
				}
			}
			std::vector<std::pair<UUID_t, bool>> states;
			for (const UUID_t &uuid : batch)
			{
				auto it = found.find(uuid);
				states.push_back({uuid, it != found.end() && it->second});
			}
			g_ReplayWatcher.SetDatabaseStates(states);
		};
		auto onFailure = [](std::string error, int)
		{
			databaseChecksInFlight--;
			KZ_LOG_WARN(LogChannel::Replays, "Failed to check replays against the database (%s).\n", error.c_str());
		};
		databaseChecksInFlight++;
		switch (type)
		{
			case RP_RUN:
				KZDatabaseService::FindRunReplays(uuids, (i32)MAX(KZOptionService::GetOptionInt("replays/disk/maxRunsPerGroup", 3), 1), onSuccess,
												  onFailure);
				break;
			case RP_JUMPSTATS:
				KZDatabaseService::FindJumpReplays(uuids, onSuccess, onFailure);
				break;
			case RP_MANUAL:
				KZDatabaseService::FindManualReplays(uuids, onSuccess, onFailure);
				break;
			default:
				KZDatabaseService::FindCheaterReplays(uuids, onSuccess, onFailure);
				break;
		}
	}
}
