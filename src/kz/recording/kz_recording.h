
#pragma once

#include "kz/kz.h"
#include "kz/replays/kz_replay.h"
#include "circularbuffer.h"

// two minute replay buffer that constantly records
//  used for replay breather and cheater replays.
struct CircularRecorder
{
	class CircularTickdata : public CFixedSizeCircularBuffer<Tickdata, 64 * 60 * 2>
	{
		virtual void ElementAlloc(Tickdata &element) {};
		virtual void ElementRelease(Tickdata &element) {};
	};

	CUtlVector<RpEvent> events;
	CircularTickdata tickdata;
};

class KZRecordingService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static void Init();
	void OnPhysicsSimulatePost();
	void OnPlayerJoinTeam(i32 team);
	void OnPostThinkPost();

	virtual void Reset() override;

	CircularRecorder circularRecording;
};
