#include "kz_jumpstats.h"
#include "kz/db/kz_db.h"
#include "kz/jumpstats/hud/jump_top.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/replays/kz_replaysystem.h"
#include "utils/simplecmds.h"
#include "utils/tables.h"
#include "utils/utils.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "tier0/memdbgon.h"

static_global const char *const jumpTopHeaders[] = {"#.",
													"Replay Table Header - Player",
													"Replay Table Header - Distance",
													"Replay Table Header - Block",
													"Replay Table Header - Strafes",
													"Jump Top - Sync",
													"Jump Top - Pre",
													"Jump Top - Max",
													"Jump Top - Airtime"};

// Prints the first page of the list the window would show to the player's console.
static_function void PrintJumpTop(KZPlayer *player, i32 typeIndex, i32 modeIndex, bool block)
{
	const i32 modeID = KZ::mode::GetModeInfo(KZ::api::GetModeShortName(KZ::api::modes[modeIndex])).databaseID;
	if (!KZDatabaseService::IsReady() || modeID < 0)
	{
		player->languageService->PrintChat(true, false, "Jump Top - Unavailable");
		return;
	}
	const CPlayerUserId userID = player->GetClient()->GetUserID();
	auto onSuccess = [userID, typeIndex, modeIndex, block](std::vector<ISQLQuery *> queries)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}
		std::string headers[KZ_ARRAYSIZE(jumpTopHeaders)];
		for (u32 i = 0; i < KZ_ARRAYSIZE(jumpTopHeaders); i++)
		{
			headers[i] = player->languageService->PrepareMessage(jumpTopHeaders[i]);
		}
		const std::string list = player->languageService->PrepareMessage(block ? "Replay Table Header - Block" : "Replay Table Header - Distance");
		const std::string title = player->languageService->PrepareMessage("Jump Top - Table Title", jumpTypeShortStr[JumpType_LongJump + typeIndex],
																		  KZ::api::GetModeShortName(KZ::api::modes[modeIndex]), list.c_str());
		utils::Table<KZ_ARRAYSIZE(jumpTopHeaders)> table(title, headers);
		ISQLResult *result = queries[0]->GetResultSet();
		for (u32 row = 0; result && result->FetchRow(); row++)
		{
			const u32 blockUnits = result->GetInt64(4) ? (u32)result->GetInt64(5) : 0;
			table.SetRow(row, tfm::format("%u", row + 1), utils::OrEmpty(result->GetString(2)),
						 tfm::format("%.4f", result->GetInt64(3) / KZ_JUMPSTATS_DISTANCE_SCALE), blockUnits ? std::to_string(blockUnits) : "-",
						 std::to_string(result->GetInt64(6)), tfm::format("%.1f%%", result->GetInt64(7) / KZ_JUMPSTATS_SYNC_SCALE),
						 tfm::format("%.2f", result->GetInt64(8) / KZ_JUMPSTATS_SPEED_SCALE),
						 tfm::format("%.2f", result->GetInt64(9) / KZ_JUMPSTATS_SPEED_SCALE),
						 tfm::format("%.3f", result->GetInt64(10) / KZ_JUMPSTATS_AIRTIME_SCALE));
		}
		player->languageService->PrintChat(true, false, "Jump Top - Check Console");
		player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
		player->PrintConsole(false, false, "%s", table.GetTitle().c_str());
		player->PrintConsole(false, false, "%s", table.GetHeader().c_str());
		for (u32 i = 0; i < table.GetNumEntries(); i++)
		{
			player->PrintConsole(false, false, "%s", table.GetLine(i).c_str());
		}
		player->PrintConsole(false, false, "%s", table.GetSeparator("=").c_str());
	};
	KZDatabaseService::QueryJumpTop(JumpType_LongJump + typeIndex, modeID, block, KZ_JUMP_TOP_ROWS, 0, onSuccess,
									KZDatabaseService::OnGenericTxnFailure);
}

// Arguments in any order: a jump type (lj, bh, ...), a mode (ckz, vnl) and "block" for the block list.
SCMD(kz_jstop, SCFL_JUMPSTATS)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	i32 typeIndex = 0;
	KZ::api::Mode mode = KZ::api::Mode::Classic;
	KZ::api::DecodeModeString(KZ::mode::GetModeInfo(player->modeService).shortModeName, mode);
	bool block = false;
	for (i32 i = 1; i < args->ArgC(); i++)
	{
		const char *arg = args->Arg(i);
		for (i32 t = 0; t < KZ_JUMP_TOP_TYPES; t++)
		{
			if (KZ_STREQI(arg, jumpTypeShortStr[JumpType_LongJump + t]))
			{
				typeIndex = t;
			}
		}
		if (KZ_STREQI(arg, "block") || KZ_STREQI(arg, "b"))
		{
			block = true;
		}
		KZ::api::DecodeModeString(arg, mode);
	}
	const i32 modeIndex = KZ::api::GetModeIndex(mode);
	// Prefer using the custom HUD if it's a chat command unless the addon is unavailable.
	const bool chat = args->Arg(0)[0] == SCMD_CHAT_TRIGGER || args->Arg(0)[0] == SCMD_CHAT_SILENT_TRIGGER;
	if (chat && player->uiService->GetWindow<KZ::jumpstats::hud::JumpTopWindow>()->Show(typeIndex, modeIndex, block))
	{
		return true;
	}
	PrintJumpTop(player, typeIndex, modeIndex, block);
	return true;
}

