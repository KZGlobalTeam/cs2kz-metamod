
#define NOMINMAX
#include "cs2kz.h"
#include "kz/kz.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/option/kz_option.h"
#include "kz/db/kz_db.h"
#include "kz/timer/kz_timer.h"
#include "commands.h"
#include "data.h"
#include "bot.h"
#include "events.h"
#include "playback.h"
#include "watcher.h"
#include "utils/uuid.h"
#include "utils/simplecmds.h"
#include "kz/global/kz_global.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include <functional>
extern ReplayWatcher g_ReplayWatcher;

namespace KZ::replaysystem::commands
{

	void NavigateReplay(KZPlayer *player, u32 targetTick)
	{
		auto replay = data::GetCurrentReplay();

		// Reset replay state and reprocess events up to target tick
		data::ResetReplayState(replay);
		events::ReprocessEventsUpToTick(replay, targetTick);

		// Set current tick
		replay->currentTick = targetTick;

		// Apply the target tick's state immediately
		auto bot = bot::GetBot();
		if (bot)
		{
			KZPlayer *botPlayer = g_pKZPlayerManager->ToPlayer(bot);
			if (botPlayer)
			{
				TickData *tickData = &replay->tickData[targetTick];
				playback::ApplyTickState(botPlayer, tickData);
			}
		}
	}

	void LoadReplay(KZPlayer *player, const char *uuid)
	{
		if (!player)
		{
			return;
		}

		// Check if already loading
		if (data::IsLoading())
		{
			player->languageService->PrintChat(true, false, "Replay - Loading Already");
			return;
		}

		UUID_t parsedUuid;
		if (!UUID_t::FromString(uuid, &parsedUuid))
		{
			// Try to find replays matching the UUID substring
			auto matches = g_ReplayWatcher.FindReplaysByUUIDSubstring(uuid);
			if (matches.empty())
			{
				player->languageService->PrintChat(true, false, "Replay - Invalid UUID");
				return;
			}
			else if (matches.size() > 1)
			{
				player->languageService->PrintChat(true, false, "Replay - Multiple Matches", uuid);
				return;
			}
			// Exactly one match found
			parsedUuid = matches[0];
		}

		// Validate uuid format
		char replayPath[512];
		V_snprintf(replayPath, sizeof(replayPath), KZ_REPLAY_PATH "/%s.replay", parsedUuid.ToString().c_str());

		if (!g_pFullFileSystem->FileExists(replayPath))
		{
			// Also check the downloads subdirectory for cached API replays.
			V_snprintf(replayPath, sizeof(replayPath), KZ_REPLAY_DOWNLOADS_PATH "/%s.replay", parsedUuid.ToString().c_str());
		}

		if (!g_pFullFileSystem->FileExists(replayPath))
		{
			if (KZGlobalService::IsAvailable())
			{
				KZGlobalService::RequestReplay(player, parsedUuid);
			}
			else
			{
				player->languageService->PrintChat(true, false, "Replay - Not Found");
			}
			return;
		}

		// Show loading message
		player->languageService->PrintChat(true, false, "Replay - Loading");

		// Get player user ID for thread-safe callback access
		CPlayerUserId playerUserID = player->GetClient()->GetUserID();

		// Start async loading
		// clang-format off
		data::LoadReplayAsync(
			replayPath,
			// Success callback (runs on main thread via ProcessAsyncLoadCompletion)
			data::LoadSuccessCallback([playerUserID]() {
				KZPlayer* player = g_pKZPlayerManager->ToPlayer(playerUserID);
				if (!player)
				{
					return;
				}
				if (!KZ_STREQI(data::GetCurrentReplay()->header.map().name().c_str(), g_pKZUtils->GetCurrentMapName().Get()))
				{
					player->languageService->PrintChat(true, false, "Replay - Wrong Map", data::GetCurrentReplay()->header.map().name().c_str(), g_pKZUtils->GetCurrentMapName().Get());
					return;
				}
				for (u32 i = 0; i < data::GetCurrentReplay()->numEvents; i++)
				{
					auto& event = data::GetCurrentReplay()->events[i];
					switch (event.type)
					{
						case RPEVENT_MODE_CHANGE:
						{
							if (KZ::mode::GetModeInfo(CUtlString(event.data.modeChange.name)).id < 0)
							{
								player->languageService->PrintChat(true, false, "Replay - Unknown Mode", event.data.modeChange.name);
							}
							break;
						}
						case RPEVENT_STYLE_CHANGE:
						{
							if (KZ::style::GetStyleInfo(CUtlString(event.data.styleChange.name)).id < 0)
							{
								player->languageService->PrintChat(true, false, "Replay - Unknown Style", event.data.styleChange.name);
							}
							break;
						}
					}
				}
				player->languageService->PrintChat(true, false, "Replay - Loaded Successfully");

				// Initialize bot and start playback (safe to call on main thread)
				// The replay data is already stored in the global g_currentReplay
				auto replay = data::GetCurrentReplay();
				bot::InitializeBotForReplay(replay->header);
				playback::StartReplay();
				playback::InitializeWeapons();
				bot::SpectateBot(player);
			}),
			// Failure callback (runs on main thread via ProcessAsyncLoadCompletion)
			data::LoadFailureCallback([playerUserID](const char* error) {
				KZPlayer* player = g_pKZPlayerManager->ToPlayer(playerUserID);
				if (player)
				{
					player->languageService->PrintChat(true, false, error);
				}
			})
		);
		// clang-format on
	}

