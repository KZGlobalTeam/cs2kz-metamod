#pragma once

#include "kz/kz.h"

class KZTelemetryService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

private:
	static f64 lastActiveCheckTime;

	struct ActiveStats
	{
		f32 lastActionTime {};
		f64 activeTime {};
		f64 afkDuration {};
		f64 timeSpentInServer {};
	} activeStats;

	struct ModernBhopStats
	{
		u32 numTruePerfs {};
		u32 numModernPerfsEarly {};
		u32 numModernPerfsLate {};
		u32 numNonPerfs {};

		u32 GetTotalJumps() const
		{
			return numTruePerfs + numModernPerfsEarly + numModernPerfsLate + numNonPerfs;
		}

	} modernBhopStats;

	struct LegacyBhopStats
	{
		u32 numPerfs {};
		u32 numNonPerfs {};

		u32 GetTotalJumps() const
		{
			return numPerfs + numNonPerfs;
		}

		f32 GetPerfRatio() const
		{
			u32 totalJumps = GetTotalJumps();
			if (totalJumps == 0)
			{
				return 0.0f;
			}
			return (f32)(numPerfs) / (f32)totalJumps;
		}
	} legacyBhopStats;

public:
	virtual void Reset() override
	{
		this->activeStats = {};
		this->modernBhopStats = {};
		this->legacyBhopStats = {};
	}

	static void ActiveCheck();

	f64 GetActiveTime() const
	{
		return activeStats.activeTime;
	}

	f64 GetAFKTime() const
	{
		return activeStats.afkDuration;
	}

	f64 GetSpectatingTime() const
	{
		return activeStats.timeSpentInServer - activeStats.activeTime - activeStats.afkDuration;
	}

	f64 GetTimeInServer() const
	{
		return activeStats.timeSpentInServer;
	}

	void OnPhysicsSimulatePost();

	Vector preOutWishVel;
	void OnJumpModern();
	void OnJumpModernPost();
	void PrintModernBhopStats();

	void ResetModernBhopStats()
	{
		this->modernBhopStats = {};
	}

	void OnJumpLegacy();
	void OnJumpLegacyPost();
	void PrintLegacyBhopStats();

	void ResetLegacyBhopStats()
	{
		this->legacyBhopStats = {};
	}

	bool mightBeModernPerf {};
};
