#include "cs_usercmd.pb.h"
#include "kz_mode_ckz.h"
#include "utils/addresses.h"
#include "utils/interfaces.h"
#include "utils/gameconfig.h"
#include "sdk/usercmd.h"
#include "sdk/tracefilter.h"
#include "sdk/entity/cbasetrigger.h"

KZClassicModePlugin g_KZClassicModePlugin;

CGameConfig *g_pGameConfig = NULL;
KZUtils *g_pKZUtils = NULL;
KZModeManager *g_pModeManager = NULL;
MappingInterface *g_pMappingApi = NULL;
ModeServiceFactory g_ModeFactory = [](KZPlayer *player) -> KZModeService * { return new KZClassicModeService(player); };
PLUGIN_EXPOSE(KZClassicModePlugin, g_KZClassicModePlugin);

CConVarRef<f32> sv_standable_normal("sv_standable_normal");

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

	if (!g_pModeManager->RegisterMode(g_PLID, MODE_NAME_SHORT, MODE_NAME, g_ModeFactory))
	{
		V_snprintf(error, maxlen, "Failed to register mode");
		return false;
	}

	ConVar_Register();
	return true;
}

bool KZClassicModePlugin::Unload(char *error, size_t maxlen)
{
	g_pModeManager->UnregisterMode(g_PLID);
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

	this->didTPM = {};
	this->overrideTPM = {};
	this->tpmVelocity = vec3_origin;
	this->tpmOrigin = vec3_origin;
	this->lastValidPlane = vec3_origin;

	this->airMoving = {};
	this->tpmTriggerFixOrigins.RemoveAll();
}

