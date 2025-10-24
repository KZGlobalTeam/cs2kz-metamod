#include "cs2kz.h"
#include "kz/kz.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz_replaycommands.h"
#include "kz_replaydata.h"
#include "kz_replaybot.h"
#include "kz_replayevents.h"
#include "kz_replayplayback.h"
#include "utils/uuid.h"
#include "utils/simplecmds.h"
#include <functional>

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

	void HandleReplayCommand(KZPlayer *player, const char *uuid)
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
		if (!UUID_t::IsValid(uuid, &parsedUuid))
		{
			player->languageService->PrintChat(true, false, "Replay - Invalid UUID");
			return;
		}

		// Validate uuid format
		char replayPath[512];
		V_snprintf(replayPath, sizeof(replayPath), KZ_REPLAY_PATH "/%s.replay", parsedUuid.ToString().c_str());

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
				if (!KZ_STREQI(data::GetCurrentReplay()->header.map.name, g_pKZUtils->GetCurrentMapName().Get()))
				{
					player->languageService->PrintChat(true, false, "Replay - Wrong Map");
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

	void HandleGotoCommand(KZPlayer *player, const char *input)
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
		utils::FormatTime(targetTick, time, sizeof(time), false);
		char maxTime[32];
		utils::FormatTime((replay->tickCount - 1), maxTime, sizeof(maxTime), false);
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

	void HandleGotoTickCommand(KZPlayer *player, const char *input)
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
		utils::FormatTime(targetTick, time, sizeof(time), false);
		player->languageService->PrintChat(true, false, "Replay - Jumped To Tick", targetTick, time);
	}

	void HandleInfoCommand(KZPlayer *player)
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
		utils::FormatTime(replay->currentTick, timeStr, sizeof(timeStr), false);
		utils::FormatTime(replay->tickCount - 1, maxTime, sizeof(maxTime), false);
		char timestamp[64];
		time_t time = replay->header.timestamp;
		strftime(timestamp, 64, "%Y-%m-%d %H:%M:%S", localtime(&time));
		player->languageService->PrintChat(true, false, "Replay - Current Info", replay->currentTick, replay->tickCount - 1, timeStr, maxTime);
		player->languageService->PrintConsole(false, false, "Replay - General Info Console", replay->uuid.ToString().c_str(),
											  replay->header.player.name, replay->header.player.steamid64, timestamp, replay->header.serverVersion,
											  replay->header.pluginVersion);
		switch (replay->header.type)
		{
			case ReplayType::RP_CHEATER:
			{
				player->languageService->PrintConsole(false, false, "Replay - Cheater Info Console", replay->cheaterHeader.reason);
				break;
			}
			case ReplayType::RP_RUN:
			{
				char modeStr[128];
				V_snprintf(modeStr, sizeof(modeStr), "%s%s", replay->runHeader.mode.name, replay->runHeader.styleCount ? "*" : "");
				CUtlString timeString = utils::FormatTime(replay->runHeader.time);
				player->languageService->PrintConsole(false, false, "Replay - Run Info Console", replay->runHeader.courseID, modeStr,
													  timeString.Get(), replay->runHeader.numTeleports);
				break;
			}
			case ReplayType::RP_JUMPSTATS:
			{
				if (replay->jumpHeader.blockDistance <= 0)
				{
					player->languageService->PrintConsole(false, false, "Replay - Jump Info Console", jumpTypeStr[replay->jumpHeader.jumpType],
														  replay->jumpHeader.distance, replay->jumpHeader.sync * 100.0f, replay->jumpHeader.pre,
														  replay->jumpHeader.max, replay->jumpHeader.airTime);
				}
				else
				{
					player->languageService->PrintConsole(false, false, "Replay - Jump Info Console", jumpTypeStr[replay->jumpHeader.jumpType],
														  replay->jumpHeader.distance, replay->jumpHeader.blockDistance,
														  replay->jumpHeader.sync * 100.0f, replay->jumpHeader.pre, replay->jumpHeader.max,
														  replay->jumpHeader.airTime);
				}
				break;
			}
		}
	}

	void HandlePauseCommand(KZPlayer *player)
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

	void HandleLoadProgressCommand(KZPlayer *player)
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

	void HandleCancelLoadCommand(KZPlayer *player)
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

} // namespace KZ::replaysystem::commands

// Command implementations using the new modules
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

	KZ::replaysystem::commands::HandleReplayCommand(player, args->Arg(1));
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

	KZ::replaysystem::commands::HandleGotoCommand(player, args->ArgS());
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

	KZ::replaysystem::commands::HandleGotoTickCommand(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

SCMD(kz_rpinfo, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::HandleInfoCommand(player);
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

	KZ::replaysystem::commands::HandlePauseCommand(player);
	return MRES_SUPERCEDE;
}

SCMD(kz_rploadprogress, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::HandleLoadProgressCommand(player);
	return MRES_SUPERCEDE;
}

SCMD(kz_rpcancelload, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return MRES_SUPERCEDE;
	}

	KZ::replaysystem::commands::HandleCancelLoadCommand(player);
	return MRES_SUPERCEDE;
}
