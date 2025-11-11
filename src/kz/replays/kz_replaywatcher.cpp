#include "kz_replaywatcher.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/mode/kz_mode.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/timer/kz_timer.h"
#include "filesystem.h"
#include "utils/tables.h"

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

void ReplayWatcher::FilterAndPrintMatchingCheaterReplays(ReplayFilterCriteria &criteria, KZPlayer *player)
{
	std::vector<std::pair<UUID_t, std::pair<GeneralReplayHeader, CheaterReplayHeader>>> matchingReplays;
	u64 numMatched = 0;
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		for (auto &replay : this->cheaterReplays)
		{
			if (criteria.PassFilters(replay.second.first, replay.second.second))
			{
				if (numMatched < criteria.offset)
				{
					numMatched++;
					continue;
				}
				// Found a matching replay
				matchingReplays.push_back(replay);
				if (numMatched++ >= criteria.offset + criteria.limit)
				{
					break;
				}
			}
		}
	}
	CUtlString headers[KZ_ARRAYSIZE(cheaterReplayTableHeaders)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(cheaterReplayTableHeaders); i++)
	{
		headers[i] = player->languageService->PrepareMessage(cheaterReplayTableHeaders[i]).c_str();
	}

	utils::Table<KZ_ARRAYSIZE(cheaterReplayTableHeaders)> table(player->languageService->PrepareMessage(CHEATER_REPLAY_TABLE_KEY).c_str(), headers);
	for (size_t i = 0; i < matchingReplays.size(); i++)
	{
		auto &pair = matchingReplays[i];
		const GeneralReplayHeader &generalHeader = pair.second.first;
		const CheaterReplayHeader &specificHeader = pair.second.second;

		std::string rowNumber = std::to_string(i + 1);
		std::string steamID = std::to_string(generalHeader.player.steamid64);
		// clang-format off
		table.SetRow(i, rowNumber.c_str(),
						generalHeader.player.name,
						generalHeader.map.name,
						specificHeader.reason,
						steamID.c_str(),
						matchingReplays[i].first.ToString().c_str());
		// clang-format on
	}

	if (table.GetNumEntries() == 0)
	{
		player->languageService->PrintChat(true, false, "Replay List - No Matching Replays");
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

void ReplayWatcher::FilterAndPrintMatchingRunReplays(ReplayFilterCriteria &criteria, KZPlayer *player)
{
	std::vector<std::pair<UUID_t, std::pair<GeneralReplayHeader, RunReplayHeader>>> matchingReplays;
	u64 numMatched = 0;
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		for (auto &replay : this->runReplays)
		{
			if (criteria.PassFilters(replay.second.first, replay.second.second))
			{
				if (numMatched < criteria.offset)
				{
					numMatched++;
					continue;
				}
				// Found a matching replay
				matchingReplays.push_back(replay);
				if (numMatched++ >= criteria.offset + criteria.limit)
				{
					break;
				}
			}
		}
	}
	CUtlString headers[KZ_ARRAYSIZE(runReplayTableHeaders)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(runReplayTableHeaders); i++)
	{
		headers[i] = player->languageService->PrepareMessage(runReplayTableHeaders[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(runReplayTableHeaders)> table(player->languageService->PrepareMessage(RUN_REPLAY_TABLE_KEY).c_str(), headers);
	for (size_t i = 0; i < matchingReplays.size(); i++)
	{
		auto &pair = matchingReplays[i];
		const GeneralReplayHeader &generalHeader = pair.second.first;
		const RunReplayHeader &specificHeader = pair.second.second;

		std::string modeName = specificHeader.mode.name;
		if (specificHeader.styleCount > 0)
		{
			modeName += " +";
			modeName += std::to_string(specificHeader.styleCount);
		}
		std::string rowNumber = std::to_string(i + 1);
		std::string teleportText = std::to_string(specificHeader.numTeleports);
		std::string steamID = std::to_string(generalHeader.player.steamid64);
		// clang-format off
		table.SetRow(i, rowNumber.c_str(),
						generalHeader.player.name,
						generalHeader.map.name,
						specificHeader.courseName,
						modeName.c_str(),
						utils::FormatTime(specificHeader.time).Get(),
						teleportText.c_str(),
						steamID.c_str(),
						matchingReplays[i].first.ToString().c_str());
		// clang-format on
	}
	if (table.GetNumEntries() == 0)
	{
		player->languageService->PrintChat(true, false, "Replay List - No Matching Replays");
		return;
	}
	player->languageService->PrintChat(true, false, "Replay List - Check Console");
	player->PrintConsole(false, false, table.GetSeparator("="));
	player->PrintConsole(false, false, table.GetTitle());
	player->PrintConsole(false, false, table.GetHeader());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, table.GetLine(i));
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
}

void ReplayWatcher::FilterAndPrintMatchingJumpReplays(ReplayFilterCriteria &criteria, KZPlayer *player)
{
	std::vector<std::pair<UUID_t, std::pair<GeneralReplayHeader, JumpReplayHeader>>> matchingReplays;
	u64 numMatched = 0;
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		for (auto &replay : this->jumpReplays)
		{
			if (criteria.PassFilters(replay.second.first, replay.second.second))
			{
				if (numMatched < criteria.offset)
				{
					numMatched++;
					continue;
				}
				// Found a matching replay
				matchingReplays.push_back(replay);
				if (numMatched++ >= criteria.offset + criteria.limit)
				{
					break;
				}
			}
		}
	}
	CUtlString headers[KZ_ARRAYSIZE(jumpReplayTableHeaders)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(jumpReplayTableHeaders); i++)
	{
		headers[i] = player->languageService->PrepareMessage(jumpReplayTableHeaders[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(jumpReplayTableHeaders)> table(player->languageService->PrepareMessage(JUMP_REPLAY_TABLE_KEY).c_str(), headers);
	for (size_t i = 0; i < matchingReplays.size(); i++)
	{
		auto &pair = matchingReplays[i];
		const GeneralReplayHeader &generalHeader = pair.second.first;
		const JumpReplayHeader &specificHeader = pair.second.second;
		if (specificHeader.jumpType < 0 || specificHeader.jumpType >= JUMPTYPE_COUNT)
		{
			continue;
		}
		std::string rowNumber = std::to_string(i + 1);
		std::string jumpType = jumpTypeStr[specificHeader.jumpType];
		std::string distance = std::to_string(specificHeader.distance);
		std::string blockDistance = std::to_string(specificHeader.blockDistance);
		std::string numStrafes = std::to_string(specificHeader.numStrafes);
		std::string steamID = std::to_string(generalHeader.player.steamid64);
		// clang-format off
		table.SetRow(i, rowNumber.c_str(),
						generalHeader.player.name,
						generalHeader.map.name,
						specificHeader.mode.name,
						jumpType.c_str(),
						distance.c_str(),
						blockDistance.c_str(),
						numStrafes.c_str(),
						steamID.c_str(),
						matchingReplays[i].first.ToString().c_str());
		// clang-format on
	}

	if (table.GetNumEntries() == 0)
	{
		player->languageService->PrintChat(true, false, "Replay List - No Matching Replays");
		return;
	}
	player->languageService->PrintChat(true, false, "Replay List - Check Console");
	player->PrintConsole(false, false, table.GetSeparator("="));
	player->PrintConsole(false, false, table.GetTitle());
	player->PrintConsole(false, false, table.GetHeader());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, table.GetLine(i));
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
}

void ReplayWatcher::FilterAndPrintMatchingManualReplays(ReplayFilterCriteria &criteria, KZPlayer *player)
{
	std::vector<std::pair<UUID_t, std::pair<GeneralReplayHeader, ManualReplayHeader>>> matchingReplays;
	u64 numMatched = 0;
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		for (auto &replay : this->manualReplays)
		{
			if (criteria.PassFilters(replay.second.first, replay.second.second))
			{
				if (numMatched < criteria.offset)
				{
					numMatched++;
					continue;
				}
				// Found a matching replay
				matchingReplays.push_back(replay);
				if (numMatched++ >= criteria.offset + criteria.limit)
				{
					break;
				}
			}
		}
	}
	CUtlString headers[KZ_ARRAYSIZE(manualReplayTableHeaders)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(manualReplayTableHeaders); i++)
	{
		headers[i] = player->languageService->PrepareMessage(manualReplayTableHeaders[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(manualReplayTableHeaders)> table(player->languageService->PrepareMessage(MANUAL_REPLAY_TABLE_KEY).c_str(), headers);
	for (size_t i = 0; i < matchingReplays.size(); i++)
	{
		auto &pair = matchingReplays[i];
		const GeneralReplayHeader &generalHeader = pair.second.first;
		const ManualReplayHeader &specificHeader = pair.second.second;

		std::string rowNumber = std::to_string(i + 1);
		std::string steamID = std::to_string(generalHeader.player.steamid64);
		// clang-format off
		table.SetRow(i, rowNumber.c_str(),
						generalHeader.player.name,
						generalHeader.map.name,
						specificHeader.savedBy.name,
						steamID.c_str(),
						matchingReplays[i].first.ToString().c_str());
		// clang-format on
	}

	if (table.GetNumEntries() == 0)
	{
		player->languageService->PrintChat(true, false, "Replay List - No Matching Replays");
		return;
	}
	player->languageService->PrintChat(true, false, "Replay List - Check Console");
	player->PrintConsole(false, false, table.GetSeparator("="));
	player->PrintConsole(false, false, table.GetTitle());
	player->PrintConsole(false, false, table.GetHeader());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, table.GetLine(i));
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
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
	// Parse inputs and filter replays accordingly.
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
	if (kv)
	{
		criteria.mapName = kv->GetString();
	}
	else
	{
		criteria.mapName = g_pKZUtils->GetCurrentMapName().Get();
	}
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

			this->FilterAndPrintMatchingCheaterReplays(criteria, player);

			break;
		}
		case RP_RUN:
		{
			utils::ParseArgsToKV3(inputs, params, const_cast<const char **>(runParameters), KZ_ARRAYSIZE(runParameters));
			kv = params.FindMember("mode");
			if (kv)
			{
				criteria.modeNameSubString = kv->GetString();
			}
			else
			{
				criteria.modeNameSubString = player->modeService->GetModeName();
			}
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

			this->FilterAndPrintMatchingRunReplays(criteria, player);
			break;
		}
		case RP_JUMPSTATS:
		{
			utils::ParseArgsToKV3(inputs, params, const_cast<const char **>(jumpParameters), KZ_ARRAYSIZE(jumpParameters));
			kv = params.FindMember("mode");
			if (kv)
			{
				criteria.modeNameSubString = kv->GetString();
			}
			else
			{
				criteria.modeNameSubString = player->modeService->GetModeName();
			}
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
				criteria.jumpType = static_cast<u8>(kv->GetInt());
			}

			this->FilterAndPrintMatchingJumpReplays(criteria, player);
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

			this->FilterAndPrintMatchingManualReplays(criteria, player);
			break;
		}
		default:
			break;
	}
}

bool ReplayFilterCriteria::PassGeneralFilters(const GeneralReplayHeader &header) const
{
	if (player.nameSubString.has_value() && !V_stristr(header.player.name, player.nameSubString.value().c_str()))
	{
		return false;
	}
	if (player.steamID.has_value() && player.steamID.value() != header.player.steamid64)
	{
		return false;
	}
	if (mapName != "*" && !V_stristr(header.map.name, mapName.c_str()))
	{
		return false;
	}
	return true;
}

bool ReplayFilterCriteria::PassCheaterFilters(const CheaterReplayHeader &header) const
{
	if (reasonSubString.has_value())
	{
		if (!V_stristr(header.reason, reasonSubString.value().c_str()))
		{
			return false;
		}
	}
	return true;
}

bool ReplayFilterCriteria::PassRunFilters(const RunReplayHeader &header) const
{
	if (!V_stristr(header.mode.name, this->modeNameSubString.c_str()) && !V_stristr(header.mode.shortName, this->modeNameSubString.c_str()))
	{
		return false;
	}
	if (this->maxTime >= 0.0f && header.time > this->maxTime)
	{
		return false;
	}
	if (this->maxTeleports >= 0 && header.numTeleports > this->maxTeleports)
	{
		return false;
	}
	if (this->numStyles >= 0 && header.styleCount != this->numStyles)
	{
		return false;
	}
	if (this->courseName.has_value() && !V_stristr(header.courseName, this->courseName.value().c_str()))
	{
		return false;
	}
	return true;
}

bool ReplayFilterCriteria::PassJumpFilters(const JumpReplayHeader &header) const
{
	if (!V_stristr(header.mode.name, this->modeNameSubString.c_str()) && !V_stristr(header.mode.shortName, this->modeNameSubString.c_str()))
	{
		return false;
	}
	if (this->jumpType != static_cast<u8>(-1) && header.jumpType != this->jumpType)
	{
		return false;
	}
	if (header.distance < this->minDistance)
	{
		return false;
	}
	return true;
}

bool ReplayFilterCriteria::PassManualFilters(const ManualReplayHeader &header) const
{
	if (savedBy.nameSubString.has_value() && !V_stristr(header.savedBy.name, savedBy.nameSubString.value().c_str()))
	{
		return false;
	}
	if (savedBy.steamID.has_value() && savedBy.steamID.value() != header.savedBy.steamid64)
	{
		return false;
	}
	return true;
}

void ReplayWatcher::UpdateArchivedReplayOnDisk(const UUID_t &uuid, GeneralReplayHeader &header, u64 archiveTimestamp)
{
	header.archivedTimestamp = archiveTimestamp;

	char fullPath[MAX_PATH];
	V_snprintf(fullPath, sizeof(fullPath), "%s/%s.replay", KZ_REPLAY_PATH, uuid.ToString().c_str());
	FileHandle_t file = g_pFullFileSystem->Open(fullPath, "r+b", "GAME");
	if (file)
	{
		g_pFullFileSystem->Write(&header, sizeof(header), file);
		g_pFullFileSystem->Close(file);
	}
}

void ReplayWatcher::ProcessCheaterReplays(std::unordered_map<UUID_t, std::pair<GeneralReplayHeader, CheaterReplayHeader>> &cheaterReplays,
										  u64 currentTime)
{
	// Mark as archived if reporter steamid is not 0 (server)
	for (auto &[uuid, pair] : cheaterReplays)
	{
		auto &[generalHeader, specificHeader] = pair;
		if (specificHeader.reporter.steamid64 != 0 && generalHeader.archivedTimestamp == 0)
		{
			UpdateArchivedReplayOnDisk(uuid, generalHeader, currentTime);
		}
	}
}

void ReplayWatcher::ProcessRunReplays(std::vector<std::tuple<UUID_t, GeneralReplayHeader, RunReplayHeader>> &tempRunReplays,
									  std::map<UUID_t, std::pair<GeneralReplayHeader, RunReplayHeader>> &runReplays, u64 currentTime)
{
	// Group by steamid, course, mode, and map
	struct RunReplayKey
	{
		u64 steamid64;
		std::string mapName;
		std::string modeName;
		std::string courseName;

		bool operator==(const RunReplayKey &other) const
		{
			return steamid64 == other.steamid64 && V_stricmp(mapName.c_str(), other.mapName.c_str()) == 0
				   && V_stricmp(modeName.c_str(), other.modeName.c_str()) == 0 && V_stricmp(courseName.c_str(), other.courseName.c_str()) == 0;
		}
	};

	struct RunReplayKeyHasher
	{
		std::size_t operator()(const RunReplayKey &k) const
		{
			std::size_t h1 = std::hash<u64> {}(k.steamid64);
			std::size_t h2 = std::hash<std::string> {}(k.mapName);
			std::size_t h3 = std::hash<std::string> {}(k.modeName);
			std::size_t h4 = std::hash<std::string> {}(k.courseName);
			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
		}
	};

	std::unordered_map<RunReplayKey, std::vector<std::tuple<UUID_t, GeneralReplayHeader, RunReplayHeader>>, RunReplayKeyHasher> runReplayGroups;

	for (auto &replay : tempRunReplays)
	{
		auto &[uuid, generalHeader, specificHeader] = replay;

		// Styled runs (styleCount > 0) are marked as archived immediately
		if (specificHeader.styleCount > 0 && generalHeader.archivedTimestamp == 0)
		{
			UpdateArchivedReplayOnDisk(uuid, generalHeader, currentTime);
		}

		// Group non-styled runs by steamid, course, mode, and map
		if (specificHeader.styleCount == 0)
		{
			RunReplayKey key;
			key.steamid64 = generalHeader.player.steamid64;
			key.mapName = generalHeader.map.name;
			key.modeName = specificHeader.mode.name;
			key.courseName = specificHeader.courseName;

			runReplayGroups[key].push_back(replay);
		}

		runReplays[uuid] = {generalHeader, specificHeader};
	}

	// For each group, keep top N by time + top N by time with 0 teleports, archive the rest
	i32 maxRunReplaysPerGroup = MAX(KZOptionService::GetOptionInt("maxRunReplaysPerGroup", 3), 2);
	for (auto &[key, replays] : runReplayGroups)
	{
		std::set<UUID_t> keepReplays;

		// Sort by time (fastest first) and keep top N
		std::sort(replays.begin(), replays.end(), [](const auto &a, const auto &b) { return std::get<2>(a).time < std::get<2>(b).time; });
		for (size_t i = 0; i < MIN((size_t)maxRunReplaysPerGroup, replays.size()); i++)
		{
			keepReplays.insert(std::get<0>(replays[i]));
		}

		// Filter to only runs with 0 teleports, sort by time (fastest first), and keep top N
		std::vector<std::tuple<UUID_t, GeneralReplayHeader, RunReplayHeader>> zeroTeleportReplays;
		for (auto &replay : replays)
		{
			if (std::get<2>(replay).numTeleports == 0)
			{
				zeroTeleportReplays.push_back(replay);
			}
		}
		std::sort(zeroTeleportReplays.begin(), zeroTeleportReplays.end(),
				  [](const auto &a, const auto &b) { return std::get<2>(a).time < std::get<2>(b).time; });
		for (size_t i = 0; i < MIN((size_t)maxRunReplaysPerGroup, zeroTeleportReplays.size()); i++)
		{
			keepReplays.insert(std::get<0>(zeroTeleportReplays[i]));
		}

		// Archive replays not in the keep set
		for (auto &[uuid, generalHeader, specificHeader] : replays)
		{
			if (keepReplays.find(uuid) == keepReplays.end() && generalHeader.archivedTimestamp == 0)
			{
				UpdateArchivedReplayOnDisk(uuid, generalHeader, currentTime);

				// Update in the map
				runReplays[uuid].first.archivedTimestamp = currentTime;
			}
		}
	}
}

void ReplayWatcher::ProcessJumpReplays(std::vector<std::tuple<UUID_t, GeneralReplayHeader, JumpReplayHeader>> &tempJumpReplays,
									   std::map<UUID_t, std::pair<GeneralReplayHeader, JumpReplayHeader>> &jumpReplays, u64 currentTime)
{
	// Group by steamid, jump type, and mode
	struct JumpReplayKey
	{
		u64 steamid64;
		u8 jumpType;
		std::string modeName;

		bool operator==(const JumpReplayKey &other) const
		{
			return steamid64 == other.steamid64 && jumpType == other.jumpType && V_stricmp(modeName.c_str(), other.modeName.c_str()) == 0;
		}
	};

	struct JumpReplayKeyHasher
	{
		std::size_t operator()(const JumpReplayKey &k) const
		{
			std::size_t h1 = std::hash<u64> {}(k.steamid64);
			std::size_t h2 = std::hash<u8> {}(k.jumpType);
			std::size_t h3 = std::hash<std::string> {}(k.modeName);
			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};

	std::unordered_map<JumpReplayKey, std::vector<std::tuple<UUID_t, GeneralReplayHeader, JumpReplayHeader>>, JumpReplayKeyHasher> jumpReplayGroups;

	for (auto &replay : tempJumpReplays)
	{
		auto &[uuid, generalHeader, specificHeader] = replay;

		JumpReplayKey key;
		key.steamid64 = generalHeader.player.steamid64;
		key.jumpType = specificHeader.jumpType;
		key.modeName = specificHeader.mode.name;

		jumpReplayGroups[key].push_back(replay);
		jumpReplays[uuid] = {generalHeader, specificHeader};
	}

	// For each group, keep top N by block distance + top N by distance, archive the rest
	i32 maxJumpReplaysPerCategory = MAX(KZOptionService::GetOptionInt("maxJumpReplaysPerCategory", 3), 2);
	for (auto &[key, replays] : jumpReplayGroups)
	{
		std::set<UUID_t> keepReplays;

		// Sort by block distance (descending) and keep top N
		std::sort(replays.begin(), replays.end(),
				  [](const auto &a, const auto &b) { return std::get<2>(a).blockDistance > std::get<2>(b).blockDistance; });
		for (size_t i = 0; i < MIN((size_t)maxJumpReplaysPerCategory, replays.size()); i++)
		{
			keepReplays.insert(std::get<0>(replays[i]));
		}

		// Sort by distance (descending) and keep top N
		std::sort(replays.begin(), replays.end(), [](const auto &a, const auto &b) { return std::get<2>(a).distance > std::get<2>(b).distance; });
		for (size_t i = 0; i < MIN((size_t)maxJumpReplaysPerCategory, replays.size()); i++)
		{
			keepReplays.insert(std::get<0>(replays[i]));
		}

		// Archive replays not in the keep set
		for (auto &[uuid, generalHeader, specificHeader] : replays)
		{
			if (keepReplays.find(uuid) == keepReplays.end() && generalHeader.archivedTimestamp == 0)
			{
				UpdateArchivedReplayOnDisk(uuid, generalHeader, currentTime);

				// Update in the map
				jumpReplays[uuid].first.archivedTimestamp = currentTime;
			}
		}
	}
}

void ReplayWatcher::CleanupManualReplays(std::unordered_map<UUID_t, std::pair<GeneralReplayHeader, ManualReplayHeader>> &manualReplays,
										 std::unordered_map<u64, std::vector<std::pair<UUID_t, u64>>> &manualReplaysBySteamID)
{
	// Clean up excess manual replays (keep only N most recent per steam ID)
	i32 maxManualReplays = MAX(KZOptionService::GetOptionInt("maxManualReplays", 2), 2);
	for (auto &[steamID, replays] : manualReplaysBySteamID)
	{
		if (replays.size() > maxManualReplays)
		{
			// Sort by timestamp (newest first)
			std::sort(replays.begin(), replays.end(), [](const auto &a, const auto &b) { return a.second > b.second; });

			// Delete files beyond the N most recent
			for (size_t i = maxManualReplays; i < replays.size(); i++)
			{
				char fullPath[MAX_PATH];
				V_snprintf(fullPath, sizeof(fullPath), "%s/%s.replay", KZ_REPLAY_PATH, replays[i].first.ToString().c_str());
				g_pFullFileSystem->RemoveFile(fullPath, "GAME");

				// Remove from the map we're about to store
				manualReplays.erase(replays[i].first);
			}
		}
	}
}

void ReplayWatcher::WatchLoop()
{
	// Initial scan
	ScanReplays();

	while (running)
	{
		std::this_thread::sleep_for(std::chrono::seconds(5));
		if (!running)
		{
			break;
		}
		ScanReplays();
	}
}

void ReplayWatcher::ScanReplays()
{
	char searchPath[MAX_PATH];
	V_snprintf(searchPath, sizeof(searchPath), "%s/*.replay", KZ_REPLAY_PATH);

	FileFindHandle_t findHandle;
	const char *pFileName = g_pFullFileSystem->FindFirstEx(searchPath, "GAME", &findHandle);

	std::unordered_map<UUID_t, std::pair<GeneralReplayHeader, CheaterReplayHeader>> newCheaterReplays;
	std::map<UUID_t, std::pair<GeneralReplayHeader, RunReplayHeader>> newRunReplays;
	std::map<UUID_t, std::pair<GeneralReplayHeader, JumpReplayHeader>> newJumpReplays;
	std::unordered_map<UUID_t, std::pair<GeneralReplayHeader, ManualReplayHeader>> newManualReplays;

	// Temporary storage for sorting run/jump replays
	std::vector<std::tuple<UUID_t, GeneralReplayHeader, RunReplayHeader>> tempRunReplays;
	std::vector<std::tuple<UUID_t, GeneralReplayHeader, JumpReplayHeader>> tempJumpReplays;

	std::unordered_map<u64, std::vector<std::pair<UUID_t, u64>>> manualReplaysBySteamID;

	// Get current unix time for archival checks
	time_t currentUnixTime = 0;
	time(&currentUnixTime);
	u32 archiveRetentionMinutes = MAX(KZOptionService::GetOptionInt("archiveRetentionMinutes", 2880), 1440);
	u64 archiveRetentionSeconds = archiveRetentionMinutes * 60;

	while (pFileName)
	{
		if (!g_pFullFileSystem->FindIsDirectory(findHandle))
		{
			// Extract UUID from filename (remove .replay extension)
			char uuidStr[64];
			V_strncpy(uuidStr, pFileName, sizeof(uuidStr));
			char *ext = V_strstr(uuidStr, ".replay");
			if (ext)
			{
				*ext = '\0';
			}

			UUID_t uuid;
			if (UUID_t::FromString(uuidStr, &uuid))
			{
				// Open and read headers
				char fullPath[MAX_PATH];
				V_snprintf(fullPath, sizeof(fullPath), "%s/%s", KZ_REPLAY_PATH, pFileName);

				FileHandle_t file = g_pFullFileSystem->Open(fullPath, "rb", "GAME");
				if (file)
				{
					GeneralReplayHeader generalHeader;
					if (g_pFullFileSystem->Read(&generalHeader, sizeof(generalHeader), file) == sizeof(generalHeader))
					{
						// Check if archived replay is older than 14 days and should be deleted
						if (generalHeader.archivedTimestamp != 0)
						{
							u64 archiveAge = currentUnixTime - generalHeader.archivedTimestamp;
							if (archiveAge >= archiveRetentionSeconds)
							{
								g_pFullFileSystem->Close(file);
								g_pFullFileSystem->RemoveFile(fullPath, "GAME");
								pFileName = g_pFullFileSystem->FindNext(findHandle);
								continue;
							}
						}

						switch (generalHeader.type)
						{
							case RP_CHEATER:
							{
								CheaterReplayHeader specificHeader;
								if (g_pFullFileSystem->Read(&specificHeader, sizeof(specificHeader), file) == sizeof(specificHeader))
								{
									newCheaterReplays[uuid] = {generalHeader, specificHeader};
								}
								break;
							}
							case RP_RUN:
							{
								RunReplayHeader specificHeader;
								if (g_pFullFileSystem->Read(&specificHeader, sizeof(specificHeader), file) == sizeof(specificHeader))
								{
									tempRunReplays.push_back({uuid, generalHeader, specificHeader});
								}
								break;
							}
							case RP_JUMPSTATS:
							{
								JumpReplayHeader specificHeader;
								if (g_pFullFileSystem->Read(&specificHeader, sizeof(specificHeader), file) == sizeof(specificHeader))
								{
									tempJumpReplays.push_back({uuid, generalHeader, specificHeader});
								}
								break;
							}
							case RP_MANUAL:
							{
								ManualReplayHeader specificHeader;
								if (g_pFullFileSystem->Read(&specificHeader, sizeof(specificHeader), file) == sizeof(specificHeader))
								{
									newManualReplays[uuid] = {generalHeader, specificHeader};

									// Track for potential cleanup
									manualReplaysBySteamID[generalHeader.player.steamid64].push_back({uuid, generalHeader.timestamp});
								}
								break;
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

	// Process each replay type with dedicated functions
	ProcessCheaterReplays(newCheaterReplays, currentUnixTime);
	ProcessRunReplays(tempRunReplays, newRunReplays, currentUnixTime);
	ProcessJumpReplays(tempJumpReplays, newJumpReplays, currentUnixTime);
	CleanupManualReplays(newManualReplays, manualReplaysBySteamID);

	// Atomically swap the maps
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		cheaterReplays = std::move(newCheaterReplays);
		runReplays = std::move(newRunReplays);
		jumpReplays = std::move(newJumpReplays);
		manualReplays = std::move(newManualReplays);
	}
}

ReplayWatcher g_ReplayWatcher;

void KZ::replaysystem::InitWatcher()
{
	g_ReplayWatcher.Start();
}

void KZ::replaysystem::CleanupWatcher()
{
	g_ReplayWatcher.Stop();
}