	void JumpToReplayTime(KZPlayer *player, const char *input)
	{
		if (!player)
		{
			return;
		}

		if (!data::IsReplayPlaying())
		{
			player->languageService->PrintChat(true, false, "Replay - No Replay Playing");
			return;
		}

		auto replay = data::GetCurrentReplay();
		u32 targetTick;
		bool isRelative = (input[0] == '+' || input[0] == '-');

		if (isRelative)
		{
			// Relative time seeking
			f64 seekSeconds = -1.0;

			if (!utils::ParseTimeString(input + 1, &seekSeconds)) // Skip the +/- sign
			{
				player->languageService->PrintChat(true, false, "Replay - Invalid Relative Time");
				return;
			}

			if (input[0] == '-')
			{
				seekSeconds = -seekSeconds;
			}

			// Calculate target tick based on current replay position + seek amount
			i32 ticksToSeek = (i32)(seekSeconds / ENGINE_FIXED_TICK_INTERVAL);
			i32 newTargetTick = (i32)replay->currentTick + ticksToSeek;

			// Clamp to valid range
			if (newTargetTick < 0)
			{
				targetTick = 0;
			}
			else if (newTargetTick >= (i32)replay->tickCount)
			{
				targetTick = replay->tickCount - 1;
			}
			else
			{
				targetTick = (u32)newTargetTick;
			}
		}
		else
		{
			// Absolute time navigation
			f64 targetSeconds = -1.0f;
			if (!utils::ParseTimeString(input, &targetSeconds))
			{
				player->languageService->PrintChat(true, false, "Replay - Invalid Absolute Time");
				return;
			}
			targetTick = (u32)(targetSeconds / ENGINE_FIXED_TICK_INTERVAL);
		}

		char time[32];
		utils::FormatTime(targetTick * ENGINE_FIXED_TICK_INTERVAL, time, sizeof(time), false);
		char maxTime[32];
		utils::FormatTime((replay->tickCount - 1) * ENGINE_FIXED_TICK_INTERVAL, maxTime, sizeof(maxTime), false);
		if (targetTick >= replay->tickCount)
		{
			player->languageService->PrintChat(true, false, "Replay - Time Out Of Range", time, maxTime);
			return;
		}

		NavigateReplay(player, targetTick);

		if (isRelative)
		{
			f64 seekSeconds = -1.0f;
			utils::ParseTimeString(input + 1, &seekSeconds);
			if (input[0] == '-')
			{
				seekSeconds = -seekSeconds;
			}
			player->languageService->PrintChat(true, false, "Replay - Seeked To Tick", seekSeconds, targetTick, time);
		}
		else
		{
			player->languageService->PrintChat(true, false, "Replay - Jumped To Tick", targetTick, time);
		}
	}

