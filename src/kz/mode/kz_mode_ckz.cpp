#include "cs_usercmd.pb.h"
#include "kz_mode_ckz.h"
#include "utils/addresses.h"
#include "utils/interfaces.h"
#include "utils/gameconfig.h"
#include "version.h"

KZClassicModePlugin g_KZClassicModePlugin;

CGameConfig *g_pGameConfig = NULL;
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
	modules::Initialize();
	if (!interfaces::Initialize(ismm, error, maxlen)
		|| nullptr == (g_pGameConfig = g_pKZUtils->GetGameConfig())
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

CGameEntitySystem *GameEntitySystem()
{
	return g_pKZUtils->GetGameEntitySystem();
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
	Vector velocity;
	this->player->GetVelocity(&velocity);
	this->preJumpZSpeed = velocity.z;
	// Emulate the 128t vertical velocity before jumping
	if (this->player->GetPawn()->m_fFlags & FL_ONGROUND && this->player->GetPawn()->m_hGroundEntity().IsValid())
	{
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

	// Fix jump occuring at 0 frametime giving extra heights.
	if (this->revertJumpTweak && g_pKZUtils->GetGlobals()->frametime == 0.0f)
	{
		this->player->GetMoveServices()->m_flJumpUntil -= ENGINE_FIXED_TICK_INTERVAL * (1.0f - g_pKZUtils->GetGlobals()->m_flSubtickFraction);
	}
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
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);
	Vector ground = this->player->landingOrigin;
	ground.z = this->player->GetGroundPosition() - 0.03125f;
	this->player->TouchTriggersAlongPath(this->player->landingOrigin, ground, bounds);
}

void KZClassicModeService::OnPhysicsSimulate()
{
	this->InsertSubtickTiming(g_pKZUtils->GetServerGlobals()->tickcount * ENGINE_FIXED_TICK_INTERVAL - 0.5 * ENGINE_FIXED_TICK_INTERVAL);
}

void KZClassicModeService::OnPhysicsSimulatePost()
{
	this->InsertSubtickTiming(g_pKZUtils->GetServerGlobals()->tickcount * ENGINE_FIXED_TICK_INTERVAL + 0.5 * ENGINE_FIXED_TICK_INTERVAL);
}

void KZClassicModeService::OnProcessUsercmds(void *cmds, int numcmds)
{
	for (i32 i = 0; i < numcmds; i++)
	{
		auto address = reinterpret_cast<char *>(cmds) + i * (sizeof(CSGOUserCmdPB) + g_pGameConfig->GetOffset("UsercmdOffset"));
		CSGOUserCmdPB *usercmdsPtr = reinterpret_cast<CSGOUserCmdPB *>(address);
		for (i32 j = 0; j < usercmdsPtr->mutable_base()->subtick_moves_size(); j++)
		{
			CSubtickMoveStep *subtickMove = usercmdsPtr->mutable_base()->mutable_subtick_moves(j);
			float when = subtickMove->when();
			// Iffy logic that doesn't care about commands with multiple cmds...
			if (subtickMove->button() == IN_JUMP)
			{
				f32 inputTime = (g_pKZUtils->GetGlobals()->tickcount + when - 1) * ENGINE_FIXED_TICK_INTERVAL;
				if (when != 0)
				{
					if (subtickMove->pressed() && inputTime - this->lastJumpReleaseTime > 0.5 * ENGINE_FIXED_TICK_INTERVAL)
					{
						this->player->GetMoveServices()->m_bOldJumpPressed = false;
					}
					if (!subtickMove->pressed())
					{
						this->lastJumpReleaseTime = (g_pKZUtils->GetGlobals()->tickcount + when - 1) * ENGINE_FIXED_TICK_INTERVAL;
					}
				}
			}
			subtickMove->set_when(when >= 0.5 ? 0.5 : 0);
		}
	}
}

void KZClassicModeService::OnProcessMovement()
{
	this->didTPM = false;
	this->CheckVelocityQuantization();
	this->RemoveCrouchJumpBind();
	this->ReduceDuckSlowdown();
	this->InterpolateViewAngles();
	this->UpdateAngleHistory();
	this->CalcPrestrafe();
}

void KZClassicModeService::OnProcessMovementPost()
{
	this->player->UpdateTriggerTouchList();
	this->RestoreInterpolatedViewAngles();
	this->oldDuckPressed = this->forcedUnduck || this->player->IsButtonPressed(IN_DUCK, true);
	Vector velocity;
	this->player->GetVelocity(&velocity);
	this->postProcessMovementZSpeed = velocity.z;
	if (!this->didTPM)
	{
		this->lastValidPlane = vec3_origin;
	}
}

void KZClassicModeService::InsertSubtickTiming(float time)
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
		// Empty slot, let's use this.
		if (moveServices->m_arrForceSubtickMoveWhen[i] < EPSILON)
		{
			moveServices->m_arrForceSubtickMoveWhen[i] = time;
			return;
		}
		// Did we already add this timing?
		if (fabs(time - moveServices->m_arrForceSubtickMoveWhen[i]) < EPSILON)
		{
			return;
		}
		// Is this subtick value still valid? If not, we use this value.
		if (moveServices->m_arrForceSubtickMoveWhen[i] * ENGINE_FIXED_TICK_RATE < g_pKZUtils->GetServerGlobals()->tickcount - 1)
		{
			moveServices->m_arrForceSubtickMoveWhen[i] = time;
			return;
		}
		// Shift the later other subtick moves. We will lose the last one... oh well.
		if (time < moveServices->m_arrForceSubtickMoveWhen[i])
		{
			for (i32 j = i; j < 3; j++)
			{
				moveServices->m_arrForceSubtickMoveWhen[j + 1] = moveServices->m_arrForceSubtickMoveWhen[j];
			}
			moveServices->m_arrForceSubtickMoveWhen[i] = time;
			return;
		}
	}
}

