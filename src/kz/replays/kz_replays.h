#pragma once
#include "../kz.h"

struct Frame
{
	f64 time;
	Vector position;
	Vector velocity;
	QAngle orientation;
	MoveType_t moveType;
	u32 flags;
	uint64 buttons;
	/* Quack */
	f32 duckAmount;
};

class KZReplayService : public KZBaseService
{
	using KZBaseService::KZBaseService;
private:
	bool replayDone = false;
	int lastIndex = 0;

	void Play();
	void AddFrame();
	void InitializeReplayBot();
	static_function KZPlayer *GetReplayBot(const char *playerName);

public:
	KZReplayService(KZPlayer *player) : KZBaseService(player)
	{
		this->frames = CUtlVector<Frame>(1, 0);
	}

	bool isReplayBot = false;
	f64 time = 0;
	CUtlVector<Frame> frames;

	virtual void Reset() override;
	static void Init();
	static void RegisterCommands();

	void StartReplay();
	void Skip(int seconds);
	void Skip(const char *secondsStr);

	void OnPhysicsSimulatePost();
	void OnProcessMovementPost();
	void OnTimerStop();
	void OnTimerStart();
};
