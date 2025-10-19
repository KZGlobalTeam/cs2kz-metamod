
#pragma once

#include <optional>
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
	CFIFOCircularBuffer<WeaponSwitchEvent, 64 * 60 * 2> *weaponChangeEvents;

	std::optional<EconInfo> earliestWeapon;
	std::optional<RpModeStyleInfo> earliestMode;
	std::optional<std::vector<RpModeStyleInfo>> earliestStyles;
	std::optional<i32> earliestCheckpointIndex;
	std::optional<i32> earliestCheckpointCount;
	std::optional<i32> earliestTeleportCount;
	// Extra 20 seconds for commands in case of network issues
	// Note that command data is tracked regardless of whether the player is alive or not.
	// This means if the player goes to spectator, the command data will no longer match the tick data.
	CFIFOCircularBuffer<CmdData, 64 * (60 * 2 + 20)> *cmdData;
	CFIFOCircularBuffer<SubtickData, 64 * (60 * 2 + 20)> *cmdSubtickData;
	CFIFOCircularBuffer<RpEvent, 64 * (60 * 2 + 20)> *rpEvents;
	// Players can only do so many jumps in 2 minutes, let's just say... 2 jumps per seconds max.
	// This should probably never be resized.
	// TODO: there's std::vector inside RpJumpStats, does this kill performance?
	CFIFOCircularBuffer<RpJumpStats, 60 * 2 * 2> *jumps;

	CircularRecorder()
	{
		this->tickData = new CFIFOCircularBuffer<TickData, 64 * 60 * 2>();
		this->subtickData = new CFIFOCircularBuffer<SubtickData, 64 * 60 * 2>();
		this->weaponChangeEvents = new CFIFOCircularBuffer<WeaponSwitchEvent, 64 * 60 * 2>();
		this->cmdData = new CFIFOCircularBuffer<CmdData, 64 * (60 * 2 + 20)>();
		this->cmdSubtickData = new CFIFOCircularBuffer<SubtickData, 64 * (60 * 2 + 20)>();
		this->rpEvents = new CFIFOCircularBuffer<RpEvent, 64 * (60 * 2 + 20)>();
		this->jumps = new CFIFOCircularBuffer<RpJumpStats, 60 * 2 * 2>();
	}

	~CircularRecorder()
	{
		delete this->tickData;
		delete this->subtickData;
		delete this->weaponChangeEvents;
		delete this->cmdData;
		delete this->cmdSubtickData;
		delete this->rpEvents;
		delete this->jumps;
	}
};

struct Recorder
{
	UUID_t uuid;
	f32 desiredStopTime = -1;
	ReplayHeader baseHeader;
	std::vector<TickData> tickData;
	std::vector<SubtickData> subtickData;
	std::vector<RpEvent> rpEvents;
	std::vector<WeaponSwitchEvent> weaponChangeEvents;
	std::vector<RpJumpStats> jumps;
	std::vector<CmdData> cmdData;
	std::vector<SubtickData> cmdSubtickData;

	// Copy the last numSeconds seconds of data from the circular recorder.
	Recorder(KZPlayer *player, f32 numSeconds, bool copyTimerEvents, DistanceTier copyJumps);

	void WriteToFile();
	virtual i32 WriteHeader(FileHandle_t file);
	virtual i32 WriteTickData(FileHandle_t file);
	virtual i32 WriteWeaponChanges(FileHandle_t file);
	virtual i32 WriteJumps(FileHandle_t file);
	virtual i32 WriteEvents(FileHandle_t file);
	virtual i32 WriteCmdData(FileHandle_t file);
};

constexpr int i = sizeof(ReplayHeader) + sizeof(ReplayRunHeader);

struct RunRecorder : public Recorder
{
	ReplayRunHeader header;
	RunRecorder(KZPlayer *player);
	void End(f32 time, i32 numTeleports);
	virtual int WriteHeader(FileHandle_t file) override;
};

struct JumpRecorder : public Recorder
{
	ReplayJumpHeader header;

	JumpRecorder(Jump *jump);
	virtual int WriteHeader(FileHandle_t file) override;
};

class KZRecordingService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static void Init();
	void OnProcessUsercmds(PlayerCommand *base, int numCmds);

	void OnPhysicsSimulate();
	void OnSetupMove(PlayerCommand *pc);
	void OnPhysicsSimulatePost();
	void OnPlayerJoinTeam(i32 team);

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

private:
	void InsertEvent(const RpEvent &event);

public:
	SubtickData currentSubtickData;
	TickData currentTickData;
	std::vector<RunRecorder> runRecorders;
	bool GetCurrentRunUUID(UUID_t &out_uuid);
	std::vector<JumpRecorder> jumpRecorders;
	CircularRecorder circularRecording;
	i32 lastCmdNumReceived = 0;
	i32 lastKnownCheckpointIndex = 0;
	i32 lastKnownCheckpointCount = 0;
	i32 lastKnownTeleportCount = 0;
	KZModeManager::ModePluginInfo lastKnownMode;
	std::vector<KZStyleManager::StylePluginInfo> lastKnownStyles;
	EconInfo currentWeaponEconInfo;
	f32 lastSensitivity = 1.25f;
	f32 lastYaw = 0.022f;
	f32 lastPitch = 0.022f;
};
