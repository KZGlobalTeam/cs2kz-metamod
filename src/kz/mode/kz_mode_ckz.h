#pragma once

#include "kz_mode.h"
#include "sdk/datatypes.h"

#define MODE_NAME_SHORT "CKZ"
#define MODE_NAME       "Classic"
// Rampbug fix related
#define MAX_BUMPS                   4
#define RAMP_PIERCE_DISTANCE        0.15f
#define RAMP_BUG_THRESHOLD          0.98f
#define RAMP_BUG_VELOCITY_THRESHOLD 0.95f
#define NEW_RAMP_THRESHOLD          0.95f

#define SPEED_NORMAL 250.0f
// Prestrafe related
#define PS_SPEED_MAX        26.0f
#define PS_MIN_REWARD_RATE  2.0f  // Minimum computed turn rate for any prestrafe reward
#define PS_MAX_REWARD_RATE  15.5f // Ideal computed turn rate for maximum prestrafe reward
#define PS_MAX_PS_TIME      0.50f // Time to reach maximum prestrafe speed with optimal turning
#define PS_TURN_RATE_WINDOW 0.02f // Turn rate will be computed over this amount of time
#define PS_DECREMENT_RATIO  3.0f  // Prestrafe will lose this fast compared to gaining
// Controls the ratio between prestrafe ratio and gain.
// The lower the value, the faster the prespeed gain at the start, but the slower the gain near the max value.
// 1 means prestrafe gain is linear with the ratio.
#define PS_RATIO_TO_SPEED 0.5f
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
		{120.0f, 160.0f, 170.0f, 180.0f, 190.0f, 200.0f}, // LAJ
		{217.0f, 260.0f, 265.0f, 270.0f, 275.0f, 280.0f}, // LAH
		{217.0f, 275.0f, 280.0f, 285.0f, 290.0f, 295.0f}, // JB
	};

	static inline CVValue_t modeCvarValues[] = {
		(bool)false,    // slope_drop_enable
		(float)6.5f,    // sv_accelerate
		(bool)false,    // sv_accelerate_use_weapon_speed
		(float)100.0f,  // sv_airaccelerate
		(float)30.0f,   // sv_air_max_wishspeed
		(bool)false,    // sv_autobunnyhopping
		(bool)true,     // sv_enablebunnyhopping
		(float)5.2f,    // sv_friction
		(float)800.0f,  // sv_gravity
		(float)302.0f,  // sv_jump_impulse
		(bool)false,    // sv_jump_precision_enable
		(float)0.0f,    // sv_jump_spam_penalty_time
		(float)-0.707f, // sv_ladder_angle
		(float)1.0f,    // sv_ladder_dampen
		(float)1.0f,    // sv_ladder_scale_speed
		(float)320.0f,  // sv_maxspeed
		(float)3500.0f, // sv_maxvelocity
		(float)0.0f,    // sv_staminajumpcost
		(float)0.0f,    // sv_staminalandcost
		(float)0.0f,    // sv_staminamax
		(float)9999.0f, // sv_staminarecoveryrate
		(float)0.7f,    // sv_standable_normal
		(float)64.0f,   // sv_step_move_vel_min
		(float)0.0f,    // sv_timebetweenducks
		(float)0.7f,    // sv_walkable_normal
		(float)10.0f,   // sv_wateraccelerate
		(float)1.0f,    // sv_waterfriction
		(float)0.9f,    // sv_water_slow_amount
		(int)0,         // mp_solid_teammates
		(int)0          // mp_solid_enemies
	};
	static_assert(KZ_ARRAYSIZE(modeCvarValues) == MODECVAR_COUNT, "Array modeCvarValues length is not the same as MODECVAR_COUNT!");

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
	Vector tpmVelocity = vec3_invalid;
	Vector tpmOrigin = vec3_invalid;
	Vector lastValidPlane = vec3_origin;

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
	virtual const CVValue_t *GetModeConVarValues() override;

	virtual void OnPhysicsSimulate() override;
	virtual void OnPhysicsSimulatePost() override;
	virtual void OnSetupMove(PlayerCommand *pc) override;
	virtual void OnProcessMovement() override;
	virtual void OnPlayerMove() override;
	virtual void OnProcessMovementPost() override;
	virtual void OnCategorizePosition(bool bStayOnGround) override;
	virtual void OnDuckPost() override;
	virtual void OnAirMove() override;
	virtual void OnAirMovePost() override;
	virtual void OnWaterMove() override;
	virtual void OnWaterMovePost() override;
	virtual void OnStartTouchGround() override;
	virtual void OnStopTouchGround() override;
	virtual void OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing) override;
	virtual void OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing) override;
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