void KZClassicModeService::InterpolateViewAngles()
{
	// Second half of the movement, no change.
	CGlobalVars *globals = g_pKZUtils->GetGlobals();
	f64 subtickFraction, whole;
	subtickFraction = modf((f64)globals->curtime * ENGINE_FIXED_TICK_RATE, &whole);
	if (subtickFraction < 0.001)
	{
		return;
	}
	
	// First half of the movement, tweak the angle to be the middle of the desired angle and the last angle
	QAngle newAngles = player->currentMoveData->m_vecViewAngles;
	QAngle oldAngles = this->hasValidDesiredViewAngle ? this->lastValidDesiredViewAngle : this->player->moveDataPost.m_vecViewAngles;
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
	if (g_pKZUtils->GetGlobals()->frametime > 0.0f)
	{
		this->hasValidDesiredViewAngle = true;
		this->lastValidDesiredViewAngle = player->currentMoveData->m_vecViewAngles;
	}
}

void KZClassicModeService::RemoveCrouchJumpBind()
{
	this->forcedUnduck = false;
	if (this->player->GetPawn()->m_fFlags & FL_ONGROUND && !this->oldDuckPressed && !this->player->GetMoveServices()->m_bOldJumpPressed && this->player->IsButtonPressed(IN_JUMP))
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
		if (this->angleHistory[i].when + PS_TURN_RATE_WINDOW < g_pKZUtils->GetGlobals()->curtime)
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
	angHist->when = g_pKZUtils->GetGlobals()->curtime;
	angHist->duration = g_pKZUtils->GetGlobals()->frametime;

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

	f32 rewardRate = Clamp(fabs(averageRate) / PS_MAX_REWARD_RATE, 0.0f, 1.0f) * g_pKZUtils->GetGlobals()->frametime;
	f32 punishRate = 0.0f;
	if (this->player->landingTime + PS_LANDING_GRACE_PERIOD < g_pKZUtils->GetGlobals()->curtime)
	{
		punishRate = g_pKZUtils->GetGlobals()->frametime * PS_DECREMENT_RATIO;
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
		rewardRate = g_pKZUtils->GetGlobals()->frametime;
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

internal void ClipVelocity(Vector &in, Vector &normal, Vector &out)
{
	// Determine how far along plane to slide based on incoming direction.
	f32 backoff = DotProduct(in, normal);

	for (i32 i = 0; i < 3; i++)
	{
		f32 change = normal[i] * backoff;
		out[i] = in[i] - change;
	}

	float adjust = -0.0078125f;
	out -= (normal * adjust);
}

internal bool IsValidMovementTrace(trace_t_s2 &tr, bbox_t bounds, CTraceFilterPlayerMovementCS *filter)
{
	trace_t_s2 stuck;
	// Maybe we don't need this one
	if (tr.fraction < FLT_EPSILON)
	{
		return false;
	}

	if (tr.startsolid)
	{
		return false;
	}

	// We hit something but no valid plane data?
	if (tr.fraction < 1.0f && fabs(tr.planeNormal.x) < FLT_EPSILON && fabs(tr.planeNormal.y) < FLT_EPSILON && fabs(tr.planeNormal.z) < FLT_EPSILON)
	{
		return false;
	}

	// Is the plane deformed?
	if (fabs(tr.planeNormal.x) > 1.0f || fabs(tr.planeNormal.y) > 1.0f || fabs(tr.planeNormal.z) > 1.0f)
	{
		return false;
	}

	// Do a swept trace and a backward trace just to be sure.
	g_pKZUtils->TracePlayerBBox(tr.endpos, tr.endpos, bounds, filter, stuck);
	if (stuck.startsolid || stuck.fraction < 1.0f - FLT_EPSILON)
	{
		return false;
	}

	g_pKZUtils->TracePlayerBBox(tr.endpos, tr.startpos, bounds, filter, stuck);
	if (stuck.startsolid || stuck.fraction < 1.0f - FLT_EPSILON)
	{
		return false;
	}

	return true;
}

void KZClassicModeService::OnTryPlayerMove(Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	this->tpmTriggerFixOrigins.RemoveAll();
	this->overrideTPM = false;
	this->didTPM = true;
	CCSPlayerPawn *pawn = this->player->GetPawn();

	f32 timeLeft = g_pKZUtils->GetGlobals()->frametime;

	Vector start, velocity, end;
	this->player->GetOrigin(&start);
	this->player->GetVelocity(&velocity);

	this->tpmTriggerFixOrigins.AddToTail(start);
	if (velocity.Length() == 0.0f)
	{
		// No move required.
		return;
	}
	Vector primalVelocity = velocity;
	bool validPlane{};

	f32 allFraction{};
	trace_t_s2 pm;
	u32 bumpCount{};
	Vector planes[5];
	u32 numPlanes{};

	bbox_t bounds;
	bounds.mins = { -16, -16, 0 };
	bounds.maxs = { 16, 16, 72 };

	if (this->player->GetMoveServices()->m_bDucked())
	{
		bounds.maxs.z = 54;
	}

	CTraceFilterPlayerMovementCS filter;
	g_pKZUtils->InitPlayerMovementTraceFilter(filter, pawn, pawn->m_Collision().m_collisionAttribute().m_nInteractsWith(), COLLISION_GROUP_PLAYER_MOVEMENT);

	for (bumpCount = 0; bumpCount < MAX_BUMPS; bumpCount++)
	{
		// Assume we can move all the way from the current origin to the end point.
		VectorMA(start, timeLeft, velocity, end);
		// See if we can make it from origin to end point.
		// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
		if (pFirstDest && end == *pFirstDest)
		{
			pm = *pFirstTrace;
		}
		else
		{
			g_pKZUtils->TracePlayerBBox(start, end, bounds, &filter, pm);
		}
		if (IsValidMovementTrace(pm, bounds, &filter) && pm.fraction == 1.0f)
		{
			// Player won't hit anything, nothing to do.
			break;
		}
		if (this->lastValidPlane.Length() > FLT_EPSILON && (!IsValidMovementTrace(pm, bounds, &filter) || pm.planeNormal.Dot(this->lastValidPlane) < RAMP_BUG_THRESHOLD))
		{
			// We hit a plane that will significantly change our velocity. Make sure that this plane is significant enough.
			Vector direction = velocity.Normalized();
			Vector offsetDirection;
			f32 offsets[] = {0.0f, -1.0f, 1.0f};
			bool success{};
			for (u32 i = 0; i < 3 && !success; i++)
			{
				for (u32 j = 0; j < 3 && !success; j++)
				{
					for (u32 k = 0; k < 3 && !success; k++)
					{
						if (i == 0 && j == 0 && k == 0)
						{
							offsetDirection = this->lastValidPlane;
						}
						else
						{
							offsetDirection = { offsets[i], offsets[j], offsets[k] };
							// Check if this random offset is even valid.
							trace_t_s2 test;
							g_pKZUtils->TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE, start, bounds, &filter, test);
							if (!IsValidMovementTrace(test, bounds, &filter))
							{
								continue;
							}
						}
						trace_t_s2 pierce;
						bool goodTrace{};
						f32 ratio{};
						bool hitNewPlane{};
						for (ratio = 0.05f; ratio <= 1.0f; ratio += 0.05f)
						{
							g_pKZUtils->TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE * ratio, end + offsetDirection * RAMP_PIERCE_DISTANCE * ratio, bounds, &filter, pierce);
							if (!IsValidMovementTrace(pierce, bounds, &filter))
							{
								continue;
							}
							// Try until we hit a similar plane.
							validPlane = pierce.fraction < 1.0f && pierce.fraction > 0.1f && pierce.planeNormal.Dot(this->lastValidPlane) >= RAMP_BUG_THRESHOLD;
							hitNewPlane = pm.planeNormal.Dot(pierce.planeNormal) < NEW_RAMP_THRESHOLD && this->lastValidPlane.Dot(pierce.planeNormal) > NEW_RAMP_THRESHOLD;
							goodTrace = CloseEnough(pierce.fraction, 1.0f, FLT_EPSILON) || validPlane;
							if (goodTrace)
							{
								break;
							}
						}
						if (goodTrace || hitNewPlane)
						{
							// Trace back to the original end point to find its normal.
							trace_t_s2 test;
							g_pKZUtils->TracePlayerBBox(pierce.endpos, end, bounds, &filter, test);
							pm = pierce;
							pm.startpos = start;
							pm.fraction = Clamp((pierce.endpos - pierce.startpos).Length() / (end - start).Length(), 0.0f, 1.0f);
							pm.endpos = test.endpos;
							pm.planeNormal = test.planeNormal;
							this->lastValidPlane = test.planeNormal;
							success = true;
							this->overrideTPM = true;
						}
					}
				}
			}
		}
		if (pm.planeNormal.Length() > 0.99f)
		{
			this->lastValidPlane = pm.planeNormal;
		}

		if (pm.fraction > 0.03125f)
		{
			allFraction += pm.fraction;
			start = pm.endpos;
			numPlanes = 0;
		}

		this->tpmTriggerFixOrigins.AddToTail(pm.endpos);

		if (allFraction == 1.0f)
		{
			break;
		}
		timeLeft -= g_pKZUtils->GetGlobals()->frametime * pm.fraction;
		planes[numPlanes] = pm.planeNormal;
		numPlanes++;
		if (numPlanes == 1 && pawn->m_MoveType() == MOVETYPE_WALK && pawn->m_hGroundEntity().Get() == nullptr)
		{
			ClipVelocity(velocity, planes[0], velocity);
		}
		else
		{
			u32 i, j;
			for (i = 0; i < numPlanes; i++)
			{
				ClipVelocity(velocity, planes[i], velocity);
				for (j = 0; j < numPlanes; j++)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (velocity.Dot(planes[j]) < 0)
							break; // not ok
					}
				}

				if (j == numPlanes) // Didn't have to clip, so we're ok
					break;
			}
			// Did we go all the way through plane set
			if (i != numPlanes)
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;
			}
			else
			{	// go along the crease
				if (numPlanes != 2)
				{
					VectorCopy(vec3_origin, velocity);
					break;
				}
				Vector dir;
				f32 d;
				CrossProduct(planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(velocity);
				VectorScale(dir, d, velocity);

				if (velocity.Dot(primalVelocity) <= 0)
				{
					velocity = vec3_origin;
					break;
				}
			}

		}
	}
	this->tpmOrigin = pm.endpos;
	this->tpmVelocity = velocity;
}

