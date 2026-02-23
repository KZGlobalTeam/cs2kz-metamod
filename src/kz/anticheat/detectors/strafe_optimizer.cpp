#include "kz/anticheat/kz_anticheat.h"
#include "sdk/usercmd.h"
#include <cmath>

float KZAnticheatService::CalculateYawSpeed(size_t index)
{
	if (index == 0)
	{
		return 0.0f;
	}

	float currentYaw = angleFrameHistory[index].angle.y;
	float lastYaw = angleFrameHistory[index - 1].angle.y;
	float frameTime = angleFrameHistory[index].frametime;

	float yawDiff = currentYaw - lastYaw;
	if (yawDiff > 180.0f)
	{
		yawDiff -= 360.0f;
	}
	if (yawDiff < -180.0f)
	{
		yawDiff += 360.0f;
	}

	return yawDiff / frameTime;
}

float KZAnticheatService::CalculateYawAccel(size_t index)
{
	if (index < 2)
	{
		return 0.0f;
	}

	float currentSpeed = CalculateYawSpeed(index);
	float lastSpeed = CalculateYawSpeed(index - 1);
	float frameTime = angleFrameHistory[index].frametime;

	return (currentSpeed - lastSpeed) / frameTime;
}

void KZAnticheatService::DetectOptimization(PlayerCommand *pc)
{
	// get yaw delta from subtick moves
	float totalYawDelta = 0.0f;
	const CBaseUserCmdPB &baseCmd = pc->base();
	for (int i = 0; i < baseCmd.subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = baseCmd.subtick_moves(i);
		if (step.has_yaw_delta())
		{
			totalYawDelta += step.yaw_delta();
		}
	}

	QAngle angles;
	angles.x = baseCmd.viewangles().x();
	angles.y = baseCmd.viewangles().y();
	angles.z = baseCmd.viewangles().z();

	// Store frame data
	AngleFrame frame;
	frame.angle = angles;
	frame.frametime = g_pKZUtils->GetGlobals()->frametime;
	frame.yawDelta = totalYawDelta;
	angleFrameHistory.push_back(frame);

	size_t historySize = angleFrameHistory.size();

	// check if there are enough angle frames to calculate yaw speed
	if (historySize > 2)
	{
		float yawSpeed2TicksAgo = CalculateYawSpeed(historySize - 3);
		float lastYawSpeed = CalculateYawSpeed(historySize - 2);
		float currentYawSpeed = CalculateYawSpeed(historySize - 1);
		bool switchedStrafeDirection = std::signbit(currentYawSpeed) != std::signbit(lastYawSpeed);

		// check if there are enough angle frames to calculate yaw accel
		if (historySize > 3)
		{
			float yawAccel2TicksAgo = std::abs(CalculateYawAccel(historySize - 3));
			float lastYawAccel = std::abs(CalculateYawAccel(historySize - 2));
			float currentYawAccel = std::abs(CalculateYawAccel(historySize - 1));
			float lastNextDiff = std::abs(currentYawAccel - yawAccel2TicksAgo);

			if (lastNextDiff < 1.0f)
			{
				float avgAccel = (currentYawAccel + yawAccel2TicksAgo) * 0.5f;
				if (avgAccel < 2.0f && (lastYawAccel - avgAccel) > 2.0f && switchedStrafeDirection)
				{
					// increase rolling average for an unusual yaw accel spike
					yawAccelPercent = yawAccelPercent * 0.95f + 1.0f * 0.05f;
				}
				else if (switchedStrafeDirection)
				{
					// decrease rolling average
					yawAccelPercent = yawAccelPercent * 0.95f + 0.0f * 0.05f;
				}
			}
		}
	}

	// finally check for suspicious yaw accel patterns
	if (yawAccelPercent > 0.9f)
	{
		META_CONPRINTF("Strafe optimizer detected for player %s (%llu)\n", player->GetName(), player->GetSteamId64(false));
		this->MarkInfraction(KZAnticheatService::Infraction::Type::StrafeHack, "Strafe optimizer detected");
		return;
	}

	// clear angle frames after reaching 200
	if (angleFrameHistory.size() > 200)
	{
		angleFrameHistory.erase(angleFrameHistory.begin());
	}
}
