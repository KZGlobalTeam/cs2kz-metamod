#include "cs2kz.h"
#include "kz_replaydata.h"
#include "filesystem.h"
#include "utils/utils.h"
#include "utils/uuid.h"
#include <thread>
#include <mutex>
#include <atomic>

CConVar<bool> kz_replay_playback_debug("kz_replay_playback_debug", FCVAR_NONE, "Prints debug info about replay playback.", true);
CConVar<bool> kz_replay_playback_skins_enable("kz_replay_playback_skins_enable", FCVAR_NONE, "Enables applying player skins during replay playback.",
											  true);

static KZ::replaysystem::data::ReplayPlayback g_currentReplay = {};
static KZ::replaysystem::data::AsyncLoadStatus g_loadStatus = {};
static std::thread g_loadThread;
static std::atomic<bool> g_cancelLoad {false};

namespace KZ::replaysystem::data
{
	void FreeReplayData(ReplayPlayback *replay)
	{
		if (replay->tickData)
		{
			delete[] replay->tickData;
			replay->tickData = nullptr;
		}
		if (replay->subtickData)
		{
			delete[] replay->subtickData;
			replay->subtickData = nullptr;
		}
		if (replay->weapons)
		{
			delete[] replay->weapons;
			replay->weapons = nullptr;
		}
		if (replay->jumps)
		{
			delete[] replay->jumps;
			replay->jumps = nullptr;
		}
		if (replay->events)
		{
			delete[] replay->events;
			replay->events = nullptr;
		}
		*replay = {};
	}

	void ResetReplayState(ReplayPlayback *replay)
	{
		// Reset all tracking indices
		replay->currentWeapon = 0;
		replay->currentJump = 0;
		replay->currentEvent = 0;

		// Reset checkpoint/teleport counters
		replay->currentCpIndex = -1;
		replay->currentCheckpoint = 0;
		replay->currentTeleport = 0;

		// Reset timer state
		replay->courseName[0] = '\0';
		replay->startTime = 0.0f;
		replay->paused = false;
		replay->endTime = 0.0f;
		replay->stopTick = 0;
		replay->lastSplitTime = 0.0f;
		replay->lastCPZTime = 0.0f;
		replay->lastStageTime = 0.0f;

		// Reset replay pause state
		replay->replayPaused = false;
	}

	ReplayPlayback *GetCurrentReplay()
	{
		return &g_currentReplay;
	}

	bool IsReplayValid()
	{
		return g_currentReplay.valid;
	}

	bool IsReplayPlaying()
	{
		return g_currentReplay.playingReplay;
	}

	void SetCurrentTick(u32 tick)
	{
		g_currentReplay.currentTick = tick;
	}

	u32 GetCurrentTick()
	{
		return g_currentReplay.currentTick;
	}

	u32 GetTickCount()
	{
		return g_currentReplay.tickCount;
	}

	i32 GetCurrentCpIndex()
	{
		return g_currentReplay.currentCpIndex;
	}

	i32 GetCheckpointCount()
	{
		return g_currentReplay.currentCheckpoint;
	}

	i32 GetTeleportCount()
	{
		return g_currentReplay.currentTeleport;
	}

	f32 GetReplayTime()
	{
		return g_currentReplay.startTime == 0.0f ? 0.0f : (g_pKZUtils->GetServerGlobals()->curtime - g_currentReplay.startTime);
	}

	f32 GetEndTime()
	{
		if (g_currentReplay.stopTick == 0)
		{
			return 0.0f;
		}
		if (g_currentReplay.stopTick + 192 < g_currentReplay.currentTick) // 3 seconds
		{
			return 0.0f;
		}
		return g_currentReplay.endTime;
	}

	bool GetPaused()
	{
		return g_currentReplay.paused;
	}

