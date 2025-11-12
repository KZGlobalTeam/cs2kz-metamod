#pragma once
#include "../kz.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/global/api.h"
#include "../jumpstats/kz_jumpstats.h"
#include "UtlStringMap.h"

#define KZ_MODE_MANAGER_INTERFACE "KZModeManagerInterface"

enum KzModeCvars
{
	MODECVAR_FIRST = 0,
	MODECVAR_SLOPE_DROP_ENABLE = 0,
	MODECVAR_SV_ACCELERATE,
	MODECVAR_SV_ACCELERATE_USE_WEAPON_SPEED,
	MODECVAR_SV_AIRACCELERATE,
	MODECVAR_SV_AIR_MAX_WISHSPEED,
	MODECVAR_SV_AUTOBUNNYHOPPING,
	MODECVAR_SV_BOUNCE,
	MODECVAR_SV_ENABLEBUNNYHOPPING,
	MODECVAR_SV_FRICTION,
	MODECVAR_SV_GRAVITY,
	MODECVAR_SV_JUMP_IMPULSE,
	MODECVAR_SV_JUMP_PRECISION_ENABLE,
	MODECVAR_SV_JUMP_SPAM_PENALTY_TIME,
	MODECVAR_SV_LADDER_ANGLE,
	MODECVAR_SV_LADDER_DAMPEN,
	MODECVAR_SV_LADDER_SCALE_SPEED,
	MODECVAR_SV_MAXSPEED,
	MODECVAR_SV_MAXVELOCITY,
	MODECVAR_SV_STAMINAJUMPCOST,
	MODECVAR_SV_STAMINALANDCOST,
	MODECVAR_SV_STAMINAMAX,
	MODECVAR_SV_STAMINARECOVERYRATE,
	MODECVAR_SV_STANDABLE_NORMAL,
	MODECVAR_SV_STEP_MOVE_VEL_MIN,
	MODECVAR_SV_TIMEBETWEENDUCKS,
	MODECVAR_SV_WALKABLE_NORMAL,
	MODECVAR_SV_WATERACCELERATE,
	MODECVAR_SV_WATERFRICTION,
	MODECVAR_SV_WATER_SLOW_AMOUNT,
	MODECVAR_MP_SOLID_TEAMMATES,
	MODECVAR_MP_SOLID_ENEMIES,
	MODECVAR_SV_SUBTICK_MOVEMENT_VIEW_ANGLES,
	MODECVAR_COUNT,
};
class KZPlayer;

