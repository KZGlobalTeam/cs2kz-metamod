#include "cs2kz.h"
#include "kz_replaydata.h"
#include "filesystem.h"
#include "utils/utils.h"
#include "utils/uuid.h"
#include "kz_replaycompression.h"
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
		if (replay->weaponTable)
		{
			delete[] replay->weaponTable;
			replay->weaponTable = nullptr;
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

	static_function void UpdateProgress(FileHandle_t file, size_t fileSize, std::atomic<f32> &progress)
	{
		if (fileSize > 0)
		{
			size_t bytesRead = g_pFullFileSystem->Tell(file);
			progress = static_cast<f32>(bytesRead) / static_cast<f32>(fileSize);
			if (kz_replay_playback_debug.Get())
			{
				META_CONPRINTF("Replay load progress: %zu bytes, %.2f%%\n", bytesRead, progress.load() * 100.0f);
			}
		}
	}

	// Helper function to load replay data with progress reporting
	static_function ReplayPlayback LoadReplayWithProgress(const char *path, std::atomic<f32> &progress, std::atomic<bool> &shouldCancel)
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
		switch (result.header.type)
		{
			case RP_CHEATER:
			{
				g_pFullFileSystem->Read(&result.cheaterHeader, sizeof(result.cheaterHeader), file);
				break;
			}
			case RP_RUN:
			{
				g_pFullFileSystem->Read(&result.runHeader, sizeof(result.runHeader), file);
				// Just to advance the reader.
				for (i32 i = 0; i < result.runHeader.styleCount; i++)
				{
					RpModeStyleInfo style = {};
					g_pFullFileSystem->Read(&style, sizeof(style), file);
				}
				break;
			}
			case RP_JUMPSTATS:
			{
				g_pFullFileSystem->Read(&result.jumpHeader, sizeof(result.jumpHeader), file);
				break;
			}
			case RP_MANUAL:
			{
				g_pFullFileSystem->Read(&result.manualHeader, sizeof(result.manualHeader), file);
				break;
			}
		}
		UpdateProgress(file, fileSize, progress);

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
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading compressed tick data...\n");
		}

		std::vector<TickData> tickDataVec;
		std::vector<SubtickData> subtickDataVec;

		if (!KZ::replaysystem::compression::ReadTickDataCompressed(file, tickDataVec, subtickDataVec))
		{
			g_pFullFileSystem->Close(file);
			return result;
		}

		result.tickCount = tickDataVec.size();

		// Shrink to fit to ensure contiguous allocation
		tickDataVec.shrink_to_fit();
		subtickDataVec.shrink_to_fit();

		// Transfer ownership - extract pointer and prevent deallocation
		result.tickData = tickDataVec.data();
		result.subtickData = subtickDataVec.data();

		// Null out the vector's internal pointers so it doesn't free the memory
		new (&tickDataVec) std::vector<TickData>();
		new (&subtickDataVec) std::vector<SubtickData>();

		UpdateProgress(file, fileSize, progress);

		// Load weapon data
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			g_pFullFileSystem->Close(file);
			return {};
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading compressed weapon change events...\n");
		}

		std::vector<WeaponSwitchEvent> weaponEventsVec;
		std::vector<EconInfo> weaponTableVec;

		if (!KZ::replaysystem::compression::ReadWeaponChangesCompressed(file, weaponEventsVec, weaponTableVec))
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			g_pFullFileSystem->Close(file);
			return {};
		}

		// Store weapon table
		result.weaponTableSize = static_cast<u32>(weaponTableVec.size());
		weaponTableVec.shrink_to_fit();
		result.weaponTable = weaponTableVec.data();
		new (&weaponTableVec) std::vector<EconInfo>();

		// Store weapon events, adding initial weapon at index 0
		result.numWeapons = static_cast<i32>(weaponEventsVec.size());
		result.weapons = new WeaponSwitchEvent[result.numWeapons + 1];

		// Find the weapon index for firstWeapon in the weapon table
		u16 firstWeaponIndex = 0;
		for (u32 i = 0; i < result.weaponTableSize; i++)
		{
			if (result.weaponTable[i] == result.header.firstWeapon)
			{
				firstWeaponIndex = i;
				break;
			}
		}
		result.weapons[0] = {0, firstWeaponIndex};

		// Copy the rest of the weapon events
		for (i32 i = 0; i < result.numWeapons; i++)
		{
			result.weapons[i + 1] = weaponEventsVec[i];
		}

		UpdateProgress(file, fileSize, progress);

		// Load jump stats
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weaponTable;
			delete[] result.weapons;
			g_pFullFileSystem->Close(file);
			return {};
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading compressed jump stats...\n");
		}

		std::vector<RpJumpStats> jumpsVec;

		if (!KZ::replaysystem::compression::ReadJumpsCompressed(file, jumpsVec))
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weaponTable;
			delete[] result.weapons;
			g_pFullFileSystem->Close(file);
			return {};
		}

		result.numJumps = jumpsVec.size();

		// Shrink and extract
		jumpsVec.shrink_to_fit();
		result.jumps = jumpsVec.data();
		new (&jumpsVec) std::vector<RpJumpStats>();

		UpdateProgress(file, fileSize, progress);

		// Load events
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weaponTable;
			delete[] result.weapons;
			delete[] result.jumps;
			g_pFullFileSystem->Close(file);
			return {};
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading compressed events...\n");
		}

		std::vector<RpEvent> eventsVec;

		if (!KZ::replaysystem::compression::ReadEventsCompressed(file, eventsVec))
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weaponTable;
			delete[] result.weapons;
			delete[] result.jumps;
			g_pFullFileSystem->Close(file);
			return {};
		}

		result.numEvents = eventsVec.size();

		// Shrink and extract
		eventsVec.shrink_to_fit();
		result.events = eventsVec.data();
		new (&eventsVec) std::vector<RpEvent>();

		UpdateProgress(file, fileSize, progress);

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
