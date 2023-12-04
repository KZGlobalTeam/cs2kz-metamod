#pragma once
#include "kz_mode.h"

class KZClassicModeService : public KZModeService
{
	using KZModeService::KZModeService;
public:
	virtual const char *GetModeName() override;
	virtual const char *GetModeShortName() override;
	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) override;
	f32 distanceTiers[JUMPTYPE_COUNT - 3][DISTANCETIER_COUNT] =
	{
		{217.0f, 265.0f, 270.0f, 275.0f, 280.0f, 285.0f}, // LJ
		{217.0f, 270.0f, 275.0f, 280.0f, 285.0f, 290.0f}, // BH
		{217.0f, 270.0f, 275.0f, 280.0f, 285.0f, 290.0f}, // MBH
		{217.0f, 270.0f, 275.0f, 280.0f, 285.0f, 290.0f}, // WJ
		{120.0f, 180.0f, 185.0f, 190.0f, 195.0f, 200.0f}, // LAJ
		{217.0f, 260.0f, 265.0f, 270.0f, 275.0f, 280.0f}, // LAH
		{217.0f, 270.0f, 275.0f, 280.0f, 285.0f, 290.0f}, // JB
	};

};