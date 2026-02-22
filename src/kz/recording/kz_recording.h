#pragma once

#include <optional>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <deque>
#include "kz/kz.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/replays/kz_replay.h"
#include "circularbuffer.h"
#include "utils/circularfifobuffer.h"
#include "utils/uuid.h"

class PlayerCommand;
class Jump;

// two minute replay buffer that constantly records
//  used for replay breather and cheater replays.
struct CircularRecorder
{
	// This is only written as long as the player is alive.
	CFIFOCircularBuffer<TickData, 64 * 60 * 2> *tickData;
	CFIFOCircularBuffer<SubtickData, 64 * 60 * 2> *subtickData;

	std::optional<RpModeStyleInfo> earliestMode;
	std::optional<std::vector<RpModeStyleInfo>> earliestStyles;
	// Extra 20 seconds for commands in case of network issues
	// Note that command data is tracked regardless of whether the player is alive or not.
	// This means if the player goes to spectator, the command data will no longer match the tick data.
	CFIFOCircularBuffer<CmdData, 64 * (60 * 2 + 20)> *cmdData;
	CFIFOCircularBuffer<SubtickData, 64 * (60 * 2 + 20)> *cmdSubtickData;
	CFIFOCircularBuffer<RpEvent, 64 * (60 * 2 + 20)> *rpEvents;
	// Using std::deque because RpJumpStats contains std::vector (non-trivially copyable)
	std::deque<RpJumpStats> jumps;

	CircularRecorder()
	{
		this->tickData = new CFIFOCircularBuffer<TickData, 64 * 60 * 2>();
		this->subtickData = new CFIFOCircularBuffer<SubtickData, 64 * 60 * 2>();
		this->cmdData = new CFIFOCircularBuffer<CmdData, 64 * (60 * 2 + 20)>();
		this->cmdSubtickData = new CFIFOCircularBuffer<SubtickData, 64 * (60 * 2 + 20)>();
		this->rpEvents = new CFIFOCircularBuffer<RpEvent, 64 * (60 * 2 + 20)>();
	}

	~CircularRecorder()
	{
		delete this->tickData;
		delete this->subtickData;
		delete this->cmdData;
		delete this->cmdSubtickData;
		delete this->rpEvents;
	}

	void TrimOldCommands(u32 currentTick);
	// Also updates the earliest mode and styles info.
	void TrimOldEvents(u32 currentTick);
	void TrimOldJumps(u32 currentTick);

	// Convenience method to trim all old data.
	void TrimOldData(u32 currentTick)
	{
		// Tick data and subtick data are automatically trimmed by the circular buffer.
		TrimOldCommands(currentTick);
		TrimOldEvents(currentTick);
		TrimOldJumps(currentTick);
	}
};

struct Recorder
{
	UUID_t uuid;
	f32 desiredStopTime = -1;
	ReplayHeader replayHeader; // Unified protobuf header
	std::vector<TickData> tickData;
	std::vector<SubtickData> subtickData;
	std::vector<RpEvent> rpEvents;

	// Empty until the replay is queued for writing.
	std::vector<std::pair<i32, EconInfo>> weaponTable;

	std::vector<RpJumpStats> jumps;
	std::vector<CmdData> cmdData;
	std::vector<SubtickData> cmdSubtickData;
	// Copy the last numSeconds seconds of data from the circular recorder.
	Recorder(KZPlayer *player, f32 numSeconds, ReplayType type, bool copyTimerEvents, DistanceTier copyJumps);

	bool ShouldStopAndSave(f32 currentTime)
	{
		return desiredStopTime >= 0 && currentTime >= desiredStopTime;
	}

	bool WriteToFile();
	virtual i32 WriteHeader(FileHandle_t file);
	virtual i32 WriteTickData(FileHandle_t file);
	virtual i32 WriteWeapons(FileHandle_t file);
	virtual i32 WriteJumps(FileHandle_t file);
	virtual i32 WriteEvents(FileHandle_t file);
	virtual i32 WriteCmdData(FileHandle_t file);

