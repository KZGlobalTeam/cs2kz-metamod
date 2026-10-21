#pragma once

#include "sdk/datatypes.h"
#include "kz_replay.h"
#include "utils/uuid.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace KZ::replaysystem::data
{
	// Replay data structure
	struct ReplayPlayback
	{
		UUID_t uuid;
		bool valid;
		ReplayHeader header;

		u32 tickCount;
		TickData *tickData;
		// tickCount + 1 entries; the subtick moves of tick i are subtickMoves[subtickOffsets[i]] to subtickMoves[subtickOffsets[i + 1] - 1].
		u64 *subtickOffsets;
		SubtickData::RpSubtickMove *subtickMoves;
		i32 weaponTableSize;
		i32 *weaponIndices;
		EconInfo *weapons;
		u32 currentJump;
		u32 numJumps;
		RpJumpStats *jumps;
		u32 currentEvent;
		u32 numEvents;
		RpEvent *events;

		// Checkpoint stuff
		i32 currentCpIndex;
		i32 currentCheckpoint;
		i32 currentTeleport;

		// Timer stuff
		char courseName[256];
		f32 startTime;
		bool paused;
		f32 endTime;
		u32 stopTick;
		f32 lastSplitTime;
		f32 lastCPZTime;
		f32 lastStageTime;

		// Playback state
		u32 currentTick;
		bool playingReplay;
		bool replayPaused;
		f32 timescale;
		// How far the bot is from currentTick's pre state to its post state, in replay ticks.
		f32 tickProgress;
		// Replay ticks left to simulate this server tick.
		u32 pendingTicks;
		// Replay ticks to play on the next server tick even while paused.
		u32 stepTicks;
		// Set on server ticks that simulate nothing and only move the bot between two replay ticks.
		bool holdingTick;
		bool hasLastMoveData;
		// Raw bytes, the engine's CUtlVectors inside must not be copied or freed by ReplayPlayback.
		alignas(CMoveData) u8 lastMoveData[sizeof(CMoveData)];
	};

	// Callback function types for async loading
	using LoadSuccessCallback = std::function<void()>;
	using LoadFailureCallback = std::function<void(const char *error)>;

	// Loading state enum
	enum class LoadingState
	{
		Idle,
		Loading,
		Completed,
		Failed
	};

	// Async loading status
	struct AsyncLoadStatus
	{
		std::atomic<LoadingState> state {LoadingState::Idle};
		std::atomic<float> progress {0.0f};
		std::string errorMessage;
		std::mutex errorMutex;
		ReplayPlayback completedReplay {};
		std::mutex replayMutex;
		LoadSuccessCallback successCallback;
		LoadFailureCallback failureCallback;
		std::mutex callbackMutex;
	};

	// Data management functions
	void LoadReplayAsync(std::string path, LoadSuccessCallback onSuccess, LoadFailureCallback onFailure);
	void LoadReplayMemoryAsync(std::vector<char> data, UUID_t uuid, LoadSuccessCallback onSuccess, LoadFailureCallback onFailure);
	void FreeReplayData(ReplayPlayback *replay);
	void ResetReplayState(ReplayPlayback *replay);
	// The first replay tick recorded on or after serverTick, or the last tick if there is none.
	u32 GetTickAtServerTick(const ReplayPlayback *replay, u32 serverTick);

	// Async loading status
	AsyncLoadStatus *GetLoadStatus();
	bool IsLoading();
	void CancelAsyncLoad();

	// Stops the loader thread and frees every replay buffer this module owns. Plugin unload only.
	void Shutdown();
	void ProcessAsyncLoadCompletion(); // Call this from main thread	// State queries
	ReplayPlayback *GetCurrentReplay();
	bool IsReplayValid();
	bool IsReplayPlaying();

	// Timer state accessors
	i32 GetCurrentCpIndex();
	i32 GetCheckpointCount();
	i32 GetTeleportCount();
	f32 GetReplayTime();
	// The bot's clock, which moves one tick per simulated replay tick, not with the server.
	i32 GetBotTickcount();
	void SetBotTickcount(i32 tick);
	f32 GetBotCurtime();
	f32 GetEndTime();
	bool GetPaused();
	const char *GetCourseName();
} // namespace KZ::replaysystem::data
