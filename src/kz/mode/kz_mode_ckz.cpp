#include "cs_usercmd.pb.h"
#include "kz_mode_ckz.h"
#include "utils/addresses.h"
#include "utils/interfaces.h"
#include "utils/gameconfig.h"
#include "sdk/usercmd.h"
#include "sdk/tracefilter.h"
#include "sdk/navphysicsinterface.h"
#include "sdk/entity/cbasetrigger.h"

KZClassicModePlugin g_KZClassicModePlugin;

CGameConfig *g_pGameConfig = NULL;
KZUtils *g_pKZUtils = NULL;
KZModeManager *g_pModeManager = NULL;
MappingInterface *g_pMappingApi = NULL;
ModeServiceFactory g_ModeFactory = [](KZPlayer *player) -> KZModeService * { return new KZClassicModeService(player); };
PLUGIN_EXPOSE(KZClassicModePlugin, g_KZClassicModePlugin);

CConVarRef<f32> sv_standable_normal("sv_standable_normal");

// The player that is currently running TryPlayerMove or CategorizePosition.
static_global KZClassicModeService *tracingModeService {};

// Every hull trace done by the game's movement code goes through this function.
static KHook::Return<void> TracePlayerBBoxPost(void *traceCache, trace_t *pm, const Vector *start, const Vector *end, const bbox_t *bounds,
											   CTraceFilter *filter)
{
	if (tracingModeService)
	{
		tracingModeService->OnTracePlayerBBoxPost(Ray_t(bounds->mins, bounds->maxs), *start, *end, filter, pm);
	}
	return {KHook::Action::Ignore};
}

static KHook::Function<void, void *, trace_t *, const Vector *, const Vector *, const bbox_t *, CTraceFilter *> TracePlayerBBox(nullptr,
																																TracePlayerBBoxPost);

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
	g_pMappingApi = (MappingInterface *)g_SMAPI->MetaFactory(KZ_MAPPING_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_MAPPING_INTERFACE);
		return false;
	}
	modules::Initialize();
	if (!interfaces::Initialize(ismm, error, maxlen))
	{
		V_snprintf(error, maxlen, "Failed to initialize interfaces");
		return false;
	}

	if (nullptr == (g_pGameConfig = g_pKZUtils->GetGameConfig()))
	{
		V_snprintf(error, maxlen, "Failed to get game config");
		return false;
	}

	void *tracePlayerBBoxAddress = g_pGameConfig->ResolveSignature("TracePlayerBBox");
	if (!tracePlayerBBoxAddress)
	{
		V_snprintf(error, maxlen, "Failed to resolve signature: TracePlayerBBox");
		return false;
	}
	TracePlayerBBox.Configure(tracePlayerBBoxAddress);

	if (!g_pModeManager->RegisterMode(g_PLID, MODE_NAME_SHORT, MODE_NAME, g_ModeFactory))
	{
		V_snprintf(error, maxlen, "Failed to register mode");
		return false;
	}

	META_CONVAR_REGISTER(FCVAR_NONE);
	return true;
}

bool KZClassicModePlugin::Unload(char *error, size_t maxlen)
{
	g_pModeManager->UnregisterMode(g_PLID);
	KZ::mode::CleanupModeCvarRefs();
	modules::Cleanup();
	return true;
}

