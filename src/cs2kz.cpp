#include "cs2kz.h"

#include "entity2/entitysystem.h"
#include "steam/steam_gameserver.h"

#include "sdk/cgameresourceserviceserver.h"
#include "utils/utils.h"
#include "utils/hooks.h"
#include "utils/gameconfig.h"

#include "movement/movement.h"
#include "kz/kz.h"
#include "kz/db/kz_db.h"
#include "kz/hud/kz_hud.h"
#include "kz/mode/kz_mode.h"
#include "kz/spec/kz_spec.h"
#include "kz/goto/kz_goto.h"
#include "kz/style/kz_style.h"
#include "kz/quiet/kz_quiet.h"
#include "kz/tip/kz_tip.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/global/kz_global.h"

#include "version.h"

#include <vendor/MultiAddonManager/public/imultiaddonmanager.h>
#include <vendor/ClientCvarValue/public/iclientcvarvalue.h>

#include "tier0/memdbgon.h"
KZPlugin g_KZPlugin;

IMultiAddonManager *g_pMultiAddonManager;
IClientCvarValue *g_pClientCvarValue;
CSteamGameServerAPIContext g_steamAPI;

PLUGIN_EXPOSE(KZPlugin, g_KZPlugin);

bool KZPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	if (!utils::Initialize(ismm, error, maxlen))
	{
		return false;
	}

	hooks::Initialize();
	movement::InitDetours();
	KZCheckpointService::Init();
	KZTimerService::Init();
	KZSpecService::Init();
	KZGotoService::Init();
	KZHUDService::Init();
	KZLanguageService::Init();
	KZ::misc::Init();
	KZQuietService::Init();
	KZ::misc::RegisterCommands();
	if (!KZ::mode::InitModeCvars())
	{
		return false;
	}

	ismm->AddListener(this, this);
	KZ::mapapi::Init();
	KZ::mode::InitModeManager();
	KZ::style::InitStyleManager();

	KZ::mode::DisableReplicatedModeCvars();

	KZOptionService::InitOptions();
	KZTipService::InitTips();
	if (late)
	{
		g_steamAPI.Init();
		g_pKZPlayerManager->OnLateLoad();
		// We need to reset the map for mapping api to properly load in.
		utils::ResetMap();
	}
	return true;
}

bool KZPlugin::Unload(char *error, size_t maxlen)
{
	this->unloading = true;
	hooks::Cleanup();
	KZ::mode::EnableReplicatedModeCvars();
	utils::Cleanup();
	g_pKZModeManager->Cleanup();
	g_pKZStyleManager->Cleanup();
	g_pPlayerManager->Cleanup();
	KZDatabaseService::Cleanup();
	KZGlobalService::Cleanup();
	return true;
}

void KZPlugin::AllPluginsLoaded()
{
	KZDatabaseService::Init();
	KZ::mode::LoadModePlugins();
	KZ::style::LoadStylePlugins();
	g_pKZPlayerManager->ResetPlayers();
	this->UpdateSelfMD5();
	g_pMultiAddonManager = (IMultiAddonManager *)g_SMAPI->MetaFactory(MULTIADDONMANAGER_INTERFACE, nullptr, nullptr);
	g_pClientCvarValue = (IClientCvarValue *)g_SMAPI->MetaFactory(CLIENTCVARVALUE_INTERFACE, nullptr, nullptr);
}

void KZPlugin::AddonInit()
{
	static_persist bool addonLoaded;
	if (g_pMultiAddonManager != nullptr && !addonLoaded)
	{
		g_pMultiAddonManager->AddAddon(KZ_WORKSHOP_ADDONS_ID);
		g_pMultiAddonManager->RefreshAddons();
		addonLoaded = true;
	}
}

bool KZPlugin::IsAddonMounted()
{
	if (g_pMultiAddonManager != nullptr)
	{
		return g_pMultiAddonManager->IsAddonMounted(KZ_WORKSHOP_ADDONS_ID);
	}
	return false;
}

bool KZPlugin::Pause(char *error, size_t maxlen)
{
	return true;
}

bool KZPlugin::Unpause(char *error, size_t maxlen)
{
	return true;
}

const char *KZPlugin::GetLicense()
{
	return "GPLv3";
}

const char *KZPlugin::GetVersion()
{
	return VERSION_STRING;
}

const char *KZPlugin::GetDate()
{
	return __DATE__;
}

const char *KZPlugin::GetLogTag()
{
	return "KZ";
}

const char *KZPlugin::GetAuthor()
{
	return "zer0.k";
}

const char *KZPlugin::GetDescription()
{
	return "CS2 KreedZ";
}

const char *KZPlugin::GetName()
{
	return "CS2KZ";
}

const char *KZPlugin::GetURL()
{
	return "https://github.com/KZGlobalTeam/cs2kz-metamod";
}

void *KZPlugin::OnMetamodQuery(const char *iface, int *ret)
{
	if (strcmp(iface, KZ_MODE_MANAGER_INTERFACE) == 0)
	{
		*ret = META_IFACE_OK;
		return g_pKZModeManager;
	}
	else if (strcmp(iface, KZ_STYLE_MANAGER_INTERFACE) == 0)
	{
		*ret = META_IFACE_OK;
		return g_pKZStyleManager;
	}
	else if (strcmp(iface, KZ_UTILS_INTERFACE) == 0)
	{
		*ret = META_IFACE_OK;
		return g_pKZUtils;
	}
	else if (strcmp(iface, KZ_MAPPING_INTERFACE) == 0)
	{
		*ret = META_IFACE_OK;
		return g_pMappingApi;
	}
	*ret = META_IFACE_FAILED;

	return NULL;
}

void KZPlugin::UpdateSelfMD5()
{
	ISmmPluginManager *pluginManager = (ISmmPluginManager *)g_SMAPI->MetaFactory(MMIFACE_PLMANAGER, nullptr, nullptr);
	const char *path;
	pluginManager->Query(g_PLID, &path, nullptr, nullptr);
	g_pKZUtils->GetFileMD5(path, this->md5, sizeof(this->md5));
}

CGameEntitySystem *GameEntitySystem()
{
	return interfaces::pGameResourceServiceServer->GetGameEntitySystem();
}
