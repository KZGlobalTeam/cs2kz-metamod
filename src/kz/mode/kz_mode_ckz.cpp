#include "kz_mode_ckz.h"
#include "version.h"

KZClassicModePlugin g_KZClassicModePlugin;
KZModeManager *g_ModeManager = NULL;
ModeServiceFactory g_ModeFactory = [](KZPlayer *player) -> KZModeService *{ return new KZClassicModeService(player); };
PLUGIN_EXPOSE(KZClassicModePlugin, g_KZClassicModePlugin);

bool KZClassicModePlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	// Load mode
	int success;
	KZModeManager *g_ModeManager = (KZModeManager*)g_SMAPI->MetaFactory(KZ_MODE_MANAGER_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED) return false;
	
	if (!g_ModeManager->RegisterMode(MODE_NAME_SHORT, MODE_NAME, g_ModeFactory)) return false;

	return true;
}

bool KZClassicModePlugin::Unload(char *error, size_t maxlen)
{
	g_ModeManager->UnregisterMode(MODE_NAME);
	return true;
}

void KZClassicModePlugin::AllPluginsLoaded()
{
}

bool KZClassicModePlugin::Pause(char *error, size_t maxlen)
{
	g_ModeManager->UnregisterMode(MODE_NAME);
	return true;
}

bool KZClassicModePlugin::Unpause(char *error, size_t maxlen)
{
	if (!g_ModeManager->RegisterMode(MODE_NAME_SHORT, MODE_NAME, g_ModeFactory)) return false;
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
	return 250.0f;
}

const char **KZClassicModeService::GetModeConVarValues()
{
	return modeCvarValues;
}