	void JumpToReplayTick(KZPlayer *player, const char *input)
	{
		if (!player)
		{
			return;
		}

		if (!data::IsReplayPlaying())
		{
			player->languageService->PrintChat(true, false, "Replay - No Replay Playing");
			return;
		}

		auto replay = data::GetCurrentReplay();
		u32 targetTick;
		bool isRelative = (input[0] == '+' || input[0] == '-');

		if (isRelative)
		{
			// Relative tick seeking
			char *endPtr;
			long tickOffset = strtol(input + 1, &endPtr, 10); // Skip the +/- sign
			if (*endPtr != '\0' || tickOffset < 0)
			{
				player->languageService->PrintChat(true, false, "Replay - Invalid Tick Number");
				return;
			}

			if (input[0] == '-')
			{
				tickOffset = -tickOffset;
			}

			// Calculate target tick based on current replay position + tick offset
			i32 newTargetTick = (i32)replay->currentTick + tickOffset;

			// Clamp to valid range
			if (newTargetTick < 0)
			{
				targetTick = 0;
			}
			else if (newTargetTick >= (i32)replay->tickCount)
			{
				targetTick = replay->tickCount - 1;
			}
			else
			{
				targetTick = (u32)newTargetTick;
			}
		}
		else
		{
			// Absolute tick navigation
			char *endPtr;
			long tickValue = strtol(input, &endPtr, 10);
			if (*endPtr != '\0' || tickValue < 0)
			{
				player->languageService->PrintChat(true, false, "Replay - Invalid Tick Number");
				return;
			}
			targetTick = (u32)tickValue;
		}

		if (targetTick >= replay->tickCount)
		{
			player->languageService->PrintChat(true, false, "Replay - Tick Out Of Range", targetTick, replay->tickCount - 1);
			return;
		}

		NavigateReplay(player, targetTick);
		char time[32];
		utils::FormatTime(targetTick * ENGINE_FIXED_TICK_INTERVAL, time, sizeof(time), false);
		player->languageService->PrintChat(true, false, "Replay - Jumped To Tick", targetTick, time);
	}

	void GetReplayInfo(KZPlayer *player)
	{
		if (!player)
		{
			return;
		}

		if (!data::IsReplayPlaying())
		{
			player->languageService->PrintChat(true, false, "Replay - No Replay Playing");
			return;
		}

		auto replay = data::GetCurrentReplay();

		char timeStr[64], maxTime[64];
		utils::FormatTime(replay->currentTick * ENGINE_FIXED_TICK_INTERVAL, timeStr, sizeof(timeStr), false);
		utils::FormatTime((replay->tickCount - 1) * ENGINE_FIXED_TICK_INTERVAL, maxTime, sizeof(maxTime), false);
		char timestamp[64];
		time_t time = replay->header.timestamp();
		strftime(timestamp, 64, "%Y-%m-%d %H:%M:%S", localtime(&time));
		player->languageService->PrintChat(true, false, "Replay - Current Info", replay->currentTick, replay->tickCount - 1, timeStr, maxTime);
		player->languageService->PrintConsole(false, false, "Replay - General Info Console", replay->uuid.ToString().c_str(),
											  replay->header.player().name().c_str(), replay->header.player().steamid64(), timestamp,
											  replay->header.server_version(), replay->header.plugin_version());
		switch (static_cast<ReplayType>(replay->header.type()))
		{
			case ReplayType::RP_CHEATER:
			{
				if (replay->header.has_cheater())
				{
					player->languageService->PrintConsole(false, false, "Replay - Cheater Info Console", replay->header.cheater().reason().c_str());
				}
				break;
			}
			case ReplayType::RP_RUN:
			{
				if (replay->header.has_run())
				{
					char modeStr[128];
					auto &run = replay->header.run();
					i32 styleCount = run.styles_size();
					V_snprintf(modeStr, sizeof(modeStr), "%s%s", run.mode().name().c_str(), styleCount ? "*" : "");
					CUtlString timeString = utils::FormatTime(run.time());
					player->languageService->PrintConsole(false, false, "Replay - Run Info Console", run.course_name().c_str(), modeStr,
														  timeString.Get(), run.num_teleports());
				}
				break;
			}
			case ReplayType::RP_JUMPSTATS:
			{
				if (replay->header.has_jump())
				{
					auto &jump = replay->header.jump();
					u8 jt = static_cast<u8>(jump.jump_type());
					if (jump.block_distance() <= 0)
					{
						player->languageService->PrintConsole(false, false, "Replay - Jump Info Console", jumpTypeStr[jt], jump.distance(),
															  jump.sync() * 100.0f, jump.pre(), jump.max(), jump.air_time());
					}
					else
					{
						player->languageService->PrintConsole(false, false, "Replay - Jump Info Console", jumpTypeStr[jt], jump.distance(),
															  jump.block_distance(), jump.sync() * 100.0f, jump.pre(), jump.max(), jump.air_time());
					}
				}
				break;
			}
			case ReplayType::RP_MANUAL:
			{
				if (replay->header.has_manual())
				{
					auto &manual = replay->header.manual();
					if (manual.has_saved_by())
					{
						player->languageService->PrintConsole(false, false, "Replay - Manual Info Console", manual.saved_by().name().c_str(),
															  manual.saved_by().steamid64());
					}
				}
				break;
			}
		}
	}

