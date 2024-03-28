#pragma once
#include "../kz.h"

#define KZ_STYLE_MANAGER_INTERFACE "KZStyleManagerInterface"

// TODO styles: normal, backwards, sw, hsw, w only, lowgrav, autobhop, 250 speed, high gravity, notrigger, alivestrafe
class KZStyleService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	virtual const char *GetStyleName() = 0;
	virtual const char *GetStyleShortName() = 0;
	virtual void Init() {};
	virtual void Cleanup() {};

	virtual META_RES GetPlayerMaxSpeed(f32 &maxSpeed)
	{
		return MRES_IGNORED;
	}

	// Movement hooks
	// These functions are always called after the mode service's functions.
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
};

typedef KZStyleService *(*StyleServiceFactory)(KZPlayer *player);

class KZStyleManager
{
	struct StylePluginInfo
	{
		PluginId id;
		const char *shortName;
		const char *longName;
		StyleServiceFactory factory;
	};

public:
	virtual bool RegisterStyle(PluginId id, const char *shortName, const char *longName, StyleServiceFactory factory);
	virtual void UnregisterStyle(const char *styleName);
	bool SwitchToStyle(KZPlayer *player, const char *styleName, bool silent = false);
	void Cleanup();

private:
	CUtlVector<StylePluginInfo> styleInfos;
};

extern KZStyleManager *g_pKZStyleManager;

namespace KZ::style
{
	void InitStyleService(KZPlayer *player);
	void InitStyleManager();
	void LoadStylePlugins();

	void RegisterCommands();
}; // namespace KZ::style