	// Helper function to load replay data with progress reporting
	static ReplayPlayback LoadReplayWithProgress(const char *path, std::atomic<f32> &progress, std::atomic<bool> &shouldCancel)
	{
		ReplayPlayback result = {};
		progress = 0.0f;

		FileHandle_t file = g_pFullFileSystem->Open(path, "rb");
		if (!file)
		{
			return result;
		}

		// Get file size for progress calculation
		size_t fileSize = g_pFullFileSystem->Size(file);

		auto updateProgress = [&](size_t bytesRead)
		{
			if (fileSize > 0)
			{
				progress = static_cast<f32>(bytesRead) / static_cast<f32>(fileSize);
				if (kz_replay_playback_debug.Get())
				{
					META_CONPRINTF("Replay load progress: %d bytes, %.2f%%\n", bytesRead, progress.load() * 100.0f);
				}
			}
		};

		size_t totalBytesRead = 0;

		// Read header
		if (shouldCancel)
		{
			g_pFullFileSystem->Close(file);
			return result;
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading replay header...\n");
		}
		g_pFullFileSystem->Read(&result.header, sizeof(result.header), file);
		totalBytesRead += sizeof(result.header);
		switch (result.header.type)
		{
			case RP_CHEATER:
			{
				totalBytesRead += g_pFullFileSystem->Read(&result.cheaterHeader, sizeof(result.cheaterHeader), file);
				break;
			}
			case RP_RUN:
			{
				totalBytesRead += g_pFullFileSystem->Read(&result.runHeader, sizeof(result.runHeader), file);
				// Just to advance the reader.
				for (i32 i = 0; i < result.runHeader.styleCount; i++)
				{
					RpModeStyleInfo style = {};
					totalBytesRead += g_pFullFileSystem->Read(&style, sizeof(style), file);
				}
				break;
			}
			case RP_JUMPSTATS:
			{
				totalBytesRead += g_pFullFileSystem->Read(&result.jumpHeader, sizeof(result.jumpHeader), file);
				break;
			}
			case RP_MANUAL:
			{
				totalBytesRead += g_pFullFileSystem->Read(&result.manualHeader, sizeof(result.manualHeader), file);
				break;
			}
		}
		updateProgress(totalBytesRead);

		if (result.header.magicNumber != KZ_REPLAY_MAGIC)
		{
			g_pFullFileSystem->Close(file);
			return result;
		}
		if (result.header.version != KZ_REPLAY_VERSION)
		{
			g_pFullFileSystem->Close(file);
			return result;
		}

		// Load tick data
		if (shouldCancel)
		{
			g_pFullFileSystem->Close(file);
			return result;
		}
		g_pFullFileSystem->Read(&result.tickCount, sizeof(result.tickCount), file);
		totalBytesRead += sizeof(result.tickCount);
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading %u ticks...\n", result.tickCount);
		}
		updateProgress(totalBytesRead);

		result.tickData = new TickData[result.tickCount];
		for (u32 i = 0; i < result.tickCount; i++)
		{
			if (shouldCancel)
			{
				delete[] result.tickData;
				g_pFullFileSystem->Close(file);
				return {};
			}

			TickData tickData = {};
			g_pFullFileSystem->Read(&tickData, sizeof(tickData), file);
			result.tickData[i] = tickData;
			totalBytesRead += sizeof(tickData);

			// Update progress every 1000 ticks to avoid too frequent updates
			if (i % 1000 == 0)
			{
				updateProgress(totalBytesRead);
			}
		}
		updateProgress(totalBytesRead);

		// Load subtick data
		if (shouldCancel)
		{
			delete[] result.tickData;
			g_pFullFileSystem->Close(file);
			return {};
		}
		result.subtickData = new SubtickData[result.tickCount];
		for (u32 i = 0; i < result.tickCount; i++)
		{
			if (shouldCancel)
			{
				delete[] result.tickData;
				delete[] result.subtickData;
				g_pFullFileSystem->Close(file);
				return {};
			}

			SubtickData subtickData = {};
			g_pFullFileSystem->Read(&subtickData.numSubtickMoves, sizeof(subtickData.numSubtickMoves), file);
			totalBytesRead += sizeof(subtickData.numSubtickMoves);

			g_pFullFileSystem->Read(&subtickData.subtickMoves, sizeof(SubtickData::RpSubtickMove) * subtickData.numSubtickMoves, file);
			totalBytesRead += sizeof(SubtickData::RpSubtickMove) * subtickData.numSubtickMoves;

			result.subtickData[i] = subtickData;

			if (i % 1000 == 0)
			{
				updateProgress(totalBytesRead);
			}
		}
		updateProgress(totalBytesRead);