	void ToggleReplayPause(KZPlayer *player)
	{
		if (!player)
		{
			return;
		}

		if (!data::IsReplayPlaying())
		{
			player->languageService->PrintChat(true, false, "Replay - No Replay Playing");
			return;
		}

		auto replay = data::GetCurrentReplay();

		// Toggle pause state
		replay->replayPaused = !replay->replayPaused;

		if (replay->replayPaused)
		{
			player->languageService->PrintChat(true, false, "Replay - Paused");
		}
		else
		{
			player->languageService->PrintChat(true, false, "Replay - Resumed");
		}
	}

	void CheckReplayLoadProgress(KZPlayer *player)
	{
		if (!player)
		{
			return;
		}

		auto status = data::GetLoadStatus();

		switch (status->state.load())
		{
			case data::LoadingState::Idle:
				player->languageService->PrintChat(true, false, "Replay - No Loading Progress");
				break;

			case data::LoadingState::Loading:
			{
				float progress = status->progress.load() * 100.0f;
				player->languageService->PrintChat(true, false, "Replay - Loading Progress", progress);
				break;
			}

			case data::LoadingState::Completed:
				player->languageService->PrintChat(true, false, "Replay - Loading Completed");
				break;

			case data::LoadingState::Failed:
			{
				std::lock_guard<std::mutex> lock(status->errorMutex);
				player->languageService->PrintChat(true, false, status->errorMessage.c_str());
				break;
			}
		}
	}

	void CancelReplayLoad(KZPlayer *player)
	{
		if (!player)
		{
			return;
		}

		if (!data::IsLoading())
		{
			player->languageService->PrintChat(true, false, "Replay - No Loading Progress");
			return;
		}

		data::CancelAsyncLoad();
		player->languageService->PrintChat(true, false, "Replay - Loading Cancelled");
	}

	void ListReplays(KZPlayer *player, const char *filter)
	{
		if (!player)
		{
			return;
		}
		if (!filter || filter[0] == '\0')
		{
			g_ReplayWatcher.PrintUsage(player);
		}
		g_ReplayWatcher.FindReplaysMatchingCriteria(filter, player);
	}

	void ToggleLegsVisibility(KZPlayer *player)
	{
		if (!player)
		{
			return;
		}
		if (!data::IsReplayPlaying())
		{
			player->languageService->PrintChat(true, false, "Replay - No Replay Playing");
			return;
		}
		bot::GetBotPlayer()->ToggleHideLegs();
		if (bot::GetBotPlayer()->optionService->GetPreferenceBool("hideLegs"))
		{
			player->languageService->PrintChat(true, false, "Replay - Hide Player Legs - Enable");
		}
		else
		{
			player->languageService->PrintChat(true, false, "Replay - Hide Player Legs - Disable");
		}
	}

	static void PlayReplayByID(KZPlayer *player, const char *idStr)
	{
		UUID_t uuid;
		if (!UUID_t::FromString(idStr, &uuid))
		{
			player->languageService->PrintChat(true, false, "Replay - Not Found");
			return;
		}
		char replayPath[512];
		V_snprintf(replayPath, sizeof(replayPath), KZ_REPLAY_PATH "/%s.replay", uuid.ToString().c_str());
		if (g_pFullFileSystem->FileExists(replayPath))
		{
			LoadReplay(player, uuid.ToString().c_str());
		}
		else if (KZGlobalService::IsAvailable())
		{
			KZGlobalService::RequestReplay(player, uuid);
		}
		else
		{
			player->languageService->PrintChat(true, false, "Replay - Not Found");
		}
	}

	struct RecordContext
	{
		CPlayerUserId playerUserID;
		CUtlString mapName;
		CUtlString courseName;
		KZ::api::Mode apiMode;
		u32 localModeID;

