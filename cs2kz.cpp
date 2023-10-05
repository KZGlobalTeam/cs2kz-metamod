#include "cs2kz.h"

#include "interface.h"
#include "icvar.h"
#include "entity2/entitysystem.h"

#include "common.h"
#include "utils/utils.h"
#include "utils/recipientfilters.h"
#include "utils/detours.h"
#include "utils/addresses.h"
#include "movement/movement.h"
#include "movement/playermanager.h"

#include "kz/hud.h"
#include "kz/misc.h"

KZPlugin g_KZPlugin;

SH_DECL_HOOK2_void(ISource2GameClients, ClientCommand, SH_NOATTRIB, false, CPlayerSlot, const CCommand&);
SH_DECL_HOOK3_void(ISource2Server, GameFrame, SH_NOATTRIB, false, bool, bool, bool);
SH_DECL_HOOK5(ISource2GameClients, ProcessUsercmds, SH_NOATTRIB, false, float, CPlayerSlot, bf_read *, int, bool, bool);

CEntitySystem *g_pEntitySystem = NULL;
CPlayerManager *g_pPlayerManager;

PLUGIN_EXPOSE(KZPlugin, g_KZPlugin);


bool KZPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	
	if (!utils::Initialize(ismm, error, maxlen))
	{
		return false;
	}
	movement::InitDetours();

	SH_ADD_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);
	SH_ADD_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_ADD_HOOK(ISource2GameClients, ProcessUsercmds, g_pSource2GameClients, SH_STATIC(Hook_ProcessUsercmds_Pre), false);
	SH_ADD_HOOK(ISource2GameClients, ProcessUsercmds, g_pSource2GameClients, SH_STATIC(Hook_ProcessUsercmds_Post), true);
	g_pPlayerManager = new CPlayerManager();
	return true;
}

bool KZPlugin::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);
	SH_REMOVE_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_REMOVE_HOOK(ISource2GameClients, ProcessUsercmds, g_pSource2GameClients, SH_STATIC(Hook_ProcessUsercmds_Pre), false);
	SH_REMOVE_HOOK(ISource2GameClients, ProcessUsercmds, g_pSource2GameClients, SH_STATIC(Hook_ProcessUsercmds_Post), true);

	utils::Cleanup();
	delete g_pPlayerManager;
	return true;
}

void KZPlugin::AllPluginsLoaded()
{
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
	return "0.0.0.1";
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
	return "https://cs2.kz/";
}

static float Hook_ProcessUsercmds_Pre(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	KZ::misc::EnableGodMode(slot);
	RETURN_META_VALUE(MRES_IGNORED, 0.0f);
}

static float Hook_ProcessUsercmds_Post(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	KZ::hud::OnProcessUsercmds_Post(slot, buf, numcmds, ignore, paused);
	RETURN_META_VALUE(MRES_IGNORED, 0.0f);
}

static void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
	if (!g_pEntitySystem)
	{
		g_pEntitySystem = interfaces::pGameResourceServiceServer->GetGameEntitySystem();
	}
	RETURN_META(MRES_IGNORED);
}

static void Hook_ClientCommand(CPlayerSlot slot, const CCommand& args)
{
	RETURN_META(MRES_IGNORED);
}
