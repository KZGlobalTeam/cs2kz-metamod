#include "kz_mode_ckz.h"

const char *KZClassicModeService::GetModeName()
{
	return "Classic";
}
const char *KZClassicModeService::GetModeShortName()
{
	return "CKZ";
}

DistanceTier KZClassicModeService::GetDistanceTier(JumpType jumpType, f32 distance)
{
	// No tiers given for 'Invalid' jumps.
	if (jumpType == JumpType_Invalid || jumpType == JumpType_FullInvalid
		|| jumpType == JumpType_Fall || jumpType == JumpType_Other
		|| distance > 500.0f)
	{
		return DistanceTier_None;
	}

	// Get highest tier distance that the jump beats
	DistanceTier tier = DistanceTier_None;
	while (tier + 1 < DISTANCETIER_COUNT && distance >= distanceTiers[jumpType][tier])
	{
		tier = (DistanceTier)(tier + 1);
	}

	return tier;
}