	template<typename T>
	void PushData(const T &data)
	{
		if constexpr (std::is_same<T, TickData>::value)
		{
			tickData.push_back(data);
		}
		else if constexpr (std::is_same<T, RpEvent>::value)
		{
			rpEvents.push_back(data);
		}
		else if constexpr (std::is_same<T, RpJumpStats>::value)
		{
			jumps.push_back(data);
		}
		else if constexpr (std::is_same<T, CmdData>::value)
		{
			cmdData.push_back(data);
		}
		else
		{
			static_assert(std::is_same_v<T, void>, "Unsupported data type for PushData");
		}
	}

	// Special case for subtick structs
	enum class Vec
	{
		Tick,
		Cmd
	};

	template<Vec V>
	void PushData(const SubtickData &data)
	{
		if constexpr (V == Vec::Tick)
		{
			subtickData.push_back(data);
		}
		else
		{
			cmdSubtickData.push_back(data);
		}
	}

	static void Init(ReplayHeader &hdr, KZPlayer *player, ReplayType type);
};

// Forward declarations
class KZRecordingService;
class KZPlayer;

// Callback types for write completion
using WriteSuccessCallback = std::function<void(const UUID_t &uuid, f32 duration)>;
using WriteFailureCallback = std::function<void(const char *error)>;

// Write task with optional callbacks
struct WriteTask
{
	std::unique_ptr<Recorder> recorder;
	WriteSuccessCallback onSuccess;
	WriteFailureCallback onFailure;
};

// Completed write result
struct WriteResult
{
	bool success;
	UUID_t uuid;
	f32 duration;
	std::string errorMessage;
	WriteSuccessCallback onSuccess;
	WriteFailureCallback onFailure;
};

// Thread-safe file writer for async replay file writing
class ReplayFileWriter
{
public:
	ReplayFileWriter();
	~ReplayFileWriter();

	void Start();
	void Stop();

	// Queue a recorder for async file writing (fire-and-forget)
	void QueueWrite(std::unique_ptr<Recorder> recorder);

	// Queue a recorder with callbacks
	void QueueWrite(std::unique_ptr<Recorder> recorder, WriteSuccessCallback onSuccess, WriteFailureCallback onFailure);

	// Run frame - process any completed writes and invoke callbacks on main thread
	void RunFrame();

private:
	void ThreadRun();

	std::unique_ptr<std::thread> m_thread;
	std::queue<WriteTask> m_writeQueue;
	std::queue<WriteResult> m_completedWrites;
	std::mutex m_queueLock;
	std::mutex m_completedLock;
	std::condition_variable m_queueCV;
	bool m_terminate = false;
};

struct RunRecorder : public Recorder
{
	RunRecorder(KZPlayer *player);
	void End(f32 time, i32 numTeleports);
	virtual i32 WriteHeader(FileHandle_t file) override;
};

struct JumpRecorder : public Recorder
{
	JumpRecorder(Jump *jump);
	virtual i32 WriteHeader(FileHandle_t file) override;
};

struct CheaterRecorder : public Recorder
{
	CheaterRecorder(KZPlayer *player, const char *reason, KZPlayer *savedBy);
	virtual i32 WriteHeader(FileHandle_t file) override;
};

struct ManualRecorder : public Recorder
{
	ManualRecorder(KZPlayer *player, f32 duration, KZPlayer *savedBy);
	virtual i32 WriteHeader(FileHandle_t file) override;
};