void KZClassicModeService::Cleanup()
{
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
	this->player->GetBBoxBounds(&bounds);
	Vector ground = this->player->landingOrigin;
	ground.z = this->player->GetGroundPosition() - 0.03125f;
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
	this->didTPM = false;
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
	if (!this->didTPM)
	{
		this->lastValidPlane = vec3_origin;
	}
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
		VectorNormalize(forward);
	}

	if (right[2] != 0)
	{
		right[2] = 0;
		VectorNormalize(right);
	}

	Vector wishdir;
	for (int i = 0; i < 2; i++)
	{
		wishdir[i] = forward[i] * fmove + right[i] * smove;
	}
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

	if (this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND)
	{
		// Prevent instant full pre from crouched prestrafe.
		Vector velocity;
		this->player->GetVelocity(&velocity);

		f32 currentPreRatio;
		if (velocity.Length2D() <= 0.0f)
		{
			currentPreRatio = 0.0f;
		}
		else
		{
			currentPreRatio = pow(this->bonusSpeed / PS_SPEED_MAX * SPEED_NORMAL / velocity.Length2D(), 1 / PS_RATIO_TO_SPEED) * PS_MAX_PS_TIME;
		}

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

	g_pKZUtils->TracePlayerBBox(this->player->currentMoveData->m_vecAbsOrigin, ground, bounds, &filter, trace);

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

// 1:1 with CS2.
static_function void ClipVelocity(Vector &in, Vector &normal, Vector &out)
{
	f32 backoff = -((in.x * normal.x) + ((normal.z * in.z) + (in.y * normal.y))) * 1;
	backoff = fmaxf(backoff, 0.0) + 0.03125;

	out = normal * backoff + in;
}

static_function bool IsValidMovementTrace(trace_t &tr, bbox_t bounds, CTraceFilterPlayerMovementCS *filter)
{
	trace_t stuck;
	// Maybe we don't need this one.
	// if (tr.m_flFraction < FLT_EPSILON)
	//{
	//	return false;
	//}

	if (tr.m_bStartInSolid)
	{
		return false;
	}

	// We hit something but no valid plane data?
	if (tr.m_flFraction < 1.0f && fabs(tr.m_vHitNormal.x) < FLT_EPSILON && fabs(tr.m_vHitNormal.y) < FLT_EPSILON
		&& fabs(tr.m_vHitNormal.z) < FLT_EPSILON)
	{
		return false;
	}

	// Is the plane deformed?
	if (fabs(tr.m_vHitNormal.x) > 1.0f || fabs(tr.m_vHitNormal.y) > 1.0f || fabs(tr.m_vHitNormal.z) > 1.0f)
	{
		return false;
	}

	// Do an unswept trace and a backward trace just to be sure.
	g_pKZUtils->TracePlayerBBox(tr.m_vEndPos, tr.m_vEndPos, bounds, filter, stuck);
	if (stuck.m_bStartInSolid || stuck.m_flFraction < 1.0f - FLT_EPSILON)
	{
		return false;
	}

	g_pKZUtils->TracePlayerBBox(tr.m_vEndPos, tr.m_vStartPos, bounds, filter, stuck);
	// For whatever reason if you can hit something in only one direction and not the other way around.
	// Only happens since Call to Arms update, so this fraction check is commented out until it is fixed.
	if (stuck.m_bStartInSolid /*|| stuck.m_flFraction < 1.0f - FLT_EPSILON*/)
	{
		return false;
	}

	return true;
}

void KZClassicModeService::OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing)
{
	this->tpmTriggerFixOrigins.RemoveAll();
	this->overrideTPM = false;
	this->didTPM = true;
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();

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

	bool validPlane {};

	f32 allFraction {};
	trace_t pm;
	u32 bumpCount {};
	Vector planes[5];
	u32 numPlanes {};
	trace_t pierce;

	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);

	CTraceFilterPlayerMovementCS filter(pawn);

	bool potentiallyStuck {};

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
			if (end == start)
			{
				continue;
			}
			if (IsValidMovementTrace(pm, bounds, &filter) && pm.m_flFraction == 1.0f)
			{
				// Player won't hit anything, nothing to do.
				break;
			}
			bool normalChanged = pm.m_vHitNormal.Dot(this->lastValidPlane) < RAMP_BUG_THRESHOLD;
			bool stuck = potentiallyStuck && pm.m_flFraction == 0.0f;
			bool lastValidPlaneWasStraightWall = this->lastValidPlane.z < 0.03125f;
			bool shouldConsiderRampbug = (normalChanged && !lastValidPlaneWasStraightWall) || stuck;
			if (this->lastValidPlane.Length() > FLT_EPSILON && shouldConsiderRampbug)
			{
				// We hit a plane that will significantly change our velocity.
				// Make sure that this plane is significant enough.
				Vector direction = velocity.Normalized();
				Vector offsetDirection;
				f32 offsets[] = {0.0f, -1.0f, 1.0f};
				bool success {};
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
								offsetDirection = {offsets[i], offsets[j], offsets[k]};
								// Check if this random offset is even valid.
								if (this->lastValidPlane.Dot(offsetDirection) <= 0.0f)
								{
									continue;
								}
								trace_t test;
								g_pKZUtils->TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE, start, bounds, &filter, test);
								if (!IsValidMovementTrace(test, bounds, &filter))
								{
									continue;
								}
							}
							bool goodTrace {};
							f32 ratio {};
							bool hitNewPlane {};
							for (ratio = 0.25f; ratio <= 1.0f; ratio += 0.25f)
							{
								g_pKZUtils->TracePlayerBBox(start + offsetDirection * RAMP_PIERCE_DISTANCE * ratio,
															end + offsetDirection * RAMP_PIERCE_DISTANCE * ratio, bounds, &filter, pierce);
								if (!IsValidMovementTrace(pierce, bounds, &filter))
								{
									continue;
								}
								// Try until we hit a similar plane.
								// clang-format off
								validPlane = pierce.m_flFraction < 1.0f && pierce.m_flFraction > 0.1f 
											 && pierce.m_vHitNormal.Dot(this->lastValidPlane) >= RAMP_BUG_THRESHOLD;

								hitNewPlane = pm.m_vHitNormal.Dot(pierce.m_vHitNormal) < NEW_RAMP_THRESHOLD 
											  && this->lastValidPlane.Dot(pierce.m_vHitNormal) > NEW_RAMP_THRESHOLD;
								// clang-format on
								goodTrace = CloseEnough(pierce.m_flFraction, 1.0f, FLT_EPSILON) || validPlane;
								if (goodTrace)
								{
									break;
								}
							}
							if (goodTrace || hitNewPlane)
							{
								// Trace back to the original end point to find its normal.
								trace_t test;
								g_pKZUtils->TracePlayerBBox(pierce.m_vEndPos, end, bounds, &filter, test);
								pm = pierce;
								pm.m_vStartPos = start;
								pm.m_flFraction = Clamp((pierce.m_vEndPos - pierce.m_vStartPos).Length() / (end - start).Length(), 0.0f, 1.0f);
								pm.m_vEndPos = test.m_vEndPos;
								if (pierce.m_vHitNormal.Length() > 0.0f)
								{
									pm.m_vHitNormal = pierce.m_vHitNormal;
									this->lastValidPlane = pierce.m_vHitNormal;
								}
								else
								{
									pm.m_vHitNormal = test.m_vHitNormal;
									this->lastValidPlane = test.m_vHitNormal;
								}
								success = true;
								this->overrideTPM = true;
							}
						}
					}
				}
			}
			if (pm.m_vHitNormal.Length() > 0.99f)
			{
				this->lastValidPlane = pm.m_vHitNormal;
			}
			potentiallyStuck = pm.m_flFraction == 0.0f;
		}

		if (pm.m_flFraction * velocity.Length() > 0.03125f || pm.m_flFraction > 0.03125f)
		{
			allFraction += pm.m_flFraction;
			start = pm.m_vEndPos;
			numPlanes = 0;
		}

		this->tpmTriggerFixOrigins.AddToTail(pm.m_vEndPos);

		if (allFraction == 1.0f)
		{
			break;
		}
		timeLeft -= g_pKZUtils->GetGlobals()->frametime * pm.m_flFraction;

		// 2024-11-07 update also adds a low velocity check... This is only correct as long as you don't collide with other players.
		if (numPlanes >= 5 || (pm.m_vHitNormal.z >= 0.7f && velocity.Length2D() < 1.0f))
		{
			VectorCopy(vec3_origin, velocity);
			break;
		}

		planes[numPlanes] = pm.m_vHitNormal;
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
						{
							break; // not ok
						}
					}
				}

				if (j == numPlanes) // Didn't have to clip, so we're ok
				{
					break;
				}
			}
			// Did we go all the way through plane set
			if (i != numPlanes)
			{ // go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;
			}
			else
			{ // go along the crease
				if (numPlanes != 2)
				{
					VectorCopy(vec3_origin, velocity);
					break;
				}
				Vector dir;
				f32 d;
				CrossProduct(planes[0], planes[1], dir);
				// Yes, that's right, you need to do this twice because running it once won't ensure that this will be fully normalized.
				dir.NormalizeInPlace();
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
	this->tpmOrigin = pm.m_vEndPos;
	this->tpmVelocity = velocity;
}

