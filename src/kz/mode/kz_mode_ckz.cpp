#include "cs_usercmd.pb.h"
#include "kz_mode_ckz.h"
#include "utils/interfaces.h"
#include "version.h"

KZClassicModePlugin g_KZClassicModePlugin;

KZUtils *g_pKZUtils = NULL;
KZModeManager *g_pModeManager = NULL;
ModeServiceFactory g_ModeFactory = [](KZPlayer *player) -> KZModeService *{ return new KZClassicModeService(player); };
PLUGIN_EXPOSE(KZClassicModePlugin, g_KZClassicModePlugin);

bool KZClassicModePlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	// Load mode
	int success;
	g_pModeManager = (KZModeManager *)g_SMAPI->MetaFactory(KZ_MODE_MANAGER_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_MODE_MANAGER_INTERFACE);
		return false;
	}
	g_pKZUtils = (KZUtils *)g_SMAPI->MetaFactory(KZ_UTILS_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_UTILS_INTERFACE);
		return false;
	}

	if (nullptr == (interfaces::pSchemaSystem = (CSchemaSystem *)g_pKZUtils->GetSchemaSystemPointer())
		|| nullptr == (schema::StateChanged = (StateChanged_t *)g_pKZUtils->GetSchemaStateChangedPointer())
		|| nullptr == (schema::NetworkStateChanged = (NetworkStateChanged_t *)g_pKZUtils->GetSchemaNetworkStateChangedPointer())
		|| !g_pModeManager->RegisterMode(g_PLID, MODE_NAME_SHORT, MODE_NAME, g_ModeFactory))
	{
		return false;
	}
	
	return true;
}

bool KZClassicModePlugin::Unload(char *error, size_t maxlen)
{
	g_pModeManager->UnregisterMode(MODE_NAME);
	return true;
}

void KZClassicModePlugin::AllPluginsLoaded()
{
}

bool KZClassicModePlugin::Pause(char *error, size_t maxlen)
{
	g_pModeManager->UnregisterMode(MODE_NAME);
	return true;
}

bool KZClassicModePlugin::Unpause(char *error, size_t maxlen)
{
	if (!g_pModeManager->RegisterMode(g_PLID, MODE_NAME_SHORT, MODE_NAME, g_ModeFactory))
	{
		return false;
	}
	return true;
}

const char *KZClassicModePlugin::GetLicense()
{
	return "GPLv3";
}

const char *KZClassicModePlugin::GetVersion()
{
	return VERSION_STRING;
}

const char *KZClassicModePlugin::GetDate()
{
	return __DATE__;
}

const char *KZClassicModePlugin::GetLogTag()
{
	return "KZ-Mode-Classic";
}

const char *KZClassicModePlugin::GetAuthor()
{
	return "zer0.k";
}

const char *KZClassicModePlugin::GetDescription()
{
	return "Classic mode plugin for CS2KZ";
}

const char *KZClassicModePlugin::GetName()
{
	return "CS2KZ-Mode-Classic";
}

const char *KZClassicModePlugin::GetURL()
{
	return "https://github.com/KZGlobalTeam/cs2kz-metamod";
}

/*
	Actual mode stuff.
*/
#define SPEED_NORMAL 250.0f

#define PS_SPEED_MAX 26.0f
#define PS_MIN_REWARD_RATE 7.0f // Minimum computed turn rate for any prestrafe reward
#define PS_MAX_REWARD_RATE 16.0f // Ideal computed turn rate for maximum prestrafe reward
#define PS_MAX_PS_TIME 0.55f // Time to reach maximum prestrafe speed with optimal turning
#define PS_TURN_RATE_WINDOW 0.02f // Turn rate will be computed over this amount of time
#define PS_DECREMENT_RATIO 3.0f // Prestrafe will lose this fast compared to gaining
#define PS_RATIO_TO_SPEED 0.5f
// Prestrafe ratio will be not go down after landing for this amount of time - helps with small movements after landing
// Ideally should be much higher than the perf window!
#define PS_LANDING_GRACE_PERIOD 0.25f 

