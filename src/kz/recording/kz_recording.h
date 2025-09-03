
#pragma once

#include "kz/kz.h"
#include "kz/replays/kz_replay.h"
#include "circularbuffer.h"
#include "utils/uuid.h"

struct PlayerCommand;

// two minute replay buffer that constantly records
//  used for replay breather and cheater replays.
struct CircularRecorder
{
	class CircularTickdata : public CFixedSizeCircularBuffer<TickData, 64 * 60 * 2>
	{
		virtual void ElementAlloc(TickData &element) {};
		virtual void ElementRelease(TickData &element) {};
	};

	class CircularCmdData : public CFixedSizeCircularBuffer<CmdData, 64 * 60 * 2>
	{
		virtual void ElementAlloc(CmdData &element) {};
		virtual void ElementRelease(CmdData &element) {};
	};

	class CircularSubtickData : public CFixedSizeCircularBuffer<SubtickData, 64 * 60 * 2>
	{
		virtual void ElementAlloc(SubtickData &element) {};
		virtual void ElementRelease(SubtickData &element) {};
	};

	CUtlVector<RpEvent> events;
	CircularTickdata tickData;
	CircularSubtickData subtickData;

	// TODO
	CircularCmdData cmdData;
	CircularSubtickData cmdSubtickData;
};

struct Recorder
{
	Recorder()
	{
		uuid.Init();
	}

	UUID_t uuid;
	f32 desiredStopTime = -1;
	std::vector<RpEvent> events;
	std::vector<TickData> tickData;
	std::vector<SubtickData> subtickData;

	// TODO
	std::vector<CmdData> cmdData;
	std::vector<SubtickData> cmdSubtickData;
};

class KZRecordingService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static void Init();
	void OnPhysicsSimulate();
	void OnSetupMove(PlayerCommand *pc);
	void OnPhysicsSimulatePost();
	void OnPlayerJoinTeam(i32 team);
	void OnPostThinkPost();

	virtual void Reset() override;
	// TODO
	void OnTimerStart();
	void OnTimerStop();
	void OnTimerEnd();

	SubtickData currentSubtickData;
	TickData currentTickData;
	UUID_t currentRecorder;
	std::vector<Recorder> recorders;
	CircularRecorder circularRecording;
};
