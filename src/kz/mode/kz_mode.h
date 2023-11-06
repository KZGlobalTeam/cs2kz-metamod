#pragma once
#include "../kz.h"
#include "../jumpstats/kz_jumpstats.h"
class KZPlayer;

class KZModeService : public KZBaseService
{
	using KZBaseService::KZBaseService;
	
public:
	virtual const char* GetModeName() = 0;
	virtual const char* GetModeShortName() = 0;
	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) = 0;
};

namespace KZ::mode
{
	void InitModeService(KZPlayer *player);
};