#define BH_PERF_WINDOW 0.02f // Any jump performed after landing will be a perf for this much time
#define BH_BASE_MULTIPLIER 51.5f // Multiplier for how much speed would a perf gain in ideal scenario
#define BH_LANDING_DECREMENT_MULTIPLIER 75.0f // How much would a non real perf impact the takeoff speed
// Magic number so that landing speed at max ground prestrafe speed would result in the same takeoff velocity
#define BH_NORMALIZE_FACTOR (BH_BASE_MULTIPLIER * log(SPEED_NORMAL + PS_SPEED_MAX) - (SPEED_NORMAL + PS_SPEED_MAX)) 

#define DUCK_SPEED_NORMAL 8.0f
#define DUCK_SPEED_MINIMUM 6.0234375f // Equal to if you just ducked/unducked for the first time in a while

const char *KZClassicModeService::GetModeName()
{
	return MODE_NAME;
}

const char *KZClassicModeService::GetModeShortName()
{
	return MODE_NAME_SHORT;
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

f32 KZClassicModeService::GetPlayerMaxSpeed()
{
	return SPEED_NORMAL + this->GetPrestrafeGain();
}

const char **KZClassicModeService::GetModeConVarValues()
{
	return modeCvarValues;
}

// Attempt to replicate 128t jump height.
void KZClassicModeService::OnJump()
{
	float time = this->player->GetMoveServices()->m_flJumpPressedTime;
	Vector velocity;
	this->player->GetVelocity(&velocity);
	this->preJumpZSpeed = velocity.z;
	// Emulate the 128t vertical velocity before jumping
	if (this->player->GetPawn()->m_fFlags & FL_ONGROUND)
	{
		// TODO: update util functions to be accessible across plugins
		velocity.z += 0.25 * this->player->GetPawn()->m_flGravityScale() * 800 * ENGINE_FIXED_TICK_INTERVAL;
		this->player->SetVelocity(velocity);
		this->tweakedJumpZSpeed = velocity.z;
		this->revertJumpTweak = true;
	}
}

void KZClassicModeService::OnJumpPost()
{
	// If we didn't jump, we revert the jump height tweak.
	Vector velocity;
	this->player->GetVelocity(&velocity);
	if (this->revertJumpTweak && velocity.z == this->tweakedJumpZSpeed)
	{
		velocity.z = this->preJumpZSpeed;
		this->player->SetVelocity(velocity);
	}
	this->revertJumpTweak = false;
}

void KZClassicModeService::OnStopTouchGround()
{
	Vector velocity;
	this->player->GetVelocity(&velocity);
	f32 speed = velocity.Length2D();

	// If we are actually taking off, we don't need to revert the change anymore.
	this->revertJumpTweak = false;

	f32 timeOnGround = this->player->takeoffTime - this->player->landingTime;
	// Perf
	if (timeOnGround <= BH_PERF_WINDOW)
	{
		// Perf speed
		Vector2D landingVelocity2D(this->player->landingVelocity.x, this->player->landingVelocity.y);
		landingVelocity2D.NormalizeInPlace();
		float newSpeed = MAX(this->player->landingVelocity.Length2D(), this->player->takeoffVelocity.Length2D());
		if (newSpeed > SPEED_NORMAL + this->GetPrestrafeGain())
		{
			newSpeed = MIN(newSpeed, (BH_BASE_MULTIPLIER - timeOnGround * BH_LANDING_DECREMENT_MULTIPLIER) * log(newSpeed) - BH_NORMALIZE_FACTOR);
			// Make sure it doesn't go lower than the ground speed.
			newSpeed = MAX(newSpeed, SPEED_NORMAL + this->GetPrestrafeGain());
		}
		velocity.x = newSpeed * landingVelocity2D.x;
		velocity.y = newSpeed * landingVelocity2D.y;
		this->player->SetVelocity(velocity);
		this->player->takeoffVelocity = velocity;

		// Perf height
		Vector origin;
		this->player->GetOrigin(&origin);
		origin.z = this->player->GetGroundPosition();
		this->player->SetOrigin(origin);
		this->player->takeoffOrigin = origin;
	}
}

void KZClassicModeService::OnStartTouchGround()
{
	this->SlopeFix();
}

void KZClassicModeService::OnProcessUsercmds(void *cmds, int numcmds)
{
	this->lastDesiredViewAngleTime = g_pKZUtils->GetServerGlobals()->curtime;
	for (i32 i = 0; i < numcmds; i++)
	{
		auto address = reinterpret_cast<char *>(cmds) + i * offsets::UsercmdOffset;
		CSGOUserCmdPB *usercmdsPtr = reinterpret_cast<CSGOUserCmdPB *>(address);
		this->lastDesiredViewAngle = { usercmdsPtr->base().viewangles().x(), usercmdsPtr->base().viewangles().y(), usercmdsPtr->base().viewangles().z() };
		for (i32 j = 0; j < usercmdsPtr->mutable_base()->subtick_moves_size(); j++)
		{
			CSubtickMoveStep *subtickMove = usercmdsPtr->mutable_base()->mutable_subtick_moves(j);
			float when = subtickMove->when();
			// Iffy logic that doesn't care about commands with multiple cmds...
			if (subtickMove->button() == IN_JUMP)
			{
				f32 inputTime = (g_pKZUtils->GetServerGlobals()->tickcount + when - 1) * ENGINE_FIXED_TICK_INTERVAL;
				if (when != 0)
				{
					if (subtickMove->pressed() && inputTime - this->lastJumpReleaseTime > 0.0078125)
					{
						this->player->GetMoveServices()->m_bOldJumpPressed = false;
					}
					if (!subtickMove->pressed())
					{
						this->lastJumpReleaseTime = (g_pKZUtils->GetServerGlobals()->tickcount + when - 1) * ENGINE_FIXED_TICK_INTERVAL;
					}
				}
			}
			subtickMove->set_when(when >= 0.5 ? 0.5 : 0);
		}
	}
	this->InsertSubtickTiming(g_pKZUtils->GetServerGlobals()->tickcount * ENGINE_FIXED_TICK_INTERVAL - 0.5 * ENGINE_FIXED_TICK_INTERVAL, false);
}

void KZClassicModeService::OnProcessMovement()
{
	this->player->enableWaterFixThisTick = true;
	this->CheckVelocityQuantization();
	this->RemoveCrouchJumpBind();
	this->ReduceDuckSlowdown();
	this->InterpolateViewAngles();
	this->UpdateAngleHistory();
	this->CalcPrestrafe();
}

void KZClassicModeService::OnProcessMovementPost()
{
	this->InsertSubtickTiming(g_pKZUtils->GetServerGlobals()->tickcount * ENGINE_FIXED_TICK_INTERVAL + 0.5 * ENGINE_FIXED_TICK_INTERVAL, true);
	this->RestoreInterpolatedViewAngles();
	this->oldDuckPressed = this->forcedUnduck || this->player->IsButtonDown(IN_DUCK, true);
	Vector velocity;
	this->player->GetVelocity(&velocity);
	this->postProcessMovementZSpeed = velocity.z;
}

void KZClassicModeService::InsertSubtickTiming(float time, bool future)
{
	CCSPlayer_MovementServices *moveServices = this->player->GetMoveServices();
	if (!moveServices
		|| fabs(roundf(time) - time) < 0.001 // Don't create subtick too close to real time, there will be movement processing there anyway.
		|| time * ENGINE_FIXED_TICK_RATE - g_pKZUtils->GetServerGlobals()->tickcount > 1.0f // Don't create subtick timing too far into the future.
		|| time * ENGINE_FIXED_TICK_RATE - g_pKZUtils->GetServerGlobals()->tickcount < -1.0f) // Don't create subtick timing too far back.
	{
		return;
	}

	for (i32 i = 0; i < 4; i++)
	{
		// Already exists.
		if (fabs(time - moveServices->m_arrForceSubtickMoveWhen[i]) < EPSILON)
		{
			return;
		}
		if (!future)
		{
			// Do not override other valid subtick moves that might happen.
			if (moveServices->m_arrForceSubtickMoveWhen[i] - g_pKZUtils->GetServerGlobals()->curtime < ENGINE_FIXED_TICK_INTERVAL) continue;
			moveServices->m_arrForceSubtickMoveWhen[i] = time;
			return;
		}
		if (moveServices->m_arrForceSubtickMoveWhen[i] <= g_pKZUtils->GetServerGlobals()->curtime)
		{
			moveServices->m_arrForceSubtickMoveWhen[i] = time;
		}
	}
}

void KZClassicModeService::InterpolateViewAngles()
{
	// Second half of the movement, no change.
	CGlobalVars *globals = g_pKZUtils->GetServerGlobals();
	// NOTE: tickcount is half a tick ahead of curtime while in the middle of a tick.
	if ((f64)globals->tickcount * ENGINE_FIXED_TICK_INTERVAL - globals->curtime < 0.001)
		return;

	if (this->lastDesiredViewAngleTime < g_pKZUtils->GetServerGlobals()->curtime + ENGINE_FIXED_TICK_INTERVAL)
	{
		this->lastDesiredViewAngle = this->player->moveDataPost.m_vecViewAngles;
	}

	// First half of the movement, tweak the angle to be the middle of the desired angle and the last angle
	QAngle newAngles = player->currentMoveData->m_vecViewAngles;
	QAngle oldAngles = this->lastDesiredViewAngle;
	if (newAngles[YAW] - oldAngles[YAW] > 180)
	{
		newAngles[YAW] -= 360.0f;
	}
	else if (newAngles[YAW] - oldAngles[YAW] < -180)
	{
		newAngles[YAW] += 360.0f;
	}

	for (u32 i = 0; i < 3; i++)
	{
		newAngles[i] += oldAngles[i];
		newAngles[i] *= 0.5f;
	}

	player->currentMoveData->m_vecViewAngles = newAngles;
}

void KZClassicModeService::RestoreInterpolatedViewAngles()
{
	player->currentMoveData->m_vecViewAngles = player->moveDataPre.m_vecViewAngles;
}

void KZClassicModeService::RemoveCrouchJumpBind()
{
	this->forcedUnduck = false;
	if (this->player->GetPawn()->m_fFlags & FL_ONGROUND && !this->oldDuckPressed && !this->player->GetMoveServices()->m_bOldJumpPressed && this->player->IsButtonDown(IN_JUMP))
	{
		this->player->GetMoveServices()->m_nButtons()->m_pButtonStates[0] &= ~IN_DUCK;
		this->forcedUnduck = true;
	}
}

void KZClassicModeService::ReduceDuckSlowdown()
{
	if (!this->player->GetMoveServices()->m_bDucking && this->player->GetMoveServices()->m_flDuckSpeed < DUCK_SPEED_NORMAL - EPSILON)
	{
		this->player->GetMoveServices()->m_flDuckSpeed = DUCK_SPEED_NORMAL;
	}
	else if (this->player->GetMoveServices()->m_flDuckSpeed < DUCK_SPEED_MINIMUM - EPSILON)
	{
		this->player->GetMoveServices()->m_flDuckSpeed = DUCK_SPEED_MINIMUM;
	}
}

void KZClassicModeService::UpdateAngleHistory()
{
	CMoveData *mv = this->player->currentMoveData;
	u32 oldEntries = 0;
	FOR_EACH_VEC(this->angleHistory, i)
	{
		if (this->angleHistory[i].when + PS_TURN_RATE_WINDOW < g_pKZUtils->GetServerGlobals()->curtime)
		{
			oldEntries++;
			continue;
		}
		break;
	}
	this->angleHistory.RemoveMultipleFromHead(oldEntries);
	if ((this->player->GetPawn()->m_fFlags & FL_ONGROUND) == 0)
		return;

	AngleHistory *angHist = this->angleHistory.AddToTailGetPtr();
	angHist->when = g_pKZUtils->GetServerGlobals()->curtime;
	angHist->duration = g_pKZUtils->GetServerGlobals()->frametime;

	// Not turning if velocity is null.
	if (mv->m_vecVelocity.Length2D() == 0)
	{
		angHist->rate = 0;
		return;
	}

	// Copying from WalkMove
	Vector forward, right, up;
	AngleVectors(mv->m_vecViewAngles, &forward, &right, &up);

	f32 fmove = mv->m_flForwardMove;
	f32 smove = -mv->m_flSideMove;

	if (forward[2] != 0)
	{
		forward[2] = 0;
		VectorNormalize(forward);
	}

	if (right[2] != 0)
	{
		right[2] = 0;
		VectorNormalize(right);
	}

	Vector wishdir;
	for (int i = 0; i < 2; i++)
		wishdir[i] = forward[i] * fmove + right[i] * smove;
	wishdir[2] = 0;

	VectorNormalize(wishdir);

	if (wishdir.Length() == 0)
	{
		angHist->rate = 0;
		return;
	}

	Vector velocity = mv->m_vecVelocity;
	velocity[2] = 0;
	VectorNormalize(velocity);
	QAngle accelAngle;
	QAngle velAngle;
	VectorAngles(wishdir, accelAngle);
	VectorAngles(velocity, velAngle);
	accelAngle.y = g_pKZUtils->NormalizeDeg(accelAngle.y);
	velAngle.y = g_pKZUtils->NormalizeDeg(velAngle.y);
	angHist->rate = g_pKZUtils->GetAngleDifference(velAngle.y, accelAngle.y, 180.0, true);
}

void KZClassicModeService::CalcPrestrafe()
{
	f32 totalDuration = 0;
	f32 sumWeightedAngles = 0;
	FOR_EACH_VEC(this->angleHistory, i)
	{
		sumWeightedAngles += this->angleHistory[i].rate * this->angleHistory[i].duration;
		totalDuration += this->angleHistory[i].duration;
	}
	f32 averageRate;
	if (totalDuration == 0) averageRate = 0;
	else averageRate = sumWeightedAngles / totalDuration;

	f32 rewardRate = Clamp(fabs(averageRate) / PS_MAX_REWARD_RATE, 0.0f, 1.0f) * g_pKZUtils->GetServerGlobals()->frametime;
	f32 punishRate = 0.0f;
	if (this->player->landingTime + PS_LANDING_GRACE_PERIOD < g_pKZUtils->GetServerGlobals()->curtime)
	{
		punishRate = g_pKZUtils->GetServerGlobals()->frametime * PS_DECREMENT_RATIO;
	}

	if (this->player->GetPawn()->m_fFlags & FL_ONGROUND)
	{
		// Prevent instant full pre from crouched prestrafe.
		Vector velocity;
		this->player->GetVelocity(&velocity);

		f32 currentPreRatio;
		if (velocity.Length2D() <= 0.0f) currentPreRatio = 0.0f;
		else currentPreRatio = pow(this->bonusSpeed / PS_SPEED_MAX * SPEED_NORMAL / velocity.Length2D(), 1 / PS_RATIO_TO_SPEED) * PS_MAX_PS_TIME;


		this->leftPreRatio = MIN(this->leftPreRatio, currentPreRatio);
		this->rightPreRatio = MIN(this->rightPreRatio, currentPreRatio);

		this->leftPreRatio += averageRate > PS_MIN_REWARD_RATE ? rewardRate : -punishRate;
		this->rightPreRatio += averageRate < -PS_MIN_REWARD_RATE ? rewardRate : -punishRate;
		this->leftPreRatio = Clamp(leftPreRatio, 0.0f, PS_MAX_PS_TIME);
		this->rightPreRatio = Clamp(rightPreRatio, 0.0f, PS_MAX_PS_TIME);
		this->bonusSpeed = this->GetPrestrafeGain() / SPEED_NORMAL * velocity.Length2D();
	}
	else
	{
		rewardRate = g_pKZUtils->GetServerGlobals()->frametime;
		// Raise both left and right pre to the same value as the player is in the air.
		if (this->leftPreRatio < this->rightPreRatio)
			this->leftPreRatio = Clamp(this->leftPreRatio + rewardRate, 0.0f, rightPreRatio);
		else this->rightPreRatio = Clamp(this->rightPreRatio + rewardRate, 0.0f, leftPreRatio);
	}
}

f32 KZClassicModeService::GetPrestrafeGain()
{
	return PS_SPEED_MAX * pow(MAX(this->leftPreRatio, this->rightPreRatio) / PS_MAX_PS_TIME, PS_RATIO_TO_SPEED);
}

void KZClassicModeService::CheckVelocityQuantization()
{
	if (this->postProcessMovementZSpeed > this->player->currentMoveData->m_vecVelocity.z
		&& this->postProcessMovementZSpeed - this->player->currentMoveData->m_vecVelocity.z < 0.03125f
		// Colliding with a flat floor can result in a velocity of +0.0078125u/s, and this breaks ladders.
		// The quantization accidentally fixed this bug...
		&& fabs(this->player->currentMoveData->m_vecVelocity.z) > 0.03125f)
	{
		this->player->currentMoveData->m_vecVelocity.z = this->postProcessMovementZSpeed;
	}
}

// ORIGINAL AUTHORS : Mev & Blacky
// URL: https://forums.alliedmods.net/showthread.php?p=2322788
void KZClassicModeService::SlopeFix()
{
	CTraceFilterPlayerMovementCS filter;
	g_pKZUtils->InitPlayerMovementTraceFilter(filter, this->player->GetPawn(), 
		this->player->GetPawn()->m_Collision().m_collisionAttribute().m_nInteractsWith(), COLLISION_GROUP_PLAYER_MOVEMENT);

	Vector ground = this->player->currentMoveData->m_vecAbsOrigin;
	ground.z -= 2;

	f32 standableZ = 0.7f; // Equal to the mode's cvar.

	bbox_t bounds;
	bounds.mins = { -16.0, -16.0, 0.0 };
	bounds.maxs = { 16.0, 16.0, 72.0 };

	if (this->player->GetMoveServices()->m_bDucked()) bounds.maxs.z = 54.0;

	trace_t_s2 trace;
	g_pKZUtils->InitGameTrace(&trace);

	g_pKZUtils->TracePlayerBBox(this->player->currentMoveData->m_vecAbsOrigin, ground, bounds, &filter, trace);

	// Doesn't hit anything, fall back to the original ground
	if (trace.startsolid || trace.fraction == 1.0f)
	{
		return;
	}
	// TODO: Unhardcode sv_standable_normal
	if (standableZ <= trace.planeNormal.z && trace.planeNormal.z < 1.0f)
	{
		// Copy the ClipVelocity function from sdk2013
		float backoff;
		float change;
		Vector newVelocity;

		backoff = DotProduct(this->player->landingVelocity, trace.planeNormal) * 1;

		for (u32 i = 0; i < 3; i++)
		{
			change = trace.planeNormal[i] * backoff;
			newVelocity[i] = this->player->landingVelocity[i] - change;
		}

		f32 adjust = DotProduct(newVelocity, trace.planeNormal);
		if (adjust < 0.0f)
		{
			newVelocity -= (trace.planeNormal * adjust);
		}
		// Make sure the player is going down a ramp by checking if they actually will gain speed from the boost.
		if (newVelocity.Length2D() >= this->player->landingVelocity.Length2D())
		{
			this->player->currentMoveData->m_vecVelocity.x = newVelocity.x;
			this->player->currentMoveData->m_vecVelocity.y = newVelocity.y;
			this->player->landingVelocity.x = newVelocity.x;
			this->player->landingVelocity.y = newVelocity.y;
		}
	}
}
