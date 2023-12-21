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
	for (i32 i = 0; i < numcmds; i++)
	{
		auto address = reinterpret_cast<char *>(cmds) + i * offset;
		CSGOUserCmdPB *usercmdsPtr = reinterpret_cast<CSGOUserCmdPB *>(address);

		for (i32 j = 0; j < usercmdsPtr->mutable_base()->subtick_moves_size(); j++)
		{
			CSubtickMoveStep *subtickMove = usercmdsPtr->mutable_base()->mutable_subtick_moves(j);
			float when = subtickMove->when();
			subtickMove->set_when(when >= 0.5 ? 0.5 : 0);
		}	
	}
}