class KZRecordingService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	~KZRecordingService()
	{
		if (circularRecording)
		{
			delete circularRecording;
		}
		circularRecording = nullptr;
	}

	static void Init();
	static void Shutdown();
	static void OnActivateServer();
	static void ProcessFileWriteCompletion();
	void OnProcessUsercmds(PlayerCommand *base, i32 numCmds);

	void OnPhysicsSimulate();
	void OnSetupMove(PlayerCommand *pc);
	void OnPhysicsSimulatePost();

	virtual void Reset() override;

	void OnTimerStart();
	void OnTimerStop();
	void OnTimerEnd();
	void OnPause();
	void OnResume();
	void OnSplit(i32 split);
	void OnCPZ(i32 cpz);
	void OnStage(i32 stage);
	void OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity);

	void OnJumpFinish(Jump *jump);

	void OnClientDisconnect();

	void RecordTickData_PhysicsSimulate();
	void RecordTickData_SetupMove(PlayerCommand *pc);
	void RecordTickData_PhysicsSimulatePost();

	// Record ALL commands as received by the server.
	void RecordCommand(PlayerCommand *cmds, i32 numCmds);

	// Check all active recorders and stop those that have reached their desired stop time.
	void CheckRecorders();

	// Check the player's currently held weapons and record weapon switch events. Also tracks the earliest weapon for circular recorder.
	void CheckWeapons();

	// Check the player's current mode and styles and record mode/style change events. Also tracks the earliest mode/styles for circular recorder.
	void CheckModeStyles();

	// Check the player's checkpoints/teleports and embed in tick data.
	void CheckCheckpoints();

	// Ensure circular recorder is initialized (lazy initialization)
	void EnsureCircularRecorderInitialized();

private:
	// Insert a replay event into the circular buffer and all active recorders.
	void InsertEvent(const RpEvent &event);

	void InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TimerEventType type, f32 time, i32 index = -1);
	void InsertTeleportEvent(const Vector *origin, const QAngle *angles, const Vector *velocity);
	void InsertModeChangeEvent(const char *name, const char *md5);
	void InsertStyleChangeEvent(const char *name, const char *md5, bool firstStyle);

public:
	// Write a replay file with completion callbacks
	void WriteCircularBufferToFileAsync(f32 duration, const char *cheaterReason, KZPlayer *saver, WriteSuccessCallback onSuccess,
										WriteFailureCallback onFailure);

public:
	SubtickData currentSubtickData;
	TickData currentTickData;
	CircularRecorder *circularRecording = nullptr;
	i32 lastCmdNumReceived = 0;
	KZModeManager::ModePluginInfo lastKnownMode;
	std::vector<KZStyleManager::StylePluginInfo> lastKnownStyles;
	i32 currentWeaponID = -1;
	std::vector<EconInfo> weapons;

	// Recorders
	std::vector<RunRecorder> runRecorders;
	std::vector<JumpRecorder> jumpRecorders;

	// Generated at timer end
	UUID_t currentRunUUID;

	const UUID_t &GetCurrentRunUUID() const
	{
		return currentRunUUID;
	}

	UUID_t lastJumpUUID;

	UUID_t GetLastJumpUUID() const
	{
		return lastJumpUUID;
	}

	enum class RecorderType
	{
		Run,
		Jump,
		Both
	};

	// Helper function to copy weapons from recording service to recorder before queuing
	void CopyWeaponsToRecorder(Recorder *recorder);

private:
	template<typename Func>
	void ApplyToTarget(Func &&func, RecorderType target)
	{
		if (target == RecorderType::Run || target == RecorderType::Both)
		{
			for (auto &recorder : runRecorders)
			{
				func(recorder);
			}
		}
		if (target == RecorderType::Jump || target == RecorderType::Both)
		{
			for (auto &recorder : jumpRecorders)
			{
				func(recorder);
			}
		}
	}

public:
	template<Recorder::Vec V>
	void PushToRecorders(const SubtickData &value, RecorderType target = RecorderType::Both)
	{
		ApplyToTarget([&](auto &r) { r.template PushData<V>(value); }, target);
	}

	template<typename T>
	void PushToRecorders(const T &value, RecorderType target = RecorderType::Both)
	{
		ApplyToTarget([&](auto &r) { r.PushData(value); }, target);
	}

	static ReplayFileWriter *fileWriter;
	static inline std::vector<UUID_t> globalUploadQueue;
};
