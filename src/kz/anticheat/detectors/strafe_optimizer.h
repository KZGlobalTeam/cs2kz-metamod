#pragma once
#include "kz/kz.h"

struct AngleFrame
{
	QAngle angle;
	float frametime;
	float yawDelta;
};

class StrafeOptimizerDetector
{
private:
	bool yawSpikeFlag = false;
	f32 yawAccelPercent = 0;
	std::vector<AngleFrame> angleFrameHistory;

	float CalculateYawSpeed(size_t index);
	float CalculateYawAccel(size_t index);

public:
	void DetectOptimization(KZPlayer *player, PlayerCommand *pc);

	bool IsFlagged() const
	{
		return yawSpikeFlag;
	}
};
