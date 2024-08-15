#pragma once

#include "kz_mode.h"
#include "sdk/datatypes.h"

#define MODE_NAME_SHORT "CKZ"
#define MODE_NAME       "Classic"
// Rampbug fix related
#define MAX_BUMPS                   4
#define RAMP_PIERCE_DISTANCE        0.1f
#define RAMP_BUG_THRESHOLD          0.99f
#define RAMP_BUG_VELOCITY_THRESHOLD 0.95f
#define NEW_RAMP_THRESHOLD          0.95f

#define SPEED_NORMAL 250.0f
// Prestrafe related
#define PS_SPEED_MAX        26.0f
#define PS_MIN_REWARD_RATE  7.0f  // Minimum computed turn rate for any prestrafe reward
#define PS_MAX_REWARD_RATE  16.0f // Ideal computed turn rate for maximum prestrafe reward
#define PS_MAX_PS_TIME      0.55f // Time to reach maximum prestrafe speed with optimal turning
#define PS_TURN_RATE_WINDOW 0.02f // Turn rate will be computed over this amount of time
#define PS_DECREMENT_RATIO  3.0f  // Prestrafe will lose this fast compared to gaining
#define PS_RATIO_TO_SPEED   0.5f
// Prestrafe ratio will be not go down after landing for this amount of time - helps with small movements after landing
// Ideally should be much higher than the perf window!
#define PS_LANDING_GRACE_PERIOD 0.25f
// Bhop related
#define BH_PERF_WINDOW                  0.02f // Any jump performed after landing will be a perf for this much time
#define BH_BASE_MULTIPLIER              51.5f // Multiplier for how much speed would a perf gain in ideal scenario
#define BH_LANDING_DECREMENT_MULTIPLIER 75.0f // How much would a non real perf impact the takeoff speed
// Magic number so that landing speed at max ground prestrafe speed would result in the same takeoff velocity
#define BH_NORMALIZE_FACTOR (BH_BASE_MULTIPLIER * log(SPEED_NORMAL + PS_SPEED_MAX) - (SPEED_NORMAL + PS_SPEED_MAX))
// Misc
#define DUCK_SPEED_NORMAL  8.0f
#define DUCK_SPEED_MINIMUM 6.0234375f // Equal to if you just ducked/unducked for the first time in a while

class KZClassicModePlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);

public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
};

class KZClassicModeService : public KZModeService
{
	using KZModeService::KZModeService;

	f32 distanceTiers[JUMPTYPE_COUNT - 3][DISTANCETIER_COUNT] = {
		{217.0f, 265.0f, 270.0f, 275.0f, 280.0f, 285.0f}, // LJ
		{217.0f, 275.0f, 280.0f, 285.0f, 290.0f, 295.0f}, // BH
		{217.0f, 275.0f, 280.0f, 285.0f, 290.0f, 295.0f}, // MBH
		{217.0f, 275.0f, 280.0f, 285.0f, 290.0f, 295.0f}, // WJ
		{120.0f, 180.0f, 185.0f, 190.0f, 195.0f, 200.0f}, // LAJ
		{217.0f, 260.0f, 265.0f, 270.0f, 275.0f, 280.0f}, // LAH
		{217.0f, 275.0f, 280.0f, 285.0f, 290.0f, 295.0f}, // JB
	};

	const char *modeCvarValues[KZ::mode::numCvar] = {
		"false",     // slope_drop_enable
		"6.5",       // sv_accelerate
		"false",     // sv_accelerate_use_weapon_speed
		"100",       // sv_airaccelerate
		"30",        // sv_air_max_wishspeed
		"false",     // sv_autobunnyhopping
		"true",      // sv_enablebunnyhopping
		"5.2",       // sv_friction
		"800",       // sv_gravity
		"302.0",     // sv_jump_impulse
		"0.0078125", // sv_jump_spam_penalty_time
		"-0.707",    // sv_ladder_angle
		"1",         // sv_ladder_dampen
		"1",         // sv_ladder_scale_speed
		"320",       // sv_maxspeed
		"3500",      // sv_maxvelocity
		"0",         // sv_staminajumpcost
		"0",         // sv_staminalandcost
		"0",         // sv_staminamax
		"9999",      // sv_staminarecoveryrate
		"0.7",       // sv_standable_normal
		"64.0",      // sv_step_move_vel_min
		"0",         // sv_timebetweenducks
		"0.7",       // sv_walkable_normal
		"10",        // sv_wateraccelerate
		"1",         // sv_waterfriction
		"0.9"        // sv_water_slow_amount
	};