class KZModeService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	virtual const char *GetModeName() = 0;
	virtual const char *GetModeShortName() = 0;
	virtual void Init() {};
	virtual void Cleanup() {};

	// Fixes
	virtual bool EnableWaterFix()
	{
		return false;
	}

	// Jumpstats
	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) = 0;
	virtual const CVValue_t *GetModeConVarValues() = 0;

	// Movement hooks
	virtual void OnPhysicsSimulate() {}

	virtual void OnPhysicsSimulatePost() {}

	virtual void OnProcessUsercmds(PlayerCommand *, int) {}

	virtual void OnProcessUsercmdsPost(PlayerCommand *, int) {}

	virtual void OnSetupMove(PlayerCommand *) {}

	virtual void OnSetupMovePost(PlayerCommand *) {}

	virtual void OnProcessMovement() {}

	virtual void OnProcessMovementPost() {}

	virtual void OnPlayerMove() {}

	virtual void OnPlayerMovePost() {}

	virtual void OnCheckParameters() {}

	virtual void OnCheckParametersPost() {}

	virtual void OnCanMove() {}

	virtual void OnCanMovePost() {}

	virtual void OnFullWalkMove(bool &) {}

	virtual void OnFullWalkMovePost(bool) {}

	virtual void OnMoveInit() {}

	virtual void OnMoveInitPost() {}

	virtual void OnCheckWater() {}

	virtual void OnCheckWaterPost() {}

	virtual void OnWaterMove() {}

	virtual void OnWaterMovePost() {}

	virtual void OnCheckVelocity(const char *) {}

	virtual void OnCheckVelocityPost(const char *) {}

	virtual void OnDuck() {}

	virtual void OnDuckPost() {}

	// Make an exception for this as it is the only time where we need to change the return value.
	virtual void OnCanUnduck() {}

	virtual void OnCanUnduckPost(bool &) {}

	virtual void OnLadderMove() {}

	virtual void OnLadderMovePost() {}

	virtual void OnCheckJumpButton() {}

	virtual void OnCheckJumpButtonPost() {}

	virtual void OnJump() {}

	virtual void OnJumpPost() {}

	virtual void OnAirMove() {}

	virtual void OnAirMovePost() {}

	virtual void OnAirAccelerate(Vector &wishdir, f32 &wishspeed, f32 &accel) {}

	virtual void OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel) {}

	virtual void OnFriction() {}

	virtual void OnFrictionPost() {}

	virtual void OnWalkMove() {}

	virtual void OnWalkMovePost() {}

	virtual void OnTryPlayerMove(Vector *, trace_t *, bool *) {}

	virtual void OnTryPlayerMovePost(Vector *, trace_t *, bool *) {}

	virtual void OnCategorizePosition(bool) {}

	virtual void OnCategorizePositionPost(bool) {}

	virtual void OnFinishGravity() {}

	virtual void OnFinishGravityPost() {}

	virtual void OnCheckFalling() {}

	virtual void OnCheckFallingPost() {}

	virtual void OnPostPlayerMove() {}

	virtual void OnPostPlayerMovePost() {}

	virtual void OnPostThink() {}

	virtual void OnPostThinkPost() {}

	// Movement events
	virtual void OnStartTouchGround() {}

	virtual void OnStopTouchGround() {}

	virtual void OnChangeMoveType(MoveType_t oldMoveType) {}

	virtual bool OnTriggerStartTouch(CBaseTrigger *trigger)
	{
		return true;
	}

	virtual bool OnTriggerTouch(CBaseTrigger *trigger)
	{
		return true;
	}

	virtual bool OnTriggerEndTouch(CBaseTrigger *trigger)
	{
		return true;
	}

	// Other events
	virtual void OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity) {}
};

typedef KZModeService *(*ModeServiceFactory)(KZPlayer *player);

class KZModeManager
{
public:
	struct ModePluginInfo
	{
		// ID 0 is reserved for VNL
		// -1 is for mode that exists in the database (but not loaded in the plugin)
		// -2 is for invalid mode.
		PluginId id = -2;
		CUtlString shortModeName;
		CUtlString longModeName;
		ModeServiceFactory factory {};
		bool shortCmdRegistered {};
		char md5[33] {};
		i32 databaseID = -1;
	};

	// clang-format off
	virtual bool RegisterMode(PluginId id, const char *shortModeName, const char *longModeName, ModeServiceFactory factory);
	// clang-format on

	virtual void UnregisterMode(PluginId id);
	bool SwitchToMode(KZPlayer *player, const char *modeName, bool silent = false, bool force = false, bool updatePreference = true);
	void Cleanup();
};

extern KZModeManager *g_pKZModeManager;

namespace KZ::mode
{
	bool CheckModeCvars();
	void InitModeService(KZPlayer *player);
	void InitModeManager();
	void LoadModePlugins();
	void UpdateModeDatabaseID(CUtlString name, i32 id, CUtlString shortName = "");
	// clang-format off