void KZClassicModeService::OnTryPlayerMovePost(Vector *pFirstDest, trace_t_s2 *pFirstTrace)
{
	if (this->overrideTPM)
	{
		this->player->SetOrigin(this->tpmOrigin);
		this->player->SetVelocity(this->tpmVelocity);
	}
	if (this->airMoving)
	{
		if (this->tpmTriggerFixOrigins.Count() > 1)
		{
			bbox_t bounds;
			this->player->GetBBoxBounds(&bounds);
			for (int i = 0; i < this->tpmTriggerFixOrigins.Count() - 1; i++)
			{
				this->player->TouchTriggersAlongPath(this->tpmTriggerFixOrigins[i], this->tpmTriggerFixOrigins[i + 1], bounds);
			}
		}
		this->player->UpdateTriggerTouchList();
	}
}

void KZClassicModeService::OnDuckPost()
{
	this->player->UpdateTriggerTouchList();
}

void KZClassicModeService::OnAirMove()
{
	this->airMoving = true;
}

void KZClassicModeService::OnAirMovePost()
{
	this->airMoving = false;
}

void KZClassicModeService::OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
{
	if (!this->player->processingMovement)
	{
		return;
	}
	// Only happens when triggerfix happens.
	if (newPosition)
	{
		this->player->currentMoveData->m_vecAbsOrigin = *newPosition;
	}
	if (newVelocity)
	{
		this->player->currentMoveData->m_vecVelocity = *newVelocity;
	}
}

// Only touch timer triggers on half ticks.
bool KZClassicModeService::OnTriggerStartTouch(CBaseTrigger *trigger)
{
	if (!trigger->IsEndZone() && !trigger->IsStartZone())
	{
		return true;
	}
	f64 tick = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_INTERVAL;
	if (fabs(roundf(tick) - tick) < 0.001f || fabs(roundf(tick) - tick - 0.5f) < 0.001f)
	{
		return true;
	}

	return false;
}

bool KZClassicModeService::OnTriggerTouch(CBaseTrigger *trigger)
{
	if (!trigger->IsEndZone() && !trigger->IsStartZone())
	{
		return true;
	}
	f64 tick = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_INTERVAL;
	if (fabs(roundf(tick) - tick) < 0.001f || fabs(roundf(tick) - tick - 0.5f) < 0.001f)
	{
		return true;
	}
	return false;
}

bool KZClassicModeService::OnTriggerEndTouch(CBaseTrigger *trigger)
{
	if (!trigger->IsStartZone())
	{
		return true;
	}
	f64 tick = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_INTERVAL;
	if (fabs(roundf(tick) - tick) < 0.001f || fabs(roundf(tick) - tick - 0.5f) < 0.001f)
	{
		return true;
	}
	return false;
}
