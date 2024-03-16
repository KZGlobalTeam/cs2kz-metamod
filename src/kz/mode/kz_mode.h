#pragma once
#include "../kz.h"
#include "../jumpstats/kz_jumpstats.h"
#include "UtlStringMap.h"

#define KZ_MODE_MANAGER_INTERFACE "KZModeManagerInterface"
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
	virtual bool EnableWaterFix() { return false; }
	// Jumpstats
	virtual DistanceTier GetDistanceTier(JumpType jumpType, f32 distance) = 0;
	virtual const char **GetModeConVarValues() = 0;
	virtual f32 GetPlayerMaxSpeed() { return 0.0f; }
	// Movement hooks
	virtual void OnPhysicsSimulate() {}
	virtual void OnPhysicsSimulatePost() {}
	virtual void OnProcessUsercmds(void *, int) {}
	virtual void OnProcessUsercmdsPost(void *, int) {}
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
	virtual void OnTryPlayerMove(Vector *, trace_t_s2 *) {}
	virtual void OnTryPlayerMovePost(Vector *, trace_t_s2 *) {}
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

	virtual bool OnTriggerStartTouch(CBaseTrigger *trigger) { return true; }
	virtual bool OnTriggerTouch(CBaseTrigger *trigger) { return true; }
	virtual bool OnTriggerEndTouch(CBaseTrigger *trigger) { return true; }
	// Other events
	virtual void OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity) {}
};

typedef KZModeService *(*ModeServiceFactory)(KZPlayer *player);

class KZModeManager
{
	struct ModePluginInfo
	{
		PluginId id;
		const char *shortModeName;
		const char *longModeName;
		ModeServiceFactory factory;
		bool shortCmdRegistered;
	};

public:
	const char *defaultMode = "Classic";

public:
	virtual bool RegisterMode(PluginId id, const char *shortModeName, const char *longModeName, ModeServiceFactory factory);
	virtual void UnregisterMode(const char *modeName);
	void LoadDefaultMode();
	bool SwitchToMode(KZPlayer *player, const char *modeName, bool silent = false);
	void Cleanup();
private:
	CUtlVector< ModePluginInfo > modeInfos;
};

extern KZModeManager *g_pKZModeManager;
namespace KZ::mode
{
	bool InitModeCvars();
	void InitModeService(KZPlayer *player);
	void InitModeManager();
	void LoadModePlugins();

	inline const char *modeCvarNames[] =
	{
		"slope_drop_enable",
		"sv_accelerate",
		"sv_accelerate_use_weapon_speed",
		"sv_airaccelerate",
		"sv_air_max_wishspeed",
		"sv_autobunnyhopping",
		"sv_enablebunnyhopping",
		"sv_friction",
		"sv_gravity",
		"sv_jump_impulse",
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
		"sv_timebetweenducks",
		"sv_walkable_normal",
		"sv_wateraccelerate",
		"sv_waterfriction",
		"sv_water_slow_amount"
	};

	constexpr u32 numCvar = sizeof(KZ::mode::modeCvarNames) / sizeof(KZ::mode::modeCvarNames[0]);

	inline ConVarHandle modeCvarHandles[numCvar];
	inline ConVar *modeCvars[numCvar];

	void ApplyModeSettings(KZPlayer *player);
	void DisableReplicatedModeCvars();
	void EnableReplicatedModeCvars();

	void RegisterCommands();
};