	inline const char *modeCvarNames[] = {
		"slope_drop_enable",
		"sv_accelerate",
		"sv_accelerate_use_weapon_speed",
		"sv_airaccelerate",
		"sv_air_max_wishspeed",
		"sv_autobunnyhopping",
		"sv_bounce",
		"sv_enablebunnyhopping",
		"sv_friction",
		"sv_gravity",
		"sv_jump_impulse",
		"sv_jump_precision_enable",
		"sv_jump_spam_penalty_time",
		"sv_ladder_angle",
		"sv_ladder_dampen",
		"sv_ladder_scale_speed",
		"sv_maxspeed",
		"sv_maxvelocity",
		"sv_staminajumpcost",
		"sv_staminalandcost",
		"sv_staminamax",
		"sv_staminarecoveryrate",
		"sv_standable_normal",
		"sv_step_move_vel_min",
		"sv_timebetweenducks",
		"sv_walkable_normal",
		"sv_wateraccelerate",
		"sv_waterfriction",
		"sv_water_slow_amount",
		"mp_solid_teammates",
		"mp_solid_enemies",
		"sv_subtick_movement_view_angles"
	};


	static_assert(KZ_ARRAYSIZE(modeCvarNames) == MODECVAR_COUNT, "Array modeCvarNames length is not the same as MODECVAR_COUNT!");
	// Quite a horrible thing to do but there is no other way around it...
	inline ConVarRefAbstract *modeCvarRefs[] =
	{
		new CConVarRef<bool>("slope_drop_enable"),
		new CConVarRef<float>("sv_accelerate"),
		new CConVarRef<bool>("sv_accelerate_use_weapon_speed"),
		new CConVarRef<float>("sv_airaccelerate"),
		new CConVarRef<float>("sv_air_max_wishspeed"),
		new CConVarRef<bool>("sv_autobunnyhopping"),
		new CConVarRef<float>("sv_bounce"),
		new CConVarRef<bool>("sv_enablebunnyhopping"),
		new CConVarRef<float>("sv_friction"),
		new CConVarRef<float>("sv_gravity"),
		new CConVarRef<float>("sv_jump_impulse"),
		new CConVarRef<bool>("sv_jump_precision_enable"),
		new CConVarRef<float>("sv_jump_spam_penalty_time"),
		new CConVarRef<float>("sv_ladder_angle"),
		new CConVarRef<float>("sv_ladder_dampen"),
		new CConVarRef<float>("sv_ladder_scale_speed"),
		new CConVarRef<float>("sv_maxspeed"),
		new CConVarRef<float>("sv_maxvelocity"),
		new CConVarRef<float>("sv_staminajumpcost"),
		new CConVarRef<float>("sv_staminalandcost"),
		new CConVarRef<float>("sv_staminamax"),
		new CConVarRef<float>("sv_staminarecoveryrate"),
		new CConVarRef<float>("sv_standable_normal"),
		new CConVarRef<float>("sv_step_move_vel_min"),
		new CConVarRef<float>("sv_timebetweenducks"),
		new CConVarRef<float>("sv_walkable_normal"),
		new CConVarRef<float>("sv_wateraccelerate"),
		new CConVarRef<float>("sv_waterfriction"),
		new CConVarRef<float>("sv_water_slow_amount"),
		new CConVarRef<int>("mp_solid_teammates"),
		new CConVarRef<int>("mp_solid_enemies"),
		new CConVarRef<bool>("sv_subtick_movement_view_angles")
	};

	// clang-format on
	static_assert(KZ_ARRAYSIZE(modeCvarNames) == MODECVAR_COUNT, "Array modeCvarRefs length is not the same as MODECVAR_COUNT!");

	void ApplyModeSettings(KZPlayer *player);
	void DisableReplicatedModeCvars();
	void EnableReplicatedModeCvars();

	KZModeManager::ModePluginInfo GetModeInfo(KZModeService *mode);
	KZModeManager::ModePluginInfo GetModeInfo(KZ::API::Mode mode);
	KZModeManager::ModePluginInfo GetModeInfo(CUtlString modeName);
	KZModeManager::ModePluginInfo GetModeInfoFromDatabaseID(i32 id);
}; // namespace KZ::mode
