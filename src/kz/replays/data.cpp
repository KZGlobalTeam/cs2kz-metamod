#include "cs2kz.h"
#include "data.h"
#include "filesystem.h"
#include "utils/utils.h"
#include "utils/uuid.h"
#include "compression.h"
#include <thread>
#include <mutex>
#include <atomic>

CConVar<bool> kz_replay_playback_debug("kz_replay_playback_debug", FCVAR_NONE, "Prints debug info about replay playback.", false);
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
		if (replay->weaponIndices)
		{
			delete[] replay->weaponIndices;
			replay->weaponIndices = nullptr;
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

	static_function void UpdateProgress(const char *cursor, const char *dataStart, size_t totalSize, std::atomic<f32> &progress)
	{
		if (totalSize > 0)
		{
			size_t bytesRead = static_cast<size_t>(cursor - dataStart);
			progress = static_cast<f32>(bytesRead) / static_cast<f32>(totalSize);
			if (kz_replay_playback_debug.Get())
			{
				META_CONPRINTF("Replay load progress: %zu bytes, %.2f%%\n", bytesRead, progress.load() * 100.0f);
			}
		}
	}

	// Parses replay data from an in-memory byte array.
	static_function ReplayPlayback LoadReplayFromMemory(const char *data, size_t size, UUID_t uuid, std::atomic<f32> &progress,
														std::atomic<bool> &shouldCancel)
	{
		ReplayPlayback result = {};
		progress = 0.0f;

		const char *cursor = data;
		const char *end = data + size;

		// Read length-prefixed protobuf header
		if (shouldCancel)
		{
			return result;
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading replay protobuf header...\n");
		}

		// Try to read header size (u32). If this fails, the data is invalid or corrupted.
		if (cursor + (ptrdiff_t)sizeof(u32) > end)
		{
			return result;
		}
		u32 headerSize = 0;
		memcpy(&headerSize, cursor, sizeof(headerSize));
		cursor += sizeof(headerSize);

		if (headerSize == 0 || headerSize > 5 * 1024 * 1024) // sanity limit 5MB
		{
			return result;
		}
		if (cursor + (ptrdiff_t)headerSize > end)
		{
			return result;
		}

		std::string serialized(cursor, cursor + headerSize);
		cursor += headerSize;

		if (!result.header.ParseFromString(serialized))
		{
			return result;
		}

		UpdateProgress(cursor, data, size, progress);

		if (result.header.version() != KZ_REPLAY_VERSION)
		{
			return result;
		}

		// Load tick data
		if (shouldCancel)
		{
			return result;
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading compressed tick data...\n");
		}

		std::vector<TickData> tickDataVec;
		std::vector<SubtickData> subtickDataVec;

		if (!KZ::replaysystem::compression::ReadTickDataCompressed(cursor, end, tickDataVec, subtickDataVec))
		{
			return result;
		}

		result.tickCount = tickDataVec.size();
		tickDataVec.shrink_to_fit();
		subtickDataVec.shrink_to_fit();
		result.tickData = tickDataVec.data();
		result.subtickData = subtickDataVec.data();
		new (&tickDataVec) std::vector<TickData>();
		new (&subtickDataVec) std::vector<SubtickData>();

		UpdateProgress(cursor, data, size, progress);

		// Load weapon data
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			return {};
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading weapons...\n");
		}

		std::vector<std::pair<i32, EconInfo>> weaponTableVec;

		if (!KZ::replaysystem::compression::ReadWeaponsCompressed(cursor, end, weaponTableVec))
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			return {};
		}

		result.weaponTableSize = weaponTableVec.size();
		assert(result.weaponTableSize > 0);
		result.weaponIndices = new i32[result.weaponTableSize];
		result.weapons = new EconInfo[result.weaponTableSize];
		for (size_t i = 0; i < weaponTableVec.size(); i++)
		{
			result.weaponIndices[i] = weaponTableVec[i].first;
			result.weapons[i] = weaponTableVec[i].second;
		}

		UpdateProgress(cursor, data, size, progress);

		// Load jump stats
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weaponIndices;
			delete[] result.weapons;
			return {};
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading compressed jump stats...\n");
		}

		std::vector<RpJumpStats> jumpsVec;

		if (!KZ::replaysystem::compression::ReadJumpsCompressed(cursor, end, jumpsVec))
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weaponIndices;
			delete[] result.weapons;
			return {};
		}

		result.numJumps = jumpsVec.size();
		if (result.numJumps > 0)
		{
			result.jumps = new RpJumpStats[result.numJumps];
			for (size_t i = 0; i < result.numJumps; i++)
			{
				result.jumps[i] = std::move(jumpsVec[i]);
			}
		}
		else
		{
			result.jumps = nullptr;
		}

		UpdateProgress(cursor, data, size, progress);

		// Load events
		if (shouldCancel)
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weaponIndices;
			delete[] result.weapons;
			delete[] result.jumps;
			return {};
		}
		if (kz_replay_playback_debug.Get())
		{
			META_CONPRINTF("Loading compressed events...\n");
		}

		std::vector<RpEvent> eventsVec;

		if (!KZ::replaysystem::compression::ReadEventsCompressed(cursor, end, eventsVec))
		{
			delete[] result.tickData;
			delete[] result.subtickData;
			delete[] result.weaponIndices;
			delete[] result.weapons;
			delete[] result.jumps;
			return {};
		}

		result.numEvents = eventsVec.size();
		eventsVec.shrink_to_fit();
		result.events = eventsVec.data();
		new (&eventsVec) std::vector<RpEvent>();

		UpdateProgress(cursor, data, size, progress);

		result.uuid = uuid;
		result.valid = true;
		progress = 1.0f;
		return result;
	}

	// File-based entry point: reads the entire file into memory then parses.
	static_function ReplayPlayback LoadReplayWithProgress(const char *path, std::atomic<f32> &progress, std::atomic<bool> &shouldCancel)
	{
		ReplayPlayback result = {};

		FileHandle_t file = g_pFullFileSystem->Open(path, "rb");
		if (!file)
		{
			return result;
		}

		size_t fileSize = g_pFullFileSystem->Size(file);
		std::vector<char> fileData(fileSize);
		if (g_pFullFileSystem->Read(fileData.data(), (int)fileSize, file) != (int)fileSize)
		{
			g_pFullFileSystem->Close(file);
			return result;
		}
		g_pFullFileSystem->Close(file);

		UUID_t uuid = {};
		if (!UUID_t::FromString(CUtlString(path).GetBaseFilename().StripExtension().Get(), &uuid))
		{
			return result;
		}

		return LoadReplayFromMemory(fileData.data(), fileSize, uuid, progress, shouldCancel);
	}

	// Memory-based entry point: parse directly from a provided buffer.
	static_function ReplayPlayback LoadReplayWithProgress(const char *data, size_t size, UUID_t uuid, std::atomic<f32> &progress,
														  std::atomic<bool> &shouldCancel)
	{
		return LoadReplayFromMemory(data, size, uuid, progress, shouldCancel);
	}

	static_function void PrepareAsyncLoad(LoadSuccessCallback onSuccess, LoadFailureCallback onFailure)
	{
		CancelAsyncLoad();
		g_loadStatus.state = LoadingState::Loading;
		g_loadStatus.progress = 0.0f;
		g_cancelLoad = false;
		{
			std::lock_guard<std::mutex> lock(g_loadStatus.callbackMutex);
			g_loadStatus.successCallback = onSuccess;
			g_loadStatus.failureCallback = onFailure;
		}
		{
			std::lock_guard<std::mutex> lock(g_loadStatus.errorMutex);
			g_loadStatus.errorMessage.clear();
		}
		{
			std::lock_guard<std::mutex> lock(g_loadStatus.replayMutex);
			FreeReplayData(&g_loadStatus.completedReplay);
			g_loadStatus.completedReplay = {};
		}
	}

	static_function void HandleAsyncResult(ReplayPlayback &result)
	{
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
	}

	void LoadReplayAsync(std::string path, LoadSuccessCallback onSuccess, LoadFailureCallback onFailure)
	{
		PrepareAsyncLoad(onSuccess, onFailure);
		g_loadThread = std::thread(
			[path = std::move(path)]()
			{
				ReplayPlayback result = LoadReplayWithProgress(path.c_str(), g_loadStatus.progress, g_cancelLoad);
				HandleAsyncResult(result);
			});
		g_loadThread.detach();
	}

	void LoadReplayMemoryAsync(std::vector<char> data, UUID_t uuid, LoadSuccessCallback onSuccess, LoadFailureCallback onFailure)
	{
		PrepareAsyncLoad(onSuccess, onFailure);
		g_loadThread = std::thread(
			[data = std::move(data), uuid]() mutable
			{
				ReplayPlayback result = LoadReplayWithProgress(data.data(), data.size(), uuid, g_loadStatus.progress, g_cancelLoad);
				HandleAsyncResult(result);
			});
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