SCMD_LINK(kz_jumptop, kz_jstop);

// Deletes the replay files of the jumps whose UUIDs the first query returned.
static_function u32 DeleteJumpReplays(ISQLQuery *query)
{
	u32 deleted = 0;
	ISQLResult *result = query ? query->GetResultSet() : nullptr;
	while (result && result->FetchRow())
	{
		std::string path;
		if (!result->IsNull(0) && KZ::replaysystem::FindReplayPath(utils::OrEmpty(result->GetString(0)), path))
		{
			utils::RemoveFile(path.c_str());
			deleted++;
		}
	}
	return deleted;
}

static_function bool CheckJumpDatabase()
{
	if (KZDatabaseService::IsReady())
	{
		return true;
	}
	KZ_LOG_INFO(LogChannel::DB, "The database is not ready.\n");
	return false;
}

CON_COMMAND_F(kz_listjumps, "List the saved jumps of a player by their SteamID64.", FCVAR_NONE)
{
	if (args.ArgC() != 2)
	{
		KZ_LOG_INFO(LogChannel::DB, "Usage: kz_listjumps <SteamID64>\n");
		return;
	}
	if (!CheckJumpDatabase())
	{
		return;
	}
	const u64 steamID64 = strtoull(args.Arg(1), nullptr, 10);
	auto onSuccess = [steamID64](std::vector<ISQLQuery *> queries)
	{
		ISQLResult *result = queries[0]->GetResultSet();
		u32 count = 0;
		KZ_LOG_INFO(LogChannel::DB, "Saved jumps of %llu (ID, type, mode, distance, block, date, replay):\n", steamID64);
		while (result && result->FetchRow())
		{
			const i64 type = result->GetInt64(1);
			const char *typeName = type >= 0 && type < JUMPTYPE_COUNT ? jumpTypeShortStr[type] : "?";
			const std::string mode = KZ::mode::GetModeInfoFromDatabaseID((i32)result->GetInt64(2)).shortModeName;
			const u32 block = result->GetInt64(4) ? (u32)result->GetInt64(5) : 0;
			std::string path;
			const bool replay = !result->IsNull(7) && KZ::replaysystem::FindReplayPath(utils::OrEmpty(result->GetString(7)), path);
			KZ_LOG_INFO(LogChannel::DB, "  %lld  %s  %s  %.4f  %s  %s  %s\n", (long long)result->GetInt64(0), typeName,
						mode.empty() ? "?" : mode.c_str(), result->GetInt64(3) / KZ_JUMPSTATS_DISTANCE_SCALE,
						block ? std::to_string(block).c_str() : "-", utils::OrEmpty(result->GetString(6)), replay ? "yes" : "no");
			count++;
		}
		KZ_LOG_INFO(LogChannel::DB, "%u jumps. Delete one with kz_deletejump <ID>.\n", count);
	};
	KZDatabaseService::FindPlayerJumps(steamID64, onSuccess, KZDatabaseService::OnGenericTxnFailure);
}

CON_COMMAND_F(kz_deletejump, "Delete a saved jump and its replay by the jump's ID (see kz_listjumps).", FCVAR_NONE)
{
	if (args.ArgC() != 2)
	{
		KZ_LOG_INFO(LogChannel::DB, "Usage: kz_deletejump <ID>\n");
		return;
	}
	if (!CheckJumpDatabase())
	{
		return;
	}
	const u32 id = (u32)strtoul(args.Arg(1), nullptr, 10);
	auto onSuccess = [id](std::vector<ISQLQuery *> queries)
	{
		ISQLResult *result = queries[0]->GetResultSet();
		if (!result || result->GetRowCount() == 0)
		{
			KZ_LOG_INFO(LogChannel::DB, "There is no saved jump with ID %u.\n", id);
			return;
		}
		const u32 replays = DeleteJumpReplays(queries[0]);
		KZ_LOG_INFO(LogChannel::DB, "Deleted jump %u and %u replay file(s).\n", id, replays);
	};
	KZDatabaseService::DeleteJumpstat(id, onSuccess, KZDatabaseService::OnGenericTxnFailure);
}

CON_COMMAND_F(kz_deletealljumps, "Delete every saved jump and jump replay of a player by their SteamID64.", FCVAR_NONE)
{
	if (args.ArgC() != 2)
	{
		KZ_LOG_INFO(LogChannel::DB, "Usage: kz_deletealljumps <SteamID64>\n");
		return;
	}
	if (!CheckJumpDatabase())
	{
		return;
	}
	const u64 steamID64 = strtoull(args.Arg(1), nullptr, 10);
	auto onSuccess = [steamID64](std::vector<ISQLQuery *> queries)
	{
		ISQLResult *result = queries[0]->GetResultSet();
		const u32 jumps = result ? (u32)result->GetRowCount() : 0;
		const u32 replays = DeleteJumpReplays(queries[0]);
		KZ_LOG_INFO(LogChannel::DB, "Deleted %u jump(s) and %u replay file(s) of %llu.\n", jumps, replays, steamID64);
	};
	KZDatabaseService::DeleteAllJumpstats(steamID64, onSuccess, KZDatabaseService::OnGenericTxnFailure);
}
