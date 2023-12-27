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
	g_pModeManager = (KZModeManager*)g_SMAPI->MetaFactory(KZ_MODE_MANAGER_INTERFACE, &success, 0);
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

	if (nullptr == (interfaces::pSchemaSystem = (CSchemaSystem *)g_pKZUtils->GetSchemaSystemPointer())) return false;
	if (nullptr == (schema::StateChanged = (StateChanged_t *)g_pKZUtils->GetSchemaStateChangedPointer())) return false;
	if (nullptr == (schema::NetworkStateChanged = (NetworkStateChanged_t *)g_pKZUtils->GetSchemaNetworkStateChangedPointer())) return false;

	if (!g_pModeManager->RegisterMode(g_PLID, MODE_NAME_SHORT, MODE_NAME, g_ModeFactory)) return false;

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
	if (!g_pModeManager->RegisterMode(g_PLID, MODE_NAME_SHORT, MODE_NAME, g_ModeFactory)) return false;
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

#define DUCK_SPEED_NORMAL 8.0
#define DUCK_SPEED_MINIMUM 6.0234375 // Equal to if you just ducked/unducked for the first time in a while

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
	// TODO: prestrafe
	return 276.0f;
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
		velocity.z += 0.25 * this->player->GetPawn()->m_flGravityScale() * 800 / 64;
		this->player->SetVelocity(velocity);
		this->tweakedJumpZSpeed = velocity.z;
		this->revertJumpTweak = true;
	}
}

void KZClassicModeService::OnJumpPost()
{
	float time = this->player->GetMoveServices()->m_flJumpPressedTime;
	// If we didn't jump, we revert the jump height tweak.
	if (this->revertJumpTweak)
	{
		Vector velocity;
		this->player->GetVelocity(&velocity);
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
	if (timeOnGround <= 0.02)
	{
		Vector2D landingVelocity2D(this->player->landingVelocity.x, this->player->landingVelocity.y);
		landingVelocity2D.NormalizeInPlace();
		float newSpeed = this->player->landingVelocity.Length2D();
		if (newSpeed > 276.0f)
		{
			newSpeed = (52 - timeOnGround * 128) * log(newSpeed) - 5.020043;
		}
		//META_CONPRINTF("currentSpeed = %.3f, timeOnGround = %.3f, landingspeed = %.3f, newSpeed = %.3f\n", speed, timeOnGround, this->player->landingVelocity.Length2D(), newSpeed);
		velocity.x = newSpeed * landingVelocity2D.x;
		velocity.y = newSpeed * landingVelocity2D.y;
		this->player->SetVelocity(velocity);
		this->player->takeoffVelocity = velocity;
	}
	// TODO: perf heights
}

void KZClassicModeService::OnProcessUsercmds(void *cmds, int numcmds)
{
#ifdef _WIN32
	constexpr u32 offset = 0x90;
#else
	constexpr u32 offset = 0x88;
#endif
	this->lastDesiredViewAngleTime = g_pKZUtils->GetServerGlobals()->curtime;
	for (i32 i = 0; i < numcmds; i++)
	{
		auto address = reinterpret_cast<char *>(cmds) + i * offset;
		CSGOUserCmdPB *usercmdsPtr = reinterpret_cast<CSGOUserCmdPB *>(address);
		this->lastDesiredViewAngle = { usercmdsPtr->base().viewangles().x(), usercmdsPtr->base().viewangles().y(), usercmdsPtr->base().viewangles().z() };
		for (i32 j = 0; j < usercmdsPtr->mutable_base()->subtick_moves_size(); j++)
		{
			CSubtickMoveStep *subtickMove = usercmdsPtr->mutable_base()->mutable_subtick_moves(j);
			float when = subtickMove->when();
			// Iffy logic that doesn't care about commands with multiple cmds...
			if (subtickMove->button() == IN_JUMP)
			{
				f32 inputTime = (g_pKZUtils->GetServerGlobals()->tickcount + when - 1) * 0.015625;
				if (when != 0)
				{
					if (subtickMove->pressed() && inputTime - this->lastJumpReleaseTime > 0.0078125)
					{
						this->player->GetMoveServices()->m_bOldJumpPressed = false;
					}
					if (!subtickMove->pressed())
					{
						this->lastJumpReleaseTime = (g_pKZUtils->GetServerGlobals()->tickcount + when - 1) * 0.015625;
					}
				}
			}
			subtickMove->set_when(when >= 0.5 ? 0.5 : 0);
		}
	}
	this->InsertSubtickTiming(g_pKZUtils->GetServerGlobals()->tickcount * 0.015625 - 0.0078125, false);
}

void KZClassicModeService::OnPlayerMove()
{
	this->ReduceDuckSlowdown();
	this->InterpolateViewAngles();
}

void KZClassicModeService::OnPostPlayerMovePost()
{
	this->InsertSubtickTiming(g_pKZUtils->GetServerGlobals()->tickcount * 0.015625 + 0.0078125, true);
	this->RestoreInterpolatedViewAngles();
}

void KZClassicModeService::InsertSubtickTiming(float time, bool future)
{
	CCSPlayer_MovementServices *moveServices = this->player->GetMoveServices();
	if (!moveServices) return;
	// Don't create subtick too close to real time, there will be movement processing there anyway.
	if (fabs(roundf(time) - time) < 0.001) return;
	// Don't create subtick timing too far into the future.
	if (time * 64.0 - g_pKZUtils->GetServerGlobals()->tickcount > 1.0f) return;
	// Don't create subtick timing too far back.
	if (time * 64.0 - g_pKZUtils->GetServerGlobals()->tickcount < -1.0f) return;

	for (i32 i = 0; i < 4; i++)
	{
		// Already exists.
		if (fabs(time - moveServices->m_arrForceSubtickMoveWhen[i]) < 0.001f) return;
		if (!future)
		{
			// Do not override other valid subtick moves that might happen.
			if (moveServices->m_arrForceSubtickMoveWhen[i] - g_pKZUtils->GetServerGlobals()->curtime < 0.015625) continue;
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
	CGlobalVars* globals = g_pKZUtils->GetServerGlobals();
	// NOTE: tickcount is half a tick ahead of curtime while in the middle of a tick.
	if ((f64)globals->tickcount / 64.0 - globals->curtime < 0.001)
		return;

	if (this->lastDesiredViewAngleTime < g_pKZUtils->GetServerGlobals()->curtime + 0.015625)
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

void KZClassicModeService::ReduceDuckSlowdown()
{
	if (!this->player->GetMoveServices()->m_bDucking && this->player->GetMoveServices()->m_flDuckSpeed < DUCK_SPEED_NORMAL - 0.000001f)
	{
		this->player->GetMoveServices()->m_flDuckSpeed = DUCK_SPEED_NORMAL;
	}
	else if (this->player->GetMoveServices()->m_flDuckSpeed < DUCK_SPEED_MINIMUM - 0.000001f)
	{
		this->player->GetMoveServices()->m_flDuckSpeed = DUCK_SPEED_MINIMUM;
	}
}