		RecordContext(CPlayerUserId uid, CUtlString map, CUtlString course, KZ::api::Mode mode, u32 modeID)
			: playerUserID(uid), mapName(std::move(map)), courseName(std::move(course)), apiMode(mode), localModeID(modeID)
		{
		}
	};

	static void LoadWRReplay(KZPlayer *player, bool isPro, const RecordContext &ctx)
	{
		if (!KZGlobalService::IsAvailable())
		{
			player->languageService->PrintChat(true, false, "Replay - API Not Available");
			return;
		}
		player->languageService->PrintChat(true, false, "Replay - Querying WR");
		// clang-format off
		KZGlobalService::MessageCallback<KZ::api::messages::WorldRecords> callback(
			[userID = ctx.playerUserID, isPro](const KZ::api::messages::WorldRecords &records)
			{
				KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
				if (!p) return;
				const auto &record = isPro ? records.pro : records.overall;
				if (!record.has_value())
				{
					p->languageService->PrintChat(true, false, "Replay - WR Not Found");
					return;
				}
				PlayReplayByID(p, record->id.c_str());
			});
		callback.OnError([userID = ctx.playerUserID](const KZ::api::messages::Error &)
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (p) p->languageService->PrintChat(true, false, "Replay - WR Not Found");
		});
		callback.OnCancelled([userID = ctx.playerUserID](KZGlobalService::MessageCallbackCancelReason)
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (p) p->languageService->PrintChat(true, false, "Replay - WR Not Found");
		});
		// clang-format on
		KZGlobalService::QueryWorldRecords(std::string_view(ctx.mapName.Get(), ctx.mapName.Length()),
										   std::string_view(ctx.courseName.Get(), ctx.courseName.Length()), ctx.apiMode, std::move(callback));
	}

	static void LoadSRReplay(KZPlayer *player, bool isPro, const RecordContext &ctx)
	{
		// clang-format off
		auto onSuccess = [userID = ctx.playerUserID, isPro](std::vector<ISQLQuery *> queries)
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (!p) return;
			ISQLResult *result = (isPro ? queries[1] : queries[0])->GetResultSet();
			if (result && result->GetRowCount() > 0 && result->FetchRow())
				PlayReplayByID(p, result->GetString(0));
			else
				p->languageService->PrintChat(true, false, "Replay - SR Not Found");
		};
		auto onFailure = [userID = ctx.playerUserID](std::string, int)
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (p) p->languageService->PrintChat(true, false, "Replay - SR Not Found");
		};
		// clang-format on
		KZDatabaseService::QueryRecords(ctx.mapName, ctx.courseName, ctx.localModeID, 1, 0, onSuccess, onFailure);
	}

	static void LoadGPBReplay(KZPlayer *player, bool isPro, const RecordContext &ctx)
	{
		if (!KZGlobalService::IsAvailable())
		{
			player->languageService->PrintChat(true, false, "Replay - API Not Available");
			return;
		}
		player->languageService->PrintChat(true, false, "Replay - Querying GPB");
		KZGlobalService::QueryPBParams params;
		params.player = KZGlobalService::PlayerIdentifier::SteamID(player->GetSteamId64());
		params.map = std::string_view(ctx.mapName.Get(), ctx.mapName.Length());
		params.course = std::string_view(ctx.courseName.Get(), ctx.courseName.Length());
		params.mode = ctx.apiMode;
		// clang-format off
		KZGlobalService::MessageCallback<KZ::api::messages::PersonalBest> callback(
			[userID = ctx.playerUserID, isPro](const KZ::api::messages::PersonalBest &pb)
			{
				KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
				if (!p) return;
				const auto &record = isPro ? pb.pro : pb.overall;
				if (!record.has_value())
				{
					p->languageService->PrintChat(true, false, "Replay - GPB Not Found");
					return;
				}
				PlayReplayByID(p, record->id.c_str());
			});
		callback.OnError([userID = ctx.playerUserID](const KZ::api::messages::Error &)
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (p) p->languageService->PrintChat(true, false, "Replay - GPB Not Found");
		});
		callback.OnCancelled([userID = ctx.playerUserID](KZGlobalService::MessageCallbackCancelReason)
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (p) p->languageService->PrintChat(true, false, "Replay - GPB Not Found");
		});
		// clang-format on
		KZGlobalService::QueryPB(params, std::move(callback));
	}

	static void LoadSPBReplay(KZPlayer *player, bool isPro, const RecordContext &ctx)
	{
		u64 steamID64 = player->GetSteamId64();
		// clang-format off
		auto onSuccess = [userID = ctx.playerUserID, isPro](std::vector<ISQLQuery *> queries)
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (!p) return;
			// queries[0] = sql_getpb (ID col 0), queries[1] = sql_getpbpro (ID col 0)
			ISQLResult *result = (isPro ? queries[1] : queries[0])->GetResultSet();
			if (result && result->GetRowCount() > 0 && result->FetchRow())
				PlayReplayByID(p, result->GetString(0));
			else
				p->languageService->PrintChat(true, false, "Replay - SPB Not Found");
		};
		auto onFailure = [userID = ctx.playerUserID](std::string, int)
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (p) p->languageService->PrintChat(true, false, "Replay - SPB Not Found");
		};
		// clang-format on
		KZDatabaseService::QueryPBRankless(steamID64, ctx.mapName, ctx.courseName, ctx.localModeID, 0, onSuccess, onFailure);
	}

	static void LoadPBReplay(KZPlayer *player, bool isPro, bool isGlobalMode, const RecordContext &ctx)
	{
		if (!isGlobalMode || !KZGlobalService::IsAvailable())
		{
			LoadSPBReplay(player, isPro, ctx);
			return;
		}
		player->languageService->PrintChat(true, false, "Replay - Querying GPB");
		KZGlobalService::QueryPBParams params;
		params.player = KZGlobalService::PlayerIdentifier::SteamID(player->GetSteamId64());
		params.map = std::string_view(ctx.mapName.Get(), ctx.mapName.Length());
		params.course = std::string_view(ctx.courseName.Get(), ctx.courseName.Length());
		params.mode = ctx.apiMode;
		// clang-format off
		KZGlobalService::MessageCallback<KZ::api::messages::PersonalBest> callback(
			[userID = ctx.playerUserID, isPro, mapName = ctx.mapName, courseName = ctx.courseName, localModeID = ctx.localModeID]
			(const KZ::api::messages::PersonalBest &pb)
			{
				KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
				if (!p) return;
				const auto &record = isPro ? pb.pro : pb.overall;
				if (record.has_value())
				{
					PlayReplayByID(p, record->id.c_str());
					return;
				}
				// GPB not found — fall back to server PB
				RecordContext spbCtx(userID, mapName, courseName, {}, localModeID);
				LoadSPBReplay(p, isPro, spbCtx);
			});
		auto spbFallback = [userID = ctx.playerUserID, isPro, mapName = ctx.mapName, courseName = ctx.courseName, localModeID = ctx.localModeID]()
		{
			KZPlayer *p = g_pKZPlayerManager->ToPlayer(userID);
			if (!p) return;
			RecordContext spbCtx(userID, mapName, courseName, {}, localModeID);
			LoadSPBReplay(p, isPro, spbCtx);
		};
		callback.OnError([spbFallback](const KZ::api::messages::Error &) { spbFallback(); });
		callback.OnCancelled([spbFallback](KZGlobalService::MessageCallbackCancelReason) { spbFallback(); });
		// clang-format on
		KZGlobalService::QueryPB(params, std::move(callback));
	}

	void LoadReplayForRecord(KZPlayer *player, RecordType type, const char *courseArg, const char *modeArg)
	{
		if (!player)
		{
			return;
		}

		const KZCourseDescriptor *course = nullptr;
		if (courseArg && courseArg[0] != '\0')
		{
			course = KZ::course::GetCourse(courseArg, false, true);
		}
		if (!course)
		{
			course = player->timerService->GetCourse();
		}
		if (!course)
		{
			course = KZ::course::GetFirstCourse();
		}
		if (!course)
		{
			player->languageService->PrintChat(true, false, "Replay - No Course");
			return;
		}

		KZModeManager::ModePluginInfo modeInfo;
		if (modeArg && modeArg[0] != '\0')
		{
			modeInfo = KZ::mode::GetModeInfo(CUtlString(modeArg));
		}
		else
		{
			modeInfo = KZ::mode::GetModeInfo(player->modeService);
		}
		if (modeInfo.id == -2)
		{
			player->languageService->PrintChat(true, false, "Replay - Invalid Mode Arg");
			return;
		}

		KZ::api::Mode apiMode {};
		bool isGlobalMode = KZ::api::DecodeModeString(std::string_view(modeInfo.shortModeName.Get(), modeInfo.shortModeName.Length()), apiMode);

		if (!isGlobalMode && (type == RecordType::WR || type == RecordType::WRPro || type == RecordType::GPB || type == RecordType::GPBPro))
		{
			player->languageService->PrintChat(true, false, "Replay - Global Mode Only");
			return;
		}

		RecordContext ctx(player->GetClient()->GetUserID(), g_pKZUtils->GetCurrentMapName(), course->GetName(), apiMode, (u32)modeInfo.databaseID);

		switch (type)
		{
				// clang-format off
			case RecordType::WR:     LoadWRReplay(player, false, ctx); break;
			case RecordType::WRPro:  LoadWRReplay(player, true,  ctx); break;
			case RecordType::SR:     LoadSRReplay(player, false, ctx); break;
			case RecordType::SRPro:  LoadSRReplay(player, true,  ctx); break;
			case RecordType::GPB:    LoadGPBReplay(player, false, ctx); break;
			case RecordType::GPBPro: LoadGPBReplay(player, true,  ctx); break;
			case RecordType::SPB:    LoadSPBReplay(player, false, ctx); break;
			case RecordType::SPBPro: LoadSPBReplay(player, true,  ctx); break;
			// clang-format on
			case RecordType::PB:
			case RecordType::PBPro:
			{
				bool isPro = (type == RecordType::PBPro);
				LoadPBReplay(player, isPro, isGlobalMode, ctx);
				break;
			}
		}
	}
} // namespace KZ::replaysystem::commands

