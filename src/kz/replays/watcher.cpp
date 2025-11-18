#include "watcher.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/mode/kz_mode.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/timer/kz_timer.h"
#include "filesystem.h"
#include "utils/tables.h"

static const char *ARCHIVE_INDEX_PATH = KZ_REPLAY_PATH "/archive_index.txt";

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
	std::vector<std::pair<UUID_t, ReplayHeader>> matchingReplays;
	u64 numMatched = 0;
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		for (auto &replay : this->cheaterReplays)
		{
			if (criteria.PassFilters(replay.second))
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
		const ReplayHeader &hdr = pair.second;

		std::string rowNumber = std::to_string(i + 1);
		std::string steamID = hdr.has_player() ? std::to_string(hdr.player().steamid64()) : "0";
		const char *reason = hdr.has_cheater() ? hdr.cheater().reason().c_str() : "";
		// clang-format off
		table.SetRow(i, rowNumber.c_str(),
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
	std::vector<std::pair<UUID_t, ReplayHeader>> matchingReplays;
	u64 numMatched = 0;
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		for (auto &replay : this->runReplays)
		{
			if (criteria.PassFilters(replay.second))
			{
				if (numMatched < criteria.offset)
				{
					numMatched++;
					continue;
				}

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
		const ReplayHeader &hdr = pair.second;
		if (!hdr.has_run())
		{
			continue;
		}
		auto &run = hdr.run();
		std::string modeName = run.mode().name();
		if (run.styles_size() > 0)
		{
			modeName += " +" + std::to_string(run.styles_size());
		}
		std::string rowNumber = std::to_string(i + 1);
		std::string teleports = std::to_string(run.num_teleports());
		std::string steamID = std::to_string(hdr.player().steamid64());
		// clang-format off
		table.SetRow(i, rowNumber.c_str(),
                        hdr.player().name().c_str(), 
                        hdr.map().name().c_str(),
                        run.course_name().c_str(), 
                        modeName.c_str(),
                        utils::FormatTime(run.time()).Get(), teleports.c_str(), steamID.c_str(), pair.first.ToString().c_str());
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
	std::vector<std::pair<UUID_t, ReplayHeader>> matchingReplays;
	u64 numMatched = 0;
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		for (auto &replay : this->jumpReplays)
		{
			if (criteria.PassFilters(replay.second))
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
		std::string rowNumber = std::to_string(i + 1);
		std::string steamID = std::to_string(hdr.player().steamid64());
		// clang-format off
		table.SetRow(i, rowNumber.c_str(), 
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
	std::vector<std::pair<UUID_t, ReplayHeader>> matchingReplays;
	u64 numMatched = 0;
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		for (auto &replay : this->manualReplays)
		{
			if (criteria.PassFilters(replay.second))
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
		const ReplayHeader &hdr = pair.second;
		if (!hdr.has_manual())
		{
			continue;
		}
		std::string rowNumber = std::to_string(i + 1);
		std::string steamID = std::to_string(hdr.player().steamid64());
		const char *savedBy = (hdr.manual().has_saved_by() ? hdr.manual().saved_by().name().c_str() : "");
		// clang-format off
		table.SetRow(i, rowNumber.c_str(), 
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
	criteria.mapName = kv ? kv->GetString() : g_pKZUtils->GetCurrentMapName().Get();

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

			this->FilterAndPrintMatchingRunReplays(criteria, player);
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

bool ReplayFilterCriteria::PassGeneralFilters(const ReplayHeader &header) const
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
	if (mapName != "*" && (!header.has_map() || !V_stristr(header.map().name().c_str(), mapName.c_str())))
	{
		return false;
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

bool ReplayFilterCriteria::PassFilters(const ReplayHeader &header) const
{
	if (!PassGeneralFilters(header))
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

void ReplayWatcher::LoadArchiveIndex()
{
	this->archivedIndex.clear();
	KeyValues3 kv;
	CUtlString error;
	if (!LoadKV3FromFile(&kv, &error, ARCHIVE_INDEX_PATH, "GAME", g_KV3Format_Generic, 0))
	{
		// File doesn't exist or failed to load - that's okay for first run
		return;
	}

	// Iterate through all members in the table
	for (int i = 0; i < kv.GetMemberCount(); i++)
	{
		const char *uuidStr = kv.GetMemberName(i);
		KeyValues3 *member = kv.GetMember(i);
		if (!member || !uuidStr)
		{
			continue;
		}

		u64 timestamp = member->GetUInt64(0);
		UUID_t uuid;
		if (UUID_t::FromString(uuidStr, &uuid))
		{
			this->archivedIndex[uuid] = timestamp;
		}
	}
}

void ReplayWatcher::SaveArchiveIndex()
{
	KeyValues3 kv;

	// Build KV3 table with UUID -> timestamp mappings
	for (auto &[uuid, timestamp] : this->archivedIndex)
	{
		kv.SetMemberUInt64(uuid.ToString().c_str(), timestamp);
	}

	CUtlString error;
	if (!SaveKV3ToFile(g_KV3Encoding_Text, g_KV3Format_Generic, &kv, &error, ARCHIVE_INDEX_PATH, "GAME", KV3_SAVE_TEXT_NONE))
	{
		// Log error if needed, but continue
		META_CONPRINTF("Failed to save archive index: %s\n", error.Get());
	}

	this->archiveDirty = false;
}

void ReplayWatcher::MarkArchived(const UUID_t &uuid, u64 archiveTimestamp)
{
	this->archivedIndex[uuid] = archiveTimestamp;
	this->archiveDirty = true;
}

void ReplayWatcher::ProcessCheaterReplays(std::unordered_map<UUID_t, ReplayHeader> &map, u64 currentTime)
{
	for (auto &[uuid, hdr] : map)
	{
		if (hdr.has_cheater() && hdr.cheater().has_reporter() && hdr.cheater().reporter().steamid64() != 0
			&& this->archivedIndex.find(uuid) == this->archivedIndex.end())
		{
			MarkArchived(uuid, currentTime);
		}
	}
}

void ReplayWatcher::ProcessRunReplays(std::vector<std::tuple<UUID_t, ReplayHeader>> &tempRunReplays, std::map<UUID_t, ReplayHeader> &runMap,
									  u64 currentTime)
{
	// Group by steamid, course, mode, and map
	struct RunReplayKey
	{
		u64 steamid64;
		std::string mapName;
		std::string modeName;
		std::string courseName;

		bool operator==(const RunReplayKey &o) const
		{
			// clang-format off
			return steamid64 == o.steamid64 
                && !V_stricmp(mapName.c_str(), o.mapName.c_str())
                && !V_stricmp(modeName.c_str(), o.modeName.c_str())
				&& !V_stricmp(courseName.c_str(), o.courseName.c_str());
			// clang-format on
		}
	};

	struct RunReplayKeyHasher
	{
		size_t operator()(const RunReplayKey &k) const
		{
			std::size_t h1 = std::hash<u64> {}(k.steamid64);
			std::size_t h2 = std::hash<std::string> {}(k.mapName);
			std::size_t h3 = std::hash<std::string> {}(k.modeName);
			std::size_t h4 = std::hash<std::string> {}(k.courseName);
			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
		}
	};

	std::unordered_map<RunReplayKey, std::vector<std::tuple<UUID_t, ReplayHeader>>, RunReplayKeyHasher> groups;
	for (auto &replay : tempRunReplays)
	{
		auto &[uuid, hdr] = replay;
		if (!hdr.has_run())
		{
			continue;
		}
		auto &run = hdr.run();
		// Styled runs (styleCount > 0) are marked as archived immediately
		if (run.styles_size() > 0 && this->archivedIndex.find(uuid) == this->archivedIndex.end())
		{
			MarkArchived(uuid, currentTime);
		}
		if (run.styles_size() == 0)
		{
			RunReplayKey key {hdr.player().steamid64(), hdr.map().name(), run.mode().name(), run.course_name()};
			groups[key].push_back(replay);
		}
		runMap[uuid] = hdr;
	}
	int maxPer = MAX(KZOptionService::GetOptionInt("maxRunReplaysPerGroup", 3), 2);
	for (auto &[uuid, header] : groups)
	{
		std::set<UUID_t> keep;
		// Sort by time (fastest first) and keep top N
		std::sort(header.begin(), header.end(), [](auto &a, auto &b) { return std::get<1>(a).run().time() < std::get<1>(b).run().time(); });
		for (size_t i = 0; i < MIN((size_t)maxPer, header.size()); i++)
		{
			keep.insert(std::get<0>(header[i]));
		}

		// Filter to only runs with 0 teleports, sort by time (fastest first), and keep top N
		std::vector<std::tuple<UUID_t, ReplayHeader>> proReplays;
		for (auto &r : header)
		{
			if (std::get<1>(r).run().num_teleports() == 0)
			{
				proReplays.push_back(r);
			}
		}
		std::sort(proReplays.begin(), proReplays.end(), [](auto &a, auto &b) { return std::get<1>(a).run().time() < std::get<1>(b).run().time(); });
		for (size_t i = 0; i < MIN((size_t)maxPer, proReplays.size()); i++)
		{
			keep.insert(std::get<0>(proReplays[i]));
		}
		for (auto &[uuid, hdr] : header)
		{
			if (!keep.count(uuid) && archivedIndex.find(uuid) == archivedIndex.end())
			{
				MarkArchived(uuid, currentTime);
			}
		}
	}
}

void ReplayWatcher::ProcessJumpReplays(std::vector<std::tuple<UUID_t, ReplayHeader>> &temp, std::map<UUID_t, ReplayHeader> &jumpMap, u64 currentTime)
{
	struct Key
	{
		u64 steamid64;
		u8 jumpType;
		std::string modeName;

		bool operator==(const Key &o) const
		{
			return steamid64 == o.steamid64 && jumpType == o.jumpType && !V_stristr(modeName.c_str(), o.modeName.c_str());
		}
	};

	struct KeyHasher
	{
		size_t operator()(const Key &k) const
		{
			return std::hash<u64> {}(k.steamid64) ^ (std::hash<u8> {}(k.jumpType) << 1) ^ (std::hash<std::string> {}(k.modeName) << 2);
		}
	};

	std::unordered_map<Key, std::vector<std::tuple<UUID_t, ReplayHeader>>, KeyHasher> groups;
	for (auto &t : temp)
	{
		auto &[uuid, hdr] = t;
		if (!hdr.has_jump())
		{
			continue;
		}
		auto &jump = hdr.jump();
		Key key {hdr.player().steamid64(), (u8)jump.jump_type(), jump.mode().name()};
		groups[key].push_back(t);
		jumpMap[uuid] = hdr;
	}
	int maxPer = MAX(KZOptionService::GetOptionInt("maxJumpReplaysPerCategory", 3), 2);
	for (auto &[key, vec] : groups)
	{
		std::set<UUID_t> keep;
		std::sort(vec.begin(), vec.end(),
				  [](auto &a, auto &b) { return std::get<1>(a).jump().block_distance() > std::get<1>(b).jump().block_distance(); });
		for (size_t i = 0; i < MIN((size_t)maxPer, vec.size()); i++)
		{
			keep.insert(std::get<0>(vec[i]));
		}
		std::sort(vec.begin(), vec.end(), [](auto &a, auto &b) { return std::get<1>(a).jump().distance() > std::get<1>(b).jump().distance(); });
		for (size_t i = 0; i < MIN((size_t)maxPer, vec.size()); i++)
		{
			keep.insert(std::get<0>(vec[i]));
		}
		for (auto &[uuid, hdr] : vec)
		{
			if (!keep.count(uuid) && archivedIndex.find(uuid) == archivedIndex.end())
			{
				MarkArchived(uuid, currentTime);
			}
		}
	}
}

void ReplayWatcher::CleanupManualReplays(std::unordered_map<UUID_t, ReplayHeader> &map,
										 std::unordered_map<u64, std::vector<std::pair<UUID_t, u64>>> &bySteam)
{
	int maxManual = MAX(KZOptionService::GetOptionInt("maxManualReplays", 2), 2);
	for (auto &[steamID, vec] : bySteam)
	{
		if (vec.size() > maxManual)
		{
			std::sort(vec.begin(), vec.end(), [](auto &a, auto &b) { return a.second > b.second; });
			for (size_t i = maxManual; i < vec.size(); i++)
			{
				char fullPath[MAX_PATH];
				V_snprintf(fullPath, sizeof(fullPath), "%s/%s.replay", KZ_REPLAY_PATH, vec[i].first.ToString().c_str());
				g_pFullFileSystem->RemoveFile(fullPath, "GAME");
				map.erase(vec[i].first);
			}
		}
	}
}

void ReplayWatcher::WatchLoop()
{
	LoadArchiveIndex();
	ScanReplays();
	while (this->running)
	{
		std::this_thread::sleep_for(std::chrono::seconds(5));
		if (!this->running)
		{
			break;
		}
		ScanReplays();
		if (this->archiveDirty)
		{
			SaveArchiveIndex();
		}
	}
}

void ReplayWatcher::ScanReplays()
{
	char searchPath[MAX_PATH];
	V_snprintf(searchPath, sizeof(searchPath), "%s/*.replay", KZ_REPLAY_PATH);

	FileFindHandle_t findHandle = {};
	const char *pFileName = g_pFullFileSystem->FindFirstEx(searchPath, "GAME", &findHandle);

	std::unordered_map<UUID_t, ReplayHeader> newCheater;
	std::map<UUID_t, ReplayHeader> newRun;
	std::map<UUID_t, ReplayHeader> newJump;
	std::unordered_map<UUID_t, ReplayHeader> newManual;
	std::vector<std::tuple<UUID_t, ReplayHeader>> tempRun;
	std::vector<std::tuple<UUID_t, ReplayHeader>> tempJump;
	std::unordered_map<u64, std::vector<std::pair<UUID_t, u64>>> manualBySteam;

	time_t currentUnixTime = 0;
	time(&currentUnixTime);
	u32 retentionMinutes = MAX(KZOptionService::GetOptionInt("archiveRetentionMinutes", 2880), 1440);
	u64 retentionSeconds = retentionMinutes * 60ULL;

	while (pFileName)
	{
		if (!g_pFullFileSystem->FindIsDirectory(findHandle))
		{
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
				char fullPath[MAX_PATH];
				V_snprintf(fullPath, sizeof(fullPath), "%s/%s", KZ_REPLAY_PATH, pFileName);
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
								auto idxIt = this->archivedIndex.find(uuid);
								if (idxIt != this->archivedIndex.end())
								{
									u64 age = currentUnixTime - idxIt->second;
									if (age >= retentionSeconds)
									{
										g_pFullFileSystem->Close(file);
										g_pFullFileSystem->RemoveFile(fullPath, "GAME");
										this->archivedIndex.erase(idxIt);
										this->archiveDirty = true;
										pFileName = g_pFullFileSystem->FindNext(findHandle);
										continue;
									}
								}
								switch (static_cast<ReplayType>(hdr.type()))
								{
									case RP_CHEATER:
										newCheater[uuid] = hdr;
										break;
									case RP_RUN:
										tempRun.push_back({uuid, hdr});
										break;
									case RP_JUMPSTATS:
										tempJump.push_back({uuid, hdr});
										break;
									case RP_MANUAL:
										newManual[uuid] = hdr;
										if (hdr.has_player())
										{
											manualBySteam[hdr.player().steamid64()].push_back({uuid, hdr.timestamp()});
										}
										break;
									default:
										break;
								}
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
	ProcessCheaterReplays(newCheater, currentUnixTime);
	ProcessRunReplays(tempRun, newRun, currentUnixTime);
	ProcessJumpReplays(tempJump, newJump, currentUnixTime);
	CleanupManualReplays(newManual, manualBySteam);
	{
		std::lock_guard<std::mutex> lock(this->replayMapsMutex);
		this->cheaterReplays = std::move(newCheater);
		this->runReplays = std::move(newRun);
		this->jumpReplays = std::move(newJump);
		this->manualReplays = std::move(newManual);
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
