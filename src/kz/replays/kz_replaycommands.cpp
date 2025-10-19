#include "cs2kz.h"
#include "kz/kz.h"
#include "kz/language/kz_language.h"
#include "kz_replaycommands.h"
#include "kz_replaydata.h"
#include "kz_replaybot.h"
#include "kz_replayevents.h"
#include "kz_replayplayback.h"
#include "utils/uuid.h"
#include "utils/simplecmds.h"
#include <functional>

static_function bool IsValidTimeString(const char *timeStr, f32 *outSeconds)
{
	if (!timeStr || !outSeconds)
	{
		return false;
	}

	// Skip leading whitespace
	while (*timeStr && isspace(*timeStr))
	{
		timeStr++;
	}

	if (*timeStr == '\0')
	{
		return false;
	}

	f32 parts[3] = {0.0f, 0.0f, 0.0f};
	i32 partCount = 0;
	const char *ptr = timeStr;

	// Parse up to 3 colon-separated parts
	while (partCount < 3)
	{
		// Parse the integer part
		if (!isdigit(*ptr))
		{
			return false;
		}

		i32 intPart = 0;
		while (isdigit(*ptr))
		{
			intPart = intPart * 10 + (*ptr - '0');
			ptr++;
		}

		parts[partCount] = (float)intPart;

		// Check for decimal part
		if (*ptr == '.')
		{
			ptr++;
			if (!isdigit(*ptr))
			{
				return false;
			}

			f32 fracPart = 0.0f;
			f32 divisor = 10.0f;
			while (isdigit(*ptr))
			{
				fracPart += (*ptr - '0') / divisor;
				divisor *= 10.0f;
				ptr++;
			}

			parts[partCount] += fracPart;
		}

		partCount++;

		// Check what comes next
		if (*ptr == ':')
		{
			ptr++;
			if (partCount >= 3)
			{
				return false; // Too many colons
			}
		}
		else
		{
			break; // End of parsing
		}
	}

	// Skip trailing whitespace
	while (*ptr && isspace(*ptr))
	{
		ptr++;
	}

	// Should be at end of string
	if (*ptr != '\0')
	{
		return false;
	}

	// Validate ranges and convert to seconds
	f32 totalSeconds = 0.0f;

	if (partCount == 1)
	{
		// Just seconds: 12 or 12.5
		totalSeconds = parts[0];
	}
	else if (partCount == 2)
	{
		// Minutes:seconds: 12:34
		if (parts[1] >= 60.0f)
		{
			return false;
		}
		totalSeconds = parts[0] * 60.0f + parts[1];
	}
	else if (partCount == 3)
	{
		// Hours:minutes:seconds: 12:34:45
		if (parts[1] >= 60.0f || parts[2] >= 60.0f)
		{
			return false;
		}
		totalSeconds = parts[0] * 3600.0f + parts[1] * 60.0f + parts[2];
	}

	*outSeconds = totalSeconds;
	return true;
}

static_function f32 ParseTimeString(const char *timeStr)
{
	f32 seconds;
	if (IsValidTimeString(timeStr, &seconds))
	{
		return seconds;
	}
	return -1.0f; // Invalid input
}

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
				if (player)
				{
					player->languageService->PrintChat(true, false, "Replay - Loaded Successfully");
					
					// Initialize bot and start playback (safe to call on main thread)
					// The replay data is already stored in the global g_currentReplay
					auto replay = data::GetCurrentReplay();
					bot::InitializeBotForReplay(replay->header);
					playback::StartReplay();
					playback::InitializeWeapons();
					bot::SpectateBot(player);
				}
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
			f32 seekSeconds = ParseTimeString(input + 1); // Skip the +/- sign
			if (seekSeconds < 0.0f)
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
			f32 targetSeconds = ParseTimeString(input);
			if (targetSeconds < 0.0f)
			{
				player->languageService->PrintChat(true, false, "Replay - Invalid Absolute Time");
				return;
			}
			targetTick = (u32)(targetSeconds / ENGINE_FIXED_TICK_INTERVAL);
		}

		if (targetTick >= replay->tickCount)
		{
			player->languageService->PrintChat(true, false, "Replay - Tick Out Of Range", targetTick, replay->tickCount - 1);
			return;
		}

		NavigateReplay(player, targetTick);

		f32 currentTime = data::GetReplayTime();
		if (isRelative)
		{
			f32 seekSeconds = ParseTimeString(input + 1);
			if (input[0] == '-')
			{
				seekSeconds = -seekSeconds;
			}
			player->languageService->PrintChat(true, false, "Replay - Seeked To Tick", seekSeconds, targetTick, currentTime);
		}
		else
		{
			player->languageService->PrintChat(true, false, "Replay - Jumped To Tick", targetTick);
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
		player->languageService->PrintChat(true, false, "Replay - Jumped To Tick", targetTick);
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
		f32 currentTime = data::GetReplayTime();

		player->languageService->PrintChat(true, false, "Replay - Current Info", replay->currentTick, replay->tickCount - 1, currentTime,
										   replay->currentCheckpoint, replay->currentTeleport);
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