	bool revertJumpTweak {};
	f32 preJumpZSpeed {};
	f32 tweakedJumpZSpeed {};
	bool hasValidDesiredViewAngle {};
	QAngle lastValidDesiredViewAngle;
	f32 lastJumpReleaseTime {};
	bool oldDuckPressed {};
	bool oldJumpPressed {};
	bool forcedUnduck {};
	f32 postProcessMovementZSpeed {};

	struct AngleHistory
	{
		f32 rate;
		f32 when;
		f32 duration;
	};

	CUtlVector<AngleHistory> angleHistory;
	f32 leftPreRatio {};
	f32 rightPreRatio {};
	f32 bonusSpeed {};
	f32 maxPre {};
	f32 originalMaxSpeed {};
	f32 tweakedMaxSpeed {};

	bool didTPM {};
	bool overrideTPM {};
	Vector tpmVelocity;
	Vector tpmOrigin;
	Vector lastValidPlane;

	// Keep track of TryPlayerMove path for triggerfixing.
	bool airMoving {};
	CUtlVector<Vector> tpmTriggerFixOrigins;

public:
	virtual void Reset() override;
	virtual void Cleanup() override;
	virtual const char *GetModeName() override;
	virtual const char *GetModeShortName() override;

	virtual bool EnableWaterFix() override;

	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) override;
	virtual const char **GetModeConVarValues() override;
	virtual META_RES GetPlayerMaxSpeed(f32 &maxSpeed) override;

	virtual void OnPhysicsSimulate() override;
	virtual void OnPhysicsSimulatePost() override;
	virtual void OnSetupMove(CUserCmd *pb) override;
	virtual void OnProcessMovement() override;
	virtual void OnProcessMovementPost() override;
	virtual void OnCategorizePosition(bool bStayOnGround) override;
	virtual void OnDuckPost() override;
	virtual void OnAirMove() override;
	virtual void OnAirMovePost() override;
	virtual void OnJump() override;
	virtual void OnJumpPost() override;
	virtual void OnStartTouchGround() override;
	virtual void OnStopTouchGround() override;
	virtual void OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace) override;
	virtual void OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace) override;
	virtual void OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity) override;

	virtual bool OnTriggerStartTouch(CBaseTrigger *trigger) override;
	virtual bool OnTriggerTouch(CBaseTrigger *trigger) override;
	virtual bool OnTriggerEndTouch(CBaseTrigger *trigger) override;

	// Insert subtick timing to be called later. Should only call this in PhysicsSimulate.
	void InsertSubtickTiming(float time);

	void InterpolateViewAngles();
	void RestoreInterpolatedViewAngles();

	void UpdateAngleHistory();
	void CalcPrestrafe();
	f32 GetPrestrafeGain();

	void CheckVelocityQuantization();
	void RemoveCrouchJumpBind();
	/*
		Ported from DanZay's SimpleKZ:
		Duck speed is reduced by the game upon ducking or unducking.
		The goal here is to accept that duck speed is reduced, but
		stop it from being reduced further when spamming duck.

		This is done by enforcing a minimum duck speed equivalent to
		the value as if the player only ducked once. When not in not
		in the middle of ducking, duck speed is reset to its normal
		value in effort to reduce the number of times the minimum
		duck speed is enforced. This should reduce noticeable lag.
	*/
	void ReduceDuckSlowdown();

	void SlopeFix();
};
