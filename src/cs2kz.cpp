#include "cs2kz.h"

#include "entity2/entitysystem.h"
#include "sdk/cgameresourceserviceserver.h"
#include "utils/utils.h"
#include "utils/hooks.h"
#include "utils/gameconfig.h"

#include "movement/movement.h"
#include "kz/kz.h"
#include "kz/hud/kz_hud.h"
#include "kz/mode/kz_mode.h"
#include "kz/spec/kz_spec.h"
#include "kz/style/kz_style.h"
#include "kz/tip/kz_tip.h"

#include "tier0/memdbgon.h"

#include "version.h"

KZPlugin g_KZPlugin;

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

	KZ::mode::InitModeManager();
	KZ::style::InitStyleManager();
	KZSpecService::Init();
	KZHUDService::Init();
	KZ::misc::RegisterCommands();
	if (!KZ::mode::InitModeCvars())
	{
		return false;
	}

	ismm->AddListener(this, this);

	KZ::mode::DisableReplicatedModeCvars();

	KZTipService::InitTips();
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
	return true;
}

void KZPlugin::AllPluginsLoaded()
{
	KZ::mode::LoadModePlugins();
	KZ::style::LoadStylePlugins();
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
	return "https://github.com/zer0k-z/cs2kz_metamod";
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
	*ret = META_IFACE_FAILED;

	return NULL;
}

CGameEntitySystem *GameEntitySystem()
{
	return interfaces::pGameResourceServiceServer->GetGameEntitySystem();
}
