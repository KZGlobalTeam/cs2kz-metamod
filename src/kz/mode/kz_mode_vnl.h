#pragma once
#include "kz_mode.h"

class KZVanillaModeService : public KZModeService
{
	using KZModeService::KZModeService;

	f32 distanceTiers[JUMPTYPE_COUNT - 3][DISTANCETIER_COUNT] = {
		{215.0f, 230.0f, 235.0f, 240.0f, 245.0f, 248.0f}, // LJ
		{150.0f, 230.0f, 233.0f, 236.0f, 238.0f, 240.0f}, // BH
		{150.0f, 232.0f, 237.0f, 242.0f, 244.0f, 246.0f}, // MBH
		{150.0f, 230.0f, 233.0f, 236.0f, 238.0f, 240.0f}, // WJ
		{50.0f, 80.0f, 90.0f, 100.0f, 105.0f, 108.0f},    // LAJ
		{215.0f, 250.0f, 253.0f, 255.0f, 258.0f, 261.0f}, // LAH
		{215.0f, 255.0f, 260.0f, 265.0f, 270.0f, 272.0f}, // JB
	};

	static inline CVValue_t modeCvarValues[] = {
		(bool)true,            // slope_drop_enable
		(float)5.5f,           // sv_accelerate
		(bool)true,            // sv_accelerate_use_weapon_speed
		(float)12.0f,          // sv_airaccelerate
		(float)30.0f,          // sv_air_max_wishspeed
		(bool)false,           // sv_autobunnyhopping
		(float)0.0f,           // sv_bounce
		(bool)false,           // sv_enablebunnyhopping
		(float)5.2f,           // sv_friction
		(float)800.0f,         // sv_gravity
		(float)301.993377411f, // sv_jump_impulse
		(bool)true,            // sv_jump_precision_enable
		(float)0.015625f,      // sv_jump_spam_penalty_time
		(float)-0.707f,        // sv_ladder_angle
		(float)0.2f,           // sv_ladder_dampen
		(float)0.78f,          // sv_ladder_scale_speed
		(float)250.0f,         // sv_maxspeed is 250.0 instead of 320.0 to prevent abuses with no weapon
		(float)3500.0f,        // sv_maxvelocity
		(float)0.08f,          // sv_staminajumpcost
		(float)0.05f,          // sv_staminalandcost
		(float)80.0f,          // sv_staminamax
		(float)60.0f,          // sv_staminarecoveryrate
		(float)0.7f,           // sv_standable_normal
		(float)64.0f,          // sv_step_move_vel_min
		(float)0.4f,           // sv_timebetweenducks
		(float)0.7f,           // sv_walkable_normal
		(float)10.0f,          // sv_wateraccelerate
		(float)1.0f,           // sv_waterfriction
		(float)0.9f,           // sv_water_slow_amount
		(int)0,                // mp_solid_teammates
		(int)0,                // mp_solid_enemies
		(bool)true,            // sv_subtick_movement_view_angles
	};

	static_assert(KZ_ARRAYSIZE(modeCvarValues) == MODECVAR_COUNT, "Array modeCvarValues length is not the same as MODECVAR_COUNT!");

	// Keep track of TryPlayerMove path for triggerfixing.
	bool airMoving {};
	CUtlVector<Vector> tpmTriggerFixOrigins;

public:
	virtual void Reset() override;
	virtual const char *GetModeName() override;
	virtual const char *GetModeShortName() override;
	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) override;
	virtual const CVValue_t *GetModeConVarValues() override;

	// Triggerfix
	virtual void OnSetupMove(PlayerCommand *pc) override;
	virtual void OnPlayerMove() override;
	virtual void OnProcessMovementPost() override;
	virtual void OnDuckPost() override;
	virtual void OnAirMove() override;
	virtual void OnAirMovePost() override;
	virtual void OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing) override;
	virtual void OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing) override;

	virtual bool OnTriggerStartTouch(CBaseTrigger *trigger) override;
	virtual bool OnTriggerTouch(CBaseTrigger *trigger) override;
	virtual bool OnTriggerEndTouch(CBaseTrigger *trigger) override;
	virtual void OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity) override;

	virtual void OnStartTouchGround() override;
};
