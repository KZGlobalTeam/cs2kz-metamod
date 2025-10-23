#ifndef KZ_REPLAYDATA_H
#define KZ_REPLAYDATA_H

#include "sdk/datatypes.h"
#include "kz_replay.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace KZ::replaysystem::data
{
	// Replay data structure
	struct ReplayPlayback
	{
		bool valid;
		GeneralReplayHeader header;
		// Can't union this because the type will become non trivial.
		CheaterReplayHeader cheaterHeader;
		RunReplayHeader runHeader;
		JumpReplayHeader jumpHeader;

		u32 tickCount;
		TickData *tickData;
		SubtickData *subtickData;
		i32 currentWeapon = -1;
		i32 numWeapons;
		WeaponSwitchEvent *weapons;
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
		i32 courseID;
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
	void FreeReplayData(ReplayPlayback *replay);
	void ResetReplayState(ReplayPlayback *replay);

	// Async loading status
	AsyncLoadStatus *GetLoadStatus();
	bool IsLoading();
	void CancelAsyncLoad();
	void ProcessAsyncLoadCompletion(); // Call this from main thread	// State queries
	ReplayPlayback *GetCurrentReplay();
	bool IsReplayValid();
	bool IsReplayPlaying();

	// Navigation support
	void SetCurrentTick(u32 tick);
	u32 GetCurrentTick();
	u32 GetTickCount();

	// Timer state accessors
	i32 GetCurrentCpIndex();
	i32 GetCheckpointCount();
	i32 GetTeleportCount();
	f32 GetReplayTime();
	f32 GetEndTime();
	bool GetPaused();
} // namespace KZ::replaysystem::data

#endif // KZ_REPLAYDATA_H
