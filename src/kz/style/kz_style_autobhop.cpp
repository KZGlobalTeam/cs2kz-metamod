#include "kz_style_autobhop.h"

#include "utils/addresses.h"
#include "utils/interfaces.h"
#include "utils/gameconfig.h"
#include "version.h"

KZAutoBhopStylePlugin g_KZAutoBhopStylePlugin;

CGameConfig *g_pGameConfig = NULL;
KZUtils *g_pKZUtils = NULL;
KZStyleManager *g_pStyleManager = NULL;
StyleServiceFactory g_StyleFactory = [](KZPlayer *player) -> KZStyleService * { return new KZAutoBhopStyleService(player); };
PLUGIN_EXPOSE(KZAutoBhopStylePlugin, g_KZAutoBhopStylePlugin);

CConVarRef<bool> sv_autobunnyhopping("sv_autobunnyhopping");

bool KZAutoBhopStylePlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	// Load mode
	int success;
	g_pStyleManager = (KZStyleManager *)g_SMAPI->MetaFactory(KZ_STYLE_MANAGER_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_STYLE_MANAGER_INTERFACE);
		return false;
	}
	g_pKZUtils = (KZUtils *)g_SMAPI->MetaFactory(KZ_UTILS_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_UTILS_INTERFACE);
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

	if (!g_pStyleManager->RegisterStyle(g_PLID, STYLE_NAME_SHORT, STYLE_NAME, g_StyleFactory))
	{
		V_snprintf(error, maxlen, "Failed to register style");
		return false;
	}
	ConVar_Register();

	return true;
}

bool KZAutoBhopStylePlugin::Unload(char *error, size_t maxlen)
{
	g_pStyleManager->UnregisterStyle(g_PLID);
	return true;
}

bool KZAutoBhopStylePlugin::Pause(char *error, size_t maxlen)
{
	g_pStyleManager->UnregisterStyle(g_PLID);
	return true;
}

bool KZAutoBhopStylePlugin::Unpause(char *error, size_t maxlen)
{
	if (!g_pStyleManager->RegisterStyle(g_PLID, STYLE_NAME_SHORT, STYLE_NAME, g_StyleFactory))
	{
		return false;
	}
	return true;
}

const char *KZAutoBhopStylePlugin::GetLicense()
{
	return "GPLv3";
}

const char *KZAutoBhopStylePlugin::GetVersion()
{
	return VERSION_STRING;
}

const char *KZAutoBhopStylePlugin::GetDate()
{
	return __DATE__;
}

const char *KZAutoBhopStylePlugin::GetLogTag()
{
	return "KZ-Style-AutoBhop";
}

const char *KZAutoBhopStylePlugin::GetAuthor()
{
	return "zer0.k";
}

const char *KZAutoBhopStylePlugin::GetDescription()
{
	return "AutoBhop style plugin for CS2KZ";
}

const char *KZAutoBhopStylePlugin::GetName()
{
	return "CS2KZ-Style-AutoBhop";
}

const char *KZAutoBhopStylePlugin::GetURL()
{
	return "https://github.com/KZGlobalTeam/cs2kz-metamod";
}

CGameEntitySystem *GameEntitySystem()
{
	return g_pKZUtils->GetGameEntitySystem();
}

void KZAutoBhopStyleService::Init()
{
	g_pKZUtils->SendConVarValue(this->player->GetPlayerSlot(), sv_autobunnyhopping, "true");
}

const CVValue_t *KZAutoBhopStyleService::GetTweakedConvarValue(const char *name)
{
	static_persist const CVValue_t sv_autobunnyhopping_desiredValue = true;
	if (!V_stricmp(name, "sv_autobunnyhopping"))
	{
		return &sv_autobunnyhopping_desiredValue;
	}
	return nullptr;
}

void KZAutoBhopStyleService::Cleanup()
{
	g_pKZUtils->SendConVarValue(this->player->GetPlayerSlot(), sv_autobunnyhopping, "false");
}

void KZAutoBhopStyleService::OnProcessMovement()
{
	sv_autobunnyhopping.Set(true);
}
