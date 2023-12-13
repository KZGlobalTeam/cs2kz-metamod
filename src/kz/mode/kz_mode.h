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

	inline const char *modeCvarNames[] =
	{
		"sv_accelerate",
		"sv_accelerate_use_weapon_speed",
		"sv_airaccelerate",
		"sv_air_max_wishspeed",
		"sv_enablebunnyhopping",
		"sv_friction",
		"sv_gravity",
		"sv_jump_impulse",
		"sv_ladder_scale_speed",
		"sv_maxspeed",
		"sv_maxvelocity",
		"sv_staminajumpcost",
		"sv_staminalandcost",
		"sv_staminamax",
		"sv_staminarecoveryrate",
		"sv_timebetweenducks",
		"sv_walkable_normal",
		"sv_wateraccelerate",
		"sv_water_slow_amount"
	};
	void DisableReplicatedModeCvars();
	void EnableReplicatedModeCvars();
};