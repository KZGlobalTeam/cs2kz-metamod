#include "cs2kz.h"

#include "interface.h"
#include "icvar.h"
#include "iserver.h"
#include "entity2/entitysystem.h"

#include "common.h"
#include "utils/utils.h"
#include "utils/recipientfilters.h"
#include "utils/detours.h"
#include "utils/addresses.h"
#include "utils/simplecmds.h"

#include "movement/movement.h"
#include "kz/kz.h"
#include "kz/quiet/kz_quiet.h"

#include "tier0/memdbgon.h"

#include "version.h"

KZPlugin g_KZPlugin;

class GameSessionConfiguration_t { };

SH_DECL_HOOK2_void(ISource2GameClients, ClientCommand, SH_NOATTRIB, false, CPlayerSlot, const CCommand&);
SH_DECL_HOOK6_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, false, CCheckTransmitInfo**, int, CBitVec<16384>&, const Entity2Networkable_t **, const uint16 *, int);
SH_DECL_HOOK3_void(ISource2Server, GameFrame, SH_NOATTRIB, false, bool, bool, bool);
SH_DECL_HOOK5(ISource2GameClients, ProcessUsercmds, SH_NOATTRIB, false, float, CPlayerSlot, bf_read *, int, bool, bool);
SH_DECL_HOOK2_void(CEntitySystem, Spawn, SH_NOATTRIB, false, int, const EntitySpawnInfo_t *);
SH_DECL_HOOK4_void(ISource2GameClients, ClientPutInServer, SH_NOATTRIB, false, CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t &, ISource2WorldSession *, const char *);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, false, bool, IGameEvent *, bool);
CEntitySystem *g_pEntitySystem = NULL;

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
	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_STATIC(Hook_CheckTransmit), true);
	SH_ADD_HOOK(ISource2GameClients, ClientPutInServer, g_pSource2GameClients, SH_STATIC(Hook_ClientPutInServer), false);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_STATIC(Hook_StartupServer), true);
	SH_ADD_HOOK(IGameEventManager2, FireEvent, interfaces::pGameEventManager, SH_STATIC(Hook_FireEvent), false);
	KZ::misc::RegisterCommands();

	return true;
}

bool KZPlugin::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);
	SH_REMOVE_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_REMOVE_HOOK(ISource2GameClients, ProcessUsercmds, g_pSource2GameClients, SH_STATIC(Hook_ProcessUsercmds_Pre), false);
	SH_REMOVE_HOOK(ISource2GameClients, ProcessUsercmds, g_pSource2GameClients, SH_STATIC(Hook_ProcessUsercmds_Post), true);
	SH_REMOVE_HOOK(CEntitySystem, Spawn, g_pEntitySystem, SH_STATIC(Hook_CEntitySystem_Spawn_Post), true);
	SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_STATIC(Hook_CheckTransmit), true);
	SH_REMOVE_HOOK(ISource2GameClients, ClientPutInServer, g_pSource2GameClients, SH_STATIC(Hook_ClientPutInServer), false);
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_STATIC(Hook_StartupServer), true);

	
	utils::Cleanup();
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

internal float Hook_ProcessUsercmds_Pre(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	RETURN_META_VALUE(MRES_IGNORED, 0.0f);
}

internal float Hook_ProcessUsercmds_Post(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	RETURN_META_VALUE(MRES_IGNORED, 0.0f);
}

internal void Hook_CEntitySystem_Spawn_Post(int nCount, const EntitySpawnInfo_t *pInfo_DontUse)
{
	EntitySpawnInfo_t *pInfo = (EntitySpawnInfo_t *)pInfo_DontUse;
	
	for (i32 i = 0; i < nCount; i++)
	{
		if (pInfo && pInfo[i].m_pEntity)
		{
			// do stuff with spawning entities!
		}
	}
}

internal void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
	if (!g_pEntitySystem)
	{
		g_pEntitySystem = interfaces::pGameResourceServiceServer->GetGameEntitySystem();
		assert(g_pEntitySystem);
		SH_ADD_HOOK(CEntitySystem, Spawn, g_pEntitySystem, SH_STATIC(Hook_CEntitySystem_Spawn_Post), true);
	}
	RETURN_META(MRES_IGNORED);
}

internal void Hook_ClientCommand(CPlayerSlot slot, const CCommand& args)
{
	if (META_RES result = scmd::OnClientCommand(slot, args))
	{
		RETURN_META(result);
	}
	RETURN_META(MRES_IGNORED);
}

internal void Hook_CheckTransmit(CCheckTransmitInfo **pInfo, int infoCount, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntities)
{
	KZ::quiet::OnCheckTransmit(pInfo, infoCount);
	RETURN_META(MRES_IGNORED);
}

internal void Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid)
{
	KZ::misc::OnClientPutInServer(slot);
	RETURN_META(MRES_IGNORED);
}

internal void Hook_StartupServer(const GameSessionConfiguration_t &config, ISource2WorldSession *, const char *)
{
	interfaces::pEngine->ServerCommand("exec cs2kz.cfg");
}

internal bool Hook_FireEvent(IGameEvent *event, bool bDontBroadcast)
{
	if (event)
	{
		META_CONPRINTF("%s fired!\n", event->GetName());
	}
	RETURN_META_VALUE(MRES_IGNORED, true);
}