void KZClassicModeService::OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing)
{
	Vector velocity;
	this->player->GetVelocity(&velocity);
	bool velocityHeavilyModified =
		this->tpmVelocity.Normalized().Dot(velocity.Normalized()) < RAMP_BUG_THRESHOLD
		|| (this->tpmVelocity.Length() > 50.0f && velocity.Length() / this->tpmVelocity.Length() < RAMP_BUG_VELOCITY_THRESHOLD);
	if (this->overrideTPM && velocityHeavilyModified && this->tpmOrigin != vec3_invalid && this->tpmVelocity != vec3_invalid)
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

void KZClassicModeService::OnCategorizePosition(bool bStayOnGround)
{
	// Already on the ground?
	// If we are already colliding on a standable valid plane, we don't want to do the check.
	if (bStayOnGround || this->lastValidPlane.Length() < EPSILON || this->lastValidPlane.z > 0.7f)
	{
		return;
	}
	// Only attempt to fix rampbugs while going down significantly enough.
	if (this->player->currentMoveData->m_vecVelocity.z > -64.0f)
	{
		return;
	}
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);

	CTraceFilterPlayerMovementCS filter(this->player->GetPlayerPawn());

	trace_t trace;

	Vector origin, groundOrigin;
	this->player->GetOrigin(&origin);
	groundOrigin = origin;
	groundOrigin.z -= 2.0f;

	g_pKZUtils->TracePlayerBBox(origin, groundOrigin, bounds, &filter, trace);

	if (trace.m_flFraction == 1.0f)
	{
		return;
	}
	// Is this something that you should be able to actually stand on?
	if (trace.m_flFraction < 0.95f && trace.m_vHitNormal.z > 0.7f && this->lastValidPlane.Dot(trace.m_vHitNormal) < RAMP_BUG_THRESHOLD)
	{
		origin += this->lastValidPlane * 0.0625f;
		groundOrigin = origin;
		groundOrigin.z -= 2.0f;
		g_pKZUtils->TracePlayerBBox(origin, groundOrigin, bounds, &filter, trace);
		if (trace.m_bStartInSolid)
		{
			return;
		}
		if (trace.m_flFraction == 1.0f || this->lastValidPlane.Dot(trace.m_vHitNormal) >= RAMP_BUG_THRESHOLD)
		{
			this->player->SetOrigin(origin);
		}
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
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	f64 tick = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_RATE;
	if (fabs(roundf(tick) - tick) < 0.001f || fabs(roundf(tick) - tick - 0.5f) < 0.001f)
	{
		return true;
	}

	return false;
}

bool KZClassicModeService::OnTriggerTouch(CBaseTrigger *trigger)
{
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	f64 tick = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_RATE;
	if (fabs(roundf(tick) - tick) < 0.001f || fabs(roundf(tick) - tick - 0.5f) < 0.001f)
	{
		return true;
	}
	return false;
}

bool KZClassicModeService::OnTriggerEndTouch(CBaseTrigger *trigger)
{
	if (!g_pMappingApi->IsTriggerATimerZone(trigger))
	{
		return true;
	}
	f64 tick = g_pKZUtils->GetGlobals()->curtime * ENGINE_FIXED_TICK_RATE;
	if (fabs(roundf(tick) - tick) < 0.001f || fabs(roundf(tick) - tick - 0.5f) < 0.001f)
	{
		return true;
	}
	return false;
}
