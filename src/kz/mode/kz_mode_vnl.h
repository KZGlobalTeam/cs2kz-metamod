#pragma once
#include "kz_mode.h"

class KZVanillaModeService : public KZModeService
{
	using KZModeService::KZModeService;
public:
	virtual const char *GetModeName() override;
	virtual const char *GetModeShortName() override;
	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) override;
	f32 distanceTiers[JUMPTYPE_COUNT - 3][DISTANCETIER_COUNT] =
	{
		{215.0f, 230.0f, 235.0f, 240.0f, 244.0f, 246.0f}, // LJ
		{150.0f, 230.0f, 233.0f, 236.0f, 238.0f, 240.0f}, // BH
		{150.0f, 232.0f, 237.0f, 242.0f, 244.0f, 246.0f}, // MBH
		{150.0f, 230.0f, 233.0f, 236.0f, 238.0f, 240.0f}, // WJ
		{50.0f , 80.0f , 90.0f , 100.0f, 105.0f, 108.0f}, // LAJ
		{215.0f, 250.0f, 253.0f, 255.0f, 258.0f, 261.0f}, // LAH
		{215.0f, 255.0f, 265.0f, 270.0f, 273.0f, 275.0f}, // JB
	};

};