		// Load weapon data
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			g_pFullFileSystem->Close(file);
			return {};
		}
		g_pFullFileSystem->Read(&result.numWeapons, sizeof(result.numWeapons), file);
		totalBytesRead += sizeof(result.numWeapons);
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading %d weapon change events...\n", result.numWeapons);
		}
		result.weapons = new WeaponSwitchEvent[result.numWeapons + 1];
		result.weapons[0] = {0, result.header.firstWeapon};

		for (i32 i = 0; i < result.numWeapons; i++)
		{
			if (shouldCancel)
			{
				delete[] result.tickData;
				delete[] result.subtickData;
				delete[] result.weapons;
				g_pFullFileSystem->Close(file);
				return {};
			}

			g_pFullFileSystem->Read(&result.weapons[i + 1].serverTick, sizeof(result.weapons[i + 1].serverTick), file);
			g_pFullFileSystem->Read(&result.weapons[i + 1].econInfo.mainInfo, sizeof(result.weapons[i + 1].econInfo.mainInfo), file);
			totalBytesRead += sizeof(result.weapons[i + 1].serverTick) + sizeof(result.weapons[i + 1].econInfo.mainInfo);

			for (i32 j = 0; j < result.weapons[i + 1].econInfo.mainInfo.numAttributes; j++)
			{
				g_pFullFileSystem->Read(&result.weapons[i + 1].econInfo.attributes[j], sizeof(result.weapons[i + 1].econInfo.attributes[j]), file);
				totalBytesRead += sizeof(result.weapons[i + 1].econInfo.attributes[j]);
			}
		}
		updateProgress(totalBytesRead);

		// Load jump stats
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weapons;
			g_pFullFileSystem->Close(file);
			return {};
		}
		g_pFullFileSystem->Read(&result.numJumps, sizeof(result.numJumps), file);
		totalBytesRead += sizeof(result.numJumps);
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading %d jumps...\n", result.numJumps);
		}
		result.jumps = new RpJumpStats[result.numJumps];
		for (u32 i = 0; i < result.numJumps; i++)
		{
			if (shouldCancel)
			{
				delete[] result.tickData;
				delete[] result.subtickData;
				delete[] result.weapons;
				delete[] result.jumps;
				g_pFullFileSystem->Close(file);
				return {};
			}

			g_pFullFileSystem->Read(&result.jumps[i].overall, sizeof(result.jumps[i].overall), file);
			totalBytesRead += sizeof(result.jumps[i].overall);

			// Load strafes
			i32 numStrafes = 0;
			g_pFullFileSystem->Read(&numStrafes, sizeof(numStrafes), file);
			totalBytesRead += sizeof(numStrafes);

			result.jumps[i].strafes.resize(numStrafes);
			for (i32 j = 0; j < numStrafes; j++)
			{
				g_pFullFileSystem->Read(&result.jumps[i].strafes[j], sizeof(RpJumpStats::StrafeData), file);
				totalBytesRead += sizeof(RpJumpStats::StrafeData);
			}

			// Load AA calls
			i32 numAACalls = 0;
			g_pFullFileSystem->Read(&numAACalls, sizeof(numAACalls), file);
			totalBytesRead += sizeof(numAACalls);

			result.jumps[i].aaCalls.resize(numAACalls);
			for (i32 j = 0; j < numAACalls; j++)
			{
				g_pFullFileSystem->Read(&result.jumps[i].aaCalls[j], sizeof(RpJumpStats::AAData), file);
				totalBytesRead += sizeof(RpJumpStats::AAData);
			}
		}
		updateProgress(totalBytesRead);

		// Load events
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weapons;
			delete[] result.jumps;
			g_pFullFileSystem->Close(file);
			return {};
		}
		g_pFullFileSystem->Read(&result.numEvents, sizeof(result.numEvents), file);
		totalBytesRead += sizeof(result.numEvents);
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading %d events...\n", result.numEvents);
		}
		result.events = new RpEvent[result.numEvents];
		for (u32 i = 0; i < result.numEvents; i++)
		{
			if (shouldCancel)
			{
				delete[] result.tickData;
				delete[] result.subtickData;
				delete[] result.weapons;
				delete[] result.jumps;
				delete[] result.events;
				g_pFullFileSystem->Close(file);
				return {};
			}

			g_pFullFileSystem->Read(&result.events[i], sizeof(result.events[i]), file);
			totalBytesRead += sizeof(result.events[i]);
		}

		result.valid = UUID_t::FromString(CUtlString(path).GetBaseFilename().StripExtension().Get(), &result.uuid);
		assert(result.valid);
		g_pFullFileSystem->Close(file);
		progress = 1.0f;
		return result;
	}

	void LoadReplayAsync(std::string path, LoadSuccessCallback onSuccess, LoadFailureCallback onFailure)
	{
		// Cancel any existing load
		CancelAsyncLoad();

		// Reset load status
		g_loadStatus.state = LoadingState::Loading;
		g_loadStatus.progress = 0.0f;
		g_cancelLoad = false;

		// Store callbacks safely
		{
			std::lock_guard<std::mutex> lock(g_loadStatus.callbackMutex);
			g_loadStatus.successCallback = onSuccess;
			g_loadStatus.failureCallback = onFailure;
		}

		// Clear any previous error message and completed replay
		{
			std::lock_guard<std::mutex> lock(g_loadStatus.errorMutex);
			g_loadStatus.errorMessage.clear();
		}
		{
			std::lock_guard<std::mutex> lock(g_loadStatus.replayMutex);
			FreeReplayData(&g_loadStatus.completedReplay);
			g_loadStatus.completedReplay = {};
		}

		// Start loading thread
		g_loadThread = std::thread(
			[=]()
			{
				// Copy path to avoid potential lifetime issues
				std::string pathCopy(path);

				ReplayPlayback result = LoadReplayWithProgress(pathCopy.c_str(), g_loadStatus.progress, g_cancelLoad);

				if (g_cancelLoad)
				{
					// Load was cancelled, cleanup and exit
					FreeReplayData(&result);
					g_loadStatus.state = LoadingState::Failed;
					{
						std::lock_guard<std::mutex> lock(g_loadStatus.errorMutex);
						g_loadStatus.errorMessage = "Replay - Load Cancelled";
					}
					return;
				}

				if (!result.valid)
				{
					g_loadStatus.state = LoadingState::Failed;
					{
						std::lock_guard<std::mutex> lock(g_loadStatus.errorMutex);
						g_loadStatus.errorMessage = "Replay - Invalid File Format";
					}
					return;
				}

				// Success - store result for main thread to process
				{
					std::lock_guard<std::mutex> lock(g_loadStatus.replayMutex);
					g_loadStatus.completedReplay = result;
				}
				g_loadStatus.state = LoadingState::Completed;
			});

		// Detach the thread so it can run independently
		g_loadThread.detach();
	}

	AsyncLoadStatus *GetLoadStatus()
	{
		return &g_loadStatus;
	}

	bool IsLoading()
	{
		return g_loadStatus.state == LoadingState::Loading;
	}

	void CancelAsyncLoad()
	{
		if (IsLoading())
		{
			g_cancelLoad = true;
		}
	}

	void ProcessAsyncLoadCompletion()
	{
		LoadingState currentState = g_loadStatus.state.load();

		if (currentState == LoadingState::Completed)
		{
			// Handle successful load
			ReplayPlayback completedReplay;
			LoadSuccessCallback successCallback;

			// Get the completed replay and callback
			{
				std::lock_guard<std::mutex> replayLock(g_loadStatus.replayMutex);
				std::lock_guard<std::mutex> callbackLock(g_loadStatus.callbackMutex);

				completedReplay = g_loadStatus.completedReplay;
				successCallback = g_loadStatus.successCallback;

				// Clear the completed replay from the status struct
				g_loadStatus.completedReplay = {};
			}

			// Process the completion on the main thread
			if (completedReplay.valid)
			{
				// Store the replay
				FreeReplayData(&g_currentReplay);
				g_currentReplay = completedReplay;

				// Call the success callback if it exists
				if (successCallback)
				{
					successCallback();
				}
			}

			// Reset state to idle
			g_loadStatus.state = LoadingState::Idle;
		}
		else if (currentState == LoadingState::Failed)
		{
			// Handle failed load
			std::string errorMessage;
			LoadFailureCallback failureCallback;

			// Get the error message and callback
			{
				std::lock_guard<std::mutex> errorLock(g_loadStatus.errorMutex);
				std::lock_guard<std::mutex> callbackLock(g_loadStatus.callbackMutex);

				errorMessage = g_loadStatus.errorMessage;
				failureCallback = g_loadStatus.failureCallback;
			}

			// Call the failure callback if it exists
			if (failureCallback)
			{
				failureCallback(errorMessage.c_str());
			}

			// Reset state to idle
			g_loadStatus.state = LoadingState::Idle;
		}
	}

} // namespace KZ::replaysystem::data
