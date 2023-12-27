#pragma once

#include "kz_mode.h"
#define MODE_NAME_SHORT "CKZ"
#define MODE_NAME "Classic"

class KZClassicModePlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AllPluginsLoaded();
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

	const char *modeCvarValues[KZ::mode::numCvar] =
	{
		"false",			// slope_drop_enable
		"6.5",				// sv_accelerate
		"false",			// sv_accelerate_use_weapon_speed
		"100",				// sv_airaccelerate
		"30",				// sv_air_max_wishspeed
		"false",			// sv_autobunnyhopping
		"true",				// sv_enablebunnyhopping
		"5.2",				// sv_friction
		"800",				// sv_gravity
		"302.0",			// sv_jump_impulse
		"0.0078125",		// sv_jump_spam_penalty_time
		"-0.707",			// sv_ladder_angle
		"1",				// sv_ladder_dampen
		"1",				// sv_ladder_scale_speed
		"320",				// sv_maxspeed
		"3500",				// sv_maxvelocity
		"0",				// sv_staminajumpcost
		"0",				// sv_staminalandcost
		"0",				// sv_staminamax
		"9999",				// sv_staminarecoveryrate
		"0.7",				// sv_standable_normal
		"0",				// sv_timebetweenducks
		"0.7",				// sv_walkable_normal
		"10",				// sv_wateraccelerate
		"1",				// sv_waterfriction 
		"0.9"				// sv_water_slow_amount
	};

	bool revertJumpTweak{};
	f32 preJumpZSpeed{};
	f32 tweakedJumpZSpeed{};
	f32 lastDesiredViewAngleTime{};
	QAngle lastDesiredViewAngle;
	f32 lastJumpReleaseTime{};

public:
	virtual const char *GetModeName() override;
	virtual const char *GetModeShortName() override;
	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) override;
	virtual const char **GetModeConVarValues() override;
	virtual f32 GetPlayerMaxSpeed() override;

	virtual void OnProcessUsercmds(void *, int) override;
	virtual void OnPlayerMove() override;
	virtual void OnJump() override;
	virtual void OnJumpPost() override;
	virtual void OnPostPlayerMovePost() override;
	virtual void OnStopTouchGround() override;

	// Insert subtick timing to be called later.
	// This is called either at the end of movement processing, or at the start of ProcessUsercmds.
	// If it is called at the end of movement processing, it must set subtick timing into the future.
	// If it is called at the start of ProcessUsercmds, it must set subtick timing in the past.
	void InsertSubtickTiming(float time, bool future);

	void InterpolateViewAngles();
	void RestoreInterpolatedViewAngles();

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
};