SCMD(kz_replay, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!g_pFullFileSystem || !player)
	{
		return MRES_SUPERCEDE;
	}

	if (args->ArgC() < 2)
	{
		player->languageService->PrintChat(true, false, "Replay - Usage Command");
		return MRES_SUPERCEDE;
	}

	using namespace KZ::replaysystem::commands;
	using RT = RecordType;

	static const struct
	{
		const char *keyword;
		RT type;
	} recordKeywords[] = {
		{"wr", RT::WR},       {"wrpro", RT::WRPro}, {"sr", RT::SR},         {"srpro", RT::SRPro}, {"pb", RT::PB},
		{"pbpro", RT::PBPro}, {"gpb", RT::GPB},     {"gpbpro", RT::GPBPro}, {"spb", RT::SPB},     {"spbpro", RT::SPBPro},
	};

	const char *arg1 = args->Arg(1);
	for (const auto &kw : recordKeywords)
	{
		if (KZ_STREQI(arg1, kw.keyword))
		{
			LoadReplayForRecord(player, kw.type, args->ArgC() >= 3 ? args->Arg(2) : "", args->ArgC() >= 4 ? args->Arg(3) : "");
			return MRES_SUPERCEDE;
		}
	}

	LoadReplay(player, arg1);
	return MRES_SUPERCEDE;
}

SCMD(kz_rpgoto, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	if (args->ArgC() < 2)
	{
		player->languageService->PrintChat(true, false, "Replay - Usage Goto Time");
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::JumpToReplayTime(player, args->ArgS());
	return MRES_SUPERCEDE;
}

SCMD(kz_rpgototick, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	if (args->ArgC() < 2)
	{
		player->languageService->PrintChat(true, false, "Replay - Usage Goto Tick");
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::JumpToReplayTick(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

SCMD(kz_rpinfo, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::GetReplayInfo(player);
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_rpseek, kz_rpgoto);

SCMD(kz_rppause, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::ToggleReplayPause(player);
	return MRES_SUPERCEDE;
}

SCMD(kz_rploadprogress, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::CheckReplayLoadProgress(player);
	return MRES_SUPERCEDE;
}

SCMD(kz_rpcancelload, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::CancelReplayLoad(player);
	return MRES_SUPERCEDE;
}

SCMD(kz_replays, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::ListReplays(player, args->ArgS());
	return MRES_SUPERCEDE;
}

SCMD(kz_rphidelegs, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}
	KZ::replaysystem::commands::ToggleLegsVisibility(player);
	return MRES_SUPERCEDE;
}
