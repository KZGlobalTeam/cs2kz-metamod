#pragma once
#include "kz_mode.h"

class KZVanillaModeService : public KZModeService
{
	using KZModeService::KZModeService;

	f32 distanceTiers[JUMPTYPE_COUNT - 3][DISTANCETIER_COUNT] = {
		{215.0f, 230.0f, 235.0f, 240.0f, 244.0f, 246.0f}, // LJ
		{150.0f, 230.0f, 233.0f, 236.0f, 238.0f, 240.0f}, // BH
		{150.0f, 232.0f, 237.0f, 242.0f, 244.0f, 246.0f}, // MBH
		{150.0f, 230.0f, 233.0f, 236.0f, 238.0f, 240.0f}, // WJ
		{50.0f, 80.0f, 90.0f, 100.0f, 105.0f, 108.0f},    // LAJ
		{215.0f, 250.0f, 253.0f, 255.0f, 258.0f, 261.0f}, // LAH
		{215.0f, 255.0f, 265.0f, 270.0f, 273.0f, 275.0f}, // JB
	};

	const char *modeCvarValues[KZ::mode::numCvar] = {
		"true",          // slope_drop_enable
		"5.5",           // sv_accelerate
		"true",          // sv_accelerate_use_weapon_speed
		"12",            // sv_airaccelerate
		"30",            // sv_air_max_wishspeed
		"false",         // sv_autobunnyhopping
		"false",         // sv_enablebunnyhopping
		"5.2",           // sv_friction
		"800",           // sv_gravity
		"301.993377411", // sv_jump_impulse
		"0.015625",      // sv_jump_spam_penalty_time
		"-0.707",        // sv_ladder_angle
		"0.2",           // sv_ladder_dampen
		"0.78",          // sv_ladder_scale_speed
		"320",           // sv_maxspeed
		"3500",          // sv_maxvelocity
		"0.08",          // sv_staminajumpcost
		"0.05",          // sv_staminalandcost
		"80",            // sv_staminamax
		"60",            // sv_staminarecoveryrate
		"0.7",           // sv_standable_normal
		"64.0",          // sv_step_move_vel_min
		"0.4",           // sv_timebetweenducks
		"0.7",           // sv_walkable_normal
		"10",            // sv_wateraccelerate
		"1",             // sv_waterfriction
		"0.9"            // sv_water_slow_amount
	};

	// Keep track of TryPlayerMove path for triggerfixing.
	bool airMoving {};
	CUtlVector<Vector> tpmTriggerFixOrigins;

public:
	virtual void Reset() override;
	virtual const char *GetModeName() override;
	virtual const char *GetModeShortName() override;
	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) override;
	virtual const char **GetModeConVarValues() override;

	virtual META_RES GetPlayerMaxSpeed(f32 &maxSpeed)
	{
		maxSpeed = MIN(maxSpeed, 250.0f);
		return MRES_SUPERCEDE;
	}

	// Triggerfix
	virtual void OnProcessMovementPost() override;
	virtual void OnDuckPost() override;
	virtual void OnAirMove() override;
	virtual void OnAirMovePost() override;
	virtual void OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace) override;
	virtual void OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace) override;

	virtual bool OnTriggerStartTouch(CBaseTrigger *trigger) override;
	virtual bool OnTriggerTouch(CBaseTrigger *trigger) override;
	virtual bool OnTriggerEndTouch(CBaseTrigger *trigger) override;
	virtual void OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity) override;

	virtual void OnStartTouchGround() override;
};
