#pragma once
#include "../kz.h"

struct Frame
{
	f64 tick;
	Vector position;
	Vector velocity;
	QAngle orientation;
	MoveType_t moveType;
	/* Quack */
	f32 duckAmount;
	u32 flags;
};

class KZReplayService : public KZBaseService
{
	using KZBaseService::KZBaseService;
private:
	bool replay = false;
	bool replayDone = false;
	f64 time = 0;
	int lastIndex = 0;

	void Play();
	static_function KZPlayer *GetReplayBot(const char *playerName);

public:
	CUtlVector<Frame> frames;

	virtual void Reset() override;
	static void Init();
	static void RegisterCommands();

	void StartReplay(CUtlVector<Frame> &frames);
	void Skip(int seconds);

	void OnPhysicsSimulatePost();
	void OnTimerStop();
	void OnTimerStart();
};