bool KZClassicModePlugin::Pause(char *error, size_t maxlen)
{
	g_pModeManager->UnregisterMode(g_PLID);
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

CGameEntitySystem *GameEntitySystem()
{
	return g_pKZUtils->GetGameEntitySystem();
}

/*
	Actual mode stuff.
*/

void KZClassicModeService::Reset()
{
	this->hasValidDesiredViewAngle = {};
	this->lastValidDesiredViewAngle = vec3_angle;
	this->lastJumpReleaseTime = {};
	this->oldDuckPressed = {};
	this->forcedUnduck = {};
	this->postProcessMovementZSpeed = {};

	this->angleHistory.RemoveAll();
	this->leftPreRatio = {};
	this->rightPreRatio = {};
	this->bonusSpeed = {};
	this->maxPre = {};

	this->inTryPlayerMove = {};
	this->lastPlane = vec3_origin;
	this->stuckTraceCount = {};

	this->airMoving = {};
	this->tpmTriggerFixOrigins.RemoveAll();
}

void KZClassicModeService::Cleanup()
{
	if (tracingModeService == this)
	{
		tracingModeService = nullptr;
	}
	auto pawn = this->player->GetPlayerPawn();
	if (pawn)
	{
		pawn->m_flVelocityModifier(1.0f);
	}
}

const char *KZClassicModeService::GetModeName()
{
	return MODE_NAME;
}

const char *KZClassicModeService::GetModeShortName()
{
	return MODE_NAME_SHORT;
}

bool KZClassicModeService::EnableWaterFix()
{
	return this->player->IsButtonPressed(IN_JUMP);
}

DistanceTier KZClassicModeService::GetDistanceTier(JumpType jumpType, f32 distance)
{
	// No tiers given for 'Invalid' jumps.
	if (jumpType == JumpType_Invalid || jumpType == JumpType_FullInvalid || jumpType == JumpType_Fall || jumpType == JumpType_Other
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

const CVValue_t *KZClassicModeService::GetModeConVarValues()
{
	return modeCvarValues;
}

void KZClassicModeService::OnStopTouchGround()
{
	if (this->player->GetMoveType() != MOVETYPE_WALK)
	{
		return;
	}
	Vector velocity;
	this->player->GetVelocity(&velocity);
	f32 speed = velocity.Length2D();

	f32 timeOnGround = this->player->takeoffTime - this->player->landingTime;
	// Perf
	if (timeOnGround <= BH_PERF_WINDOW && !this->player->possibleLadderHop)
	{
		this->player->inPerf = true;
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
	// Shrink the bounds here as well, otherwise we touch triggers that are flush with the ground.
	bbox_t offset = {{0.03125f, 0.03125f, 0.03125f}, {-0.03125f, -0.03125f, -0.03125f}};
	this->player->GetBBoxBounds(&bounds, &offset);
	Vector ground = this->player->landingOrigin;
	ground.z = this->player->GetGroundPosition();
	this->player->TouchTriggersAlongPath(this->player->landingOrigin, ground, bounds);
}

void KZClassicModeService::OnPhysicsSimulate()
{
	CCSPlayer_MovementServices *moveServices = this->player->GetMoveServices();
	if (!moveServices)
	{
		return;
	}
	u32 tickCount = g_pKZUtils->GetServerGlobals()->tickcount;

	f32 subtickMoveTime = (tickCount - 0.5) * ENGINE_FIXED_TICK_INTERVAL;
	for (u32 i = 0; i < 4; i++)
	{
		if (fabs(subtickMoveTime - moveServices->m_arrForceSubtickMoveWhen[i]) < 0.001)
		{
			return;
		}
		if (subtickMoveTime > moveServices->m_arrForceSubtickMoveWhen[i])
		{
			moveServices->SetForcedSubtickMove(i, subtickMoveTime, false);
			return;
		}
	}
}

void KZClassicModeService::OnPhysicsSimulatePost()
{
	CCSPlayer_MovementServices *moveServices = this->player->GetMoveServices();
	if (!moveServices)
	{
		return;
	}
	u32 tickCount = g_pKZUtils->GetServerGlobals()->tickcount;

	f32 subtickMoveTime = (tickCount + 0.5) * ENGINE_FIXED_TICK_INTERVAL;
	for (u32 i = 0; i < 4; i++)
	{
		if (fabs(subtickMoveTime - moveServices->m_arrForceSubtickMoveWhen[i]) < 0.001)
		{
			subtickMoveTime += ENGINE_FIXED_TICK_INTERVAL;
			continue;
		}
		if (subtickMoveTime > moveServices->m_arrForceSubtickMoveWhen[i])
		{
			moveServices->SetForcedSubtickMove(i, subtickMoveTime);
			subtickMoveTime += ENGINE_FIXED_TICK_INTERVAL;
		}
	}
}

void KZClassicModeService::OnSetupMove(PlayerCommand *pc)
{
	for (i32 j = 0; j < pc->mutable_base()->subtick_moves_size(); j++)
	{
		CSubtickMoveStep *subtickMove = pc->mutable_base()->mutable_subtick_moves(j);
		if (subtickMove->button() == IN_ATTACK || subtickMove->button() == IN_ATTACK2 || subtickMove->button() == IN_RELOAD)
		{
			continue;
		}
		float when = subtickMove->when();
		if (subtickMove->button() == IN_JUMP)
		{
			f32 inputTime = (g_pKZUtils->GetGlobals()->tickcount + when - 1) * ENGINE_FIXED_TICK_INTERVAL;
			if (when != 0)
			{
				if (subtickMove->pressed() && inputTime - this->lastJumpReleaseTime > 0.5 * ENGINE_FIXED_TICK_INTERVAL)
				{
					this->player->GetMoveServices()->m_LegacyJump().m_bOldJumpPressed = false;
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

void KZClassicModeService::OnProcessMovement()
{
	if (this->player->GetPlayerPawn()->m_flVelocityModifier() != 1.0f)
	{
		this->player->GetPlayerPawn()->m_flVelocityModifier(1.0f);
	}
	this->CheckVelocityQuantization();
	this->RemoveCrouchJumpBind();
	this->ReduceDuckSlowdown();
	this->InterpolateViewAngles();
	this->UpdateAngleHistory();
	this->CalcPrestrafe();
}

void KZClassicModeService::OnPlayerMove()
{
	this->originalMaxSpeed = this->player->currentMoveData->m_flMaxSpeed;
	this->player->currentMoveData->m_flMaxSpeed = SPEED_NORMAL + this->GetPrestrafeGain();
}

void KZClassicModeService::OnProcessMovementPost()
{
	this->player->UpdateTriggerTouchList();
	this->RestoreInterpolatedViewAngles();
	this->oldDuckPressed = this->forcedUnduck || this->player->IsButtonPressed(IN_DUCK, true);
	this->oldJumpPressed = this->player->IsButtonPressed(IN_JUMP);
	Vector velocity;
	this->player->GetVelocity(&velocity);
	this->postProcessMovementZSpeed = velocity.z;
	f32 velMod = this->originalMaxSpeed >= 0 ? (SPEED_NORMAL + this->GetPrestrafeGain()) / this->originalMaxSpeed : 1.0f;
	if (this->player->GetPlayerPawn()->m_flVelocityModifier() != velMod)
	{
		this->player->GetPlayerPawn()->m_flVelocityModifier(velMod);
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

	bool onGround = this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND;
	bool justJumped = !this->oldJumpPressed && this->player->IsButtonPressed(IN_JUMP);

	if (onGround && !this->oldDuckPressed && justJumped)
	{
		this->player->GetMoveServices()->m_nButtons().m_pButtonStates[0] &= ~IN_DUCK;
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
	if ((this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND) == 0)
	{
		return;
	}

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
		forward = g_pKZUtils->NormalizeVector(forward);
	}

	if (right[2] != 0)
	{
		right[2] = 0;
		right = g_pKZUtils->NormalizeVector(right);
	}

	Vector wishdir;
	for (int i = 0; i < 2; i++)
	{
		wishdir[i] = forward[i] * fmove + right[i] * smove;
	}
	wishdir[2] = 0;

	wishdir = g_pKZUtils->NormalizeVector(wishdir);

	if (wishdir.Length() == 0)
	{
		angHist->rate = 0;
		return;
	}

	Vector velocity = mv->m_vecVelocity;
	velocity[2] = 0;
	velocity = g_pKZUtils->NormalizeVector(velocity);
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
	if (totalDuration == 0)
	{
		averageRate = 0;
	}
	else
	{
		averageRate = sumWeightedAngles / totalDuration;
	}

	f32 rewardRate = Clamp(fabs(averageRate) / PS_MAX_REWARD_RATE, 0.0f, 1.0f) * g_pKZUtils->GetGlobals()->frametime;
	f32 punishRate = 0.0f;
	if (this->player->landingTime + PS_LANDING_GRACE_PERIOD < g_pKZUtils->GetGlobals()->curtime)
	{
		punishRate = g_pKZUtils->GetGlobals()->frametime * PS_DECREMENT_RATIO;
	}

	bool perfEligible = g_pKZUtils->GetGlobals()->curtime - g_pKZUtils->GetGlobals()->frametime - this->player->landingTime <= BH_PERF_WINDOW
						&& !this->player->possibleLadderHop;

	if ((this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND) && !perfEligible)
	{
		// Prevent instant full pre from crouched prestrafe.
		Vector velocity;
		this->player->GetVelocity(&velocity);
		f32 speed = Clamp(velocity.Length2D(), 0.0f, SPEED_NORMAL);

		f32 currentPreRatio;
		if (speed <= 0.0f)
		{
			currentPreRatio = 0.0f;
		}
		else
		{
			currentPreRatio = pow(this->bonusSpeed / PS_SPEED_MAX * SPEED_NORMAL / speed, 1 / PS_RATIO_TO_SPEED) * PS_MAX_PS_TIME;
		}

		this->leftPreRatio = MIN(this->leftPreRatio, currentPreRatio);
		this->rightPreRatio = MIN(this->rightPreRatio, currentPreRatio);

		this->leftPreRatio += averageRate > PS_MIN_REWARD_RATE ? rewardRate : -punishRate;
		this->rightPreRatio += averageRate < -PS_MIN_REWARD_RATE ? rewardRate : -punishRate;
		this->leftPreRatio = Clamp(leftPreRatio, 0.0f, PS_MAX_PS_TIME);
		this->rightPreRatio = Clamp(rightPreRatio, 0.0f, PS_MAX_PS_TIME);
		this->bonusSpeed = this->GetPrestrafeGain() / SPEED_NORMAL * speed;
	}
	else
	{
		rewardRate = g_pKZUtils->GetGlobals()->frametime;
		// Raise both left and right pre to the same value as the player is in the air.
		if (this->leftPreRatio < this->rightPreRatio)
		{
			this->leftPreRatio = Clamp(this->leftPreRatio + rewardRate, 0.0f, rightPreRatio);
		}
		else
		{
			this->rightPreRatio = Clamp(this->rightPreRatio + rewardRate, 0.0f, leftPreRatio);
		}
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
	CTraceFilterPlayerMovementCS filter(this->player->GetPlayerPawn());

	Vector ground = this->player->currentMoveData->m_vecAbsOrigin;
	ground.z -= 2;

	f32 standableZ = 0.7f; // Equal to the mode's cvar.

	if (sv_standable_normal.IsValidRef() && sv_standable_normal.IsConVarDataAvailable())
	{
		standableZ = sv_standable_normal.Get();
	}
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);
	trace_t trace;

	INavPhysicsInterface::TraceShape(Ray_t(bounds.mins, bounds.maxs), this->player->currentMoveData->m_vecAbsOrigin, ground, &filter, &trace);

	// Doesn't hit anything, fall back to the original ground
	if (trace.m_bStartInSolid || trace.m_flFraction == 1.0f)
	{
		return;
	}

	if (standableZ <= trace.m_vHitNormal.z && trace.m_vHitNormal.z < 1.0f)
	{
		// Copy the ClipVelocity function from sdk2013
		float backoff;
		float change;
		Vector newVelocity;

		backoff = DotProduct(this->player->landingVelocity, trace.m_vHitNormal) * 1;

		for (u32 i = 0; i < 3; i++)
		{
			change = trace.m_vHitNormal[i] * backoff;
			newVelocity[i] = this->player->landingVelocity[i] - change;
		}

		f32 adjust = DotProduct(newVelocity, trace.m_vHitNormal);
		if (adjust < 0.0f)
		{
			newVelocity -= (trace.m_vHitNormal * adjust);
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

void KZClassicModeService::OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing)
{
	tracingModeService = this;
	this->inTryPlayerMove = true;
	this->stuckTraceCount = 0;
	// The rest of the path is added by FixTryPlayerMoveTrace.
	Vector origin;
	this->player->GetOrigin(&origin);
	this->tpmTriggerFixOrigins.RemoveAll();
	this->tpmTriggerFixOrigins.AddToTail(origin);

	// WalkMove does the first trace before calling TryPlayerMove, so we need to fix that one here.
	// TryPlayerMove only uses it if it ends where its own first trace would end.
	if (pFirstDest && pFirstTrace && (this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND))
	{
		Vector velocity, end;
		this->player->GetVelocity(&velocity);
		VectorMA(origin, g_pKZUtils->GetGlobals()->frametime, velocity, end);
		if (end == *pFirstDest)
		{
			bbox_t bounds;
			this->player->GetBBoxBounds(&bounds);
			CTraceFilterPlayerMovementCS filter(this->player->GetPlayerPawn());
			this->FixTryPlayerMoveTrace(Ray_t(bounds.mins, bounds.maxs), origin, end, &filter, pFirstTrace);
		}
	}
}

void KZClassicModeService::OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing)
{
	tracingModeService = nullptr;
	this->inTryPlayerMove = false;
	if (this->airMoving)
	{
		if (this->tpmTriggerFixOrigins.Count() > 1)
		{
			bbox_t bounds;
			// We need to shrink the bounds a bit to prevent touching triggers that we shouldn't be touching when doing triggerfix.
			bbox_t offset = {{0.03125f, 0.03125f, 0.03125f}, {-0.03125f, -0.03125f, -0.03125f}};
			this->player->GetBBoxBounds(&bounds, &offset);
			for (int i = 0; i < this->tpmTriggerFixOrigins.Count() - 1; i++)
			{
				this->player->TouchTriggersAlongPath(this->tpmTriggerFixOrigins[i], this->tpmTriggerFixOrigins[i + 1], bounds);
			}
		}
		this->player->UpdateTriggerTouchList();
	}
}

void KZClassicModeService::OnCategorizePosition(bool bStayOnGround)
{
	tracingModeService = this;
}

void KZClassicModeService::OnCategorizePositionPost(bool bStayOnGround)
{
	tracingModeService = nullptr;
}

void KZClassicModeService::OnTracePlayerBBoxPost(const Ray_t &ray, const Vector &start, const Vector &end, CTraceFilter *filter, trace_t *pm)
{
	if (this->inTryPlayerMove)
	{
		this->FixTryPlayerMoveTrace(ray, start, end, filter, pm);
	}
	else
	{
		this->FixGroundTrace(ray, start, end, filter, pm);
	}
}

/*
	Rampbug fix:
	The game keeps the player 1/32 unit away from whatever they collide with.
	While sliding on a ramp, the player can end up closer to the ramp than that, or collide with the edge between two triangles of the ramp
	instead of the ramp itself, which gives a wrong normal. TryPlayerMove then either stops the player or sends them in a random direction.

	To fix this, we redo the trace a bit further away from the ramp. We only use that trace if it actually goes further than the original one.
	If it just ends up on another plane right away, the player is really running into a corner and the original trace is kept.
*/
void KZClassicModeService::FixTryPlayerMoveTrace(const Ray_t &ray, const Vector &start, const Vector &end, CTraceFilter *filter, trace_t *pm)
{
	Vector direction = end - start;
	f32 length = direction.NormalizeInPlace();
	// We hit a plane that will change our velocity, or we barely moved at all. This is either a real obstacle or a rampbug.
	bool shouldConsiderRampbug = pm->m_flFraction < 1.0f && !pm->m_bStartInSolid && this->lastPlane.LengthSqr() > 0.0f && length > 0.0f
								 && (pm->m_vHitNormal.Dot(this->lastPlane) < RAMP_BUG_THRESHOLD || pm->m_flFraction * length < RAMP_BUG_OFFSET);
	if (shouldConsiderRampbug)
	{
		bool success {};
		// We already failed to move twice, so TryPlayerMove is about to stop the player. Use any trace that goes further.
		bool stuck = this->stuckTraceCount >= 2;
		// This can only be a rampbug if the player is still right next to the last plane they hit.
		trace_t test;
		INavPhysicsInterface::TraceShape(ray, pm->m_vEndPos + this->lastPlane * RAMP_BUG_OFFSET,
										 pm->m_vEndPos - this->lastPlane * (RAMP_BUG_OFFSET * 2.0f), filter, &test);
		if (test.m_flFraction < 1.0f && !test.m_bStartInSolid && test.m_vHitNormal.Dot(this->lastPlane) >= RAMP_BUG_THRESHOLD)
		{
			// Redo the trace a bit further away from the plane.
			Vector offset = this->lastPlane * RAMP_BUG_OFFSET;
			trace_t offsetTrace;
			INavPhysicsInterface::TraceShape(ray, start + offset, end + offset, filter, &offsetTrace);
			// If this trace only goes a bit further and then hits a different plane, the original trace hit something real,
			// like a corner or two ramps meeting each other. Keep the original so TryPlayerMove can slide along both planes.
			f32 distance = (offsetTrace.m_vEndPos - pm->m_vEndPos).Dot(direction);
			bool samePlane = offsetTrace.m_vHitNormal.Dot(this->lastPlane) >= RAMP_BUG_SAME_PLANE;
			success = !offsetTrace.m_bStartInSolid && distance > RAMP_BUG_OFFSET
					  && (stuck || offsetTrace.m_flFraction == 1.0f || distance > RAMP_BUG_MIN_DISTANCE || samePlane);
			if (success)
			{
				*pm = offsetTrace;
			}
		}
		if (!success && stuck && pm->m_flFraction * length < RAMP_BUG_OFFSET)
		{
			// The player is stuck between two planes, moving away from one of them will just push them into the other one.
			// A trace only gives us one of the two planes, so we find the other one by tracing away from the first.
			Vector normal = pm->m_vHitNormal;
			Vector otherNormal = vec3_origin;
			if (normal.Dot(this->lastPlane) < RAMP_BUG_THRESHOLD)
			{
				otherNormal = this->lastPlane;
			}
			else
			{
				INavPhysicsInterface::TraceShape(ray, start, start + normal * (RAMP_BUG_OFFSET * 2.0f), filter, &test);
				if (test.m_flFraction < 1.0f && !test.m_bStartInSolid && test.m_vHitNormal.Dot(normal) < RAMP_BUG_THRESHOLD)
				{
					otherNormal = test.m_vHitNormal;
				}
			}
			if (otherNormal.LengthSqr() > 0.0f && normal.Dot(otherNormal) > -0.99f)
			{
				// Move away from both planes at once.
				Vector offset = normal + otherNormal;
				offset.NormalizeInPlace();
				offset *= MIN(RAMP_BUG_OFFSET / normal.Dot(offset), 1.0f);
				trace_t offsetTrace;
				INavPhysicsInterface::TraceShape(ray, start + offset, end + offset, filter, &offsetTrace);
				if (!offsetTrace.m_bStartInSolid && (offsetTrace.m_vEndPos - pm->m_vEndPos).Dot(direction) > RAMP_BUG_OFFSET)
				{
					*pm = offsetTrace;
				}
			}
		}
	}
	this->FixGroundTrace(ray, start, end, filter, pm);
	if (pm->m_flFraction < 1.0f && !pm->m_bStartInSolid)
	{
		if (pm->m_vHitNormal.LengthSqr() > 0.98f)
		{
			this->lastPlane = pm->m_vHitNormal;
		}
		if (pm->m_flFraction * length < RAMP_BUG_OFFSET)
		{
			this->stuckTraceCount++;
		}
	}
	// Triggerfix related
	this->tpmTriggerFixOrigins.AddToTail(pm->m_vEndPos);
}

// Sometimes the game thinks the player is on standable ground while they are actually on a steep slope right below a wall.
// A slightly smaller hull doesn't have this problem and will hit the slope instead.
// It will also hit a slope if the player is standing over a gap between two floors though, so we make sure there's a wall next to the player.
void KZClassicModeService::FixGroundTrace(const Ray_t &ray, const Vector &start, const Vector &end, CTraceFilter *filter, trace_t *pm)
{
	f32 standableZ = KZ::mode::modeCvarRefs[MODECVAR_SV_STANDABLE_NORMAL]->GetFloat();
	if (pm->m_flFraction == 1.0f || pm->m_bStartInSolid || pm->m_vHitNormal.z < standableZ)
	{
		return;
	}
	Vector shrink(GROUND_CHECK_SHRINK_SIZE, GROUND_CHECK_SHRINK_SIZE, 0.0f);
	trace_t smallTrace;
	INavPhysicsInterface::TraceShape(Ray_t(ray.m_Hull.m_vMins + shrink, ray.m_Hull.m_vMaxs - shrink), start, end, filter, &smallTrace);
	if (smallTrace.m_flFraction == 1.0f || smallTrace.m_bStartInSolid || smallTrace.m_vHitNormal.z >= standableZ)
	{
		return;
	}
	// Is there a wall on the side that the slope goes up towards?
	Vector wallDirection(-smallTrace.m_vHitNormal.x, -smallTrace.m_vHitNormal.y, 0.0f);
	wallDirection.NormalizeInPlace();
	trace_t wallTrace;
	INavPhysicsInterface::TraceShape(ray, pm->m_vEndPos, pm->m_vEndPos + wallDirection * GROUND_CHECK_SHRINK_SIZE, filter, &wallTrace);
	if (wallTrace.m_flFraction < 0.5f && !wallTrace.m_bStartInSolid)
	{
		pm->m_vHitNormal = smallTrace.m_vHitNormal;
	}
}

void KZClassicModeService::OnDuckPost()
{
	this->player->UpdateTriggerTouchList();
}

void KZClassicModeService::OnAirMove()
{
	this->airMoving = true;
	this->player->currentMoveData->m_flMaxSpeed = SPEED_NORMAL;
}

void KZClassicModeService::OnAirMovePost()
{
	this->airMoving = false;
	this->player->currentMoveData->m_flMaxSpeed = SPEED_NORMAL + this->GetPrestrafeGain();
}

void KZClassicModeService::OnWaterMove()
{
	this->player->currentMoveData->m_flMaxSpeed = SPEED_NORMAL;
}

void KZClassicModeService::OnWaterMovePost()
{
	this->player->currentMoveData->m_flMaxSpeed = SPEED_NORMAL + this->GetPrestrafeGain();
}

void KZClassicModeService::OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
{
	// Prevent prekeep from teleporting the player to a lower speed.
	if (newVelocity)
	{
		f32 speed = Clamp(newVelocity->Length2D(), 0.0f, SPEED_NORMAL);

		f32 maxPreRatio;
		if (speed <= 0.0f)
		{
			maxPreRatio = 0.0f;
		}
		else
		{
			maxPreRatio = pow(this->bonusSpeed / PS_SPEED_MAX * SPEED_NORMAL / speed, 1 / PS_RATIO_TO_SPEED) * PS_MAX_PS_TIME;
		}

		this->leftPreRatio = MIN(this->leftPreRatio, maxPreRatio);
		this->rightPreRatio = MIN(this->rightPreRatio, maxPreRatio);
		this->leftPreRatio = Clamp(leftPreRatio, 0.0f, PS_MAX_PS_TIME);
		this->rightPreRatio = Clamp(rightPreRatio, 0.0f, PS_MAX_PS_TIME);
		this->bonusSpeed = this->GetPrestrafeGain() / SPEED_NORMAL * speed;
	}
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

bool KZClassicModeService::CanTouchTimerZone()
{
	f64 tick = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_RATE;
	return fabs(roundf(tick) - tick) < 0.001f || fabs(roundf(tick) - tick - 0.5f) < 0.001f;
}

// Only touch timer triggers on half ticks.
bool KZClassicModeService::OnTriggerStartTouch(CBaseTrigger *trigger)
{
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	return this->CanTouchTimerZone();
}

bool KZClassicModeService::OnTriggerTouch(CBaseTrigger *trigger)
{
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	return this->CanTouchTimerZone();
}

bool KZClassicModeService::OnTriggerEndTouch(CBaseTrigger *trigger)
{
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	return this->CanTouchTimerZone();
}
