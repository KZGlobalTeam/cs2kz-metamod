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
#include "kz/mode/kz_mode.h"

#include "tier0/memdbgon.h"

#include "version.h"

KZPlugin g_KZPlugin;

class GameSessionConfiguration_t {};

SH_DECL_HOOK2_void(ISource2GameClients, ClientCommand, SH_NOATTRIB, false, CPlayerSlot, const CCommand &);
SH_DECL_HOOK6_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, false, CCheckTransmitInfo **, int, CBitVec<16384> &, const Entity2Networkable_t **, const uint16 *, int);
SH_DECL_HOOK3_void(ISource2Server, GameFrame, SH_NOATTRIB, false, bool, bool, bool);
SH_DECL_HOOK5(ISource2GameClients, ProcessUsercmds, SH_NOATTRIB, false, int, CPlayerSlot, bf_read *, int, bool, bool);
SH_DECL_HOOK2_void(CEntitySystem, Spawn, SH_NOATTRIB, false, int, const EntitySpawnInfo_t *);
SH_DECL_HOOK4_void(ISource2GameClients, ClientPutInServer, SH_NOATTRIB, false, CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t &, ISource2WorldSession *, const char *);
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, false, bool, IGameEvent *, bool);
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandHandle, const CCommandContext &, const CCommand &);
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64 *,
	INetworkSerializable *, const void *, unsigned long, NetChannelBufType_t)

	CEntitySystem *g_pEntitySystem = NULL;
CGlobalVars *gpGlobals = NULL;
PLUGIN_EXPOSE(KZPlugin, g_KZPlugin);

bool KZPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	if (!utils::Initialize(ismm, error, maxlen))
	{
		return false;
	}
	movement::InitDetours();

	KZ::mode::InitModeManager();

	SH_ADD_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);
	SH_ADD_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_ADD_HOOK(ISource2GameClients, ProcessUsercmds, g_pSource2GameClients, SH_STATIC(Hook_ProcessUsercmds_Pre), false);
	SH_ADD_HOOK(ISource2GameClients, ProcessUsercmds, g_pSource2GameClients, SH_STATIC(Hook_ProcessUsercmds_Post), true);
	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_STATIC(Hook_CheckTransmit), true);
	SH_ADD_HOOK(ISource2GameClients, ClientPutInServer, g_pSource2GameClients, SH_STATIC(Hook_ClientPutInServer), false);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_STATIC(Hook_StartupServer), true);
	SH_ADD_HOOK(IGameEventManager2, FireEvent, interfaces::pGameEventManager, SH_STATIC(Hook_FireEvent), false);
	SH_ADD_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_STATIC(Hook_DispatchConCommand), false);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, interfaces::pGameEventSystem, SH_STATIC(Hook_PostEvent), false);

	KZ::misc::RegisterCommands();

	if (!KZ::mode::InitModeCvars())
	{
		return false;
	}

	ismm->AddListener(this, this);

	KZ::mode::DisableReplicatedModeCvars();

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
	SH_REMOVE_HOOK(IGameEventManager2, FireEvent, interfaces::pGameEventManager, SH_STATIC(Hook_FireEvent), false);
	SH_REMOVE_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_STATIC(Hook_DispatchConCommand), false);
	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, interfaces::pGameEventSystem, SH_STATIC(Hook_PostEvent), false);

	KZ::mode::EnableReplicatedModeCvars();
	utils::Cleanup();
	g_pKZModeManager->Cleanup();
	return true;
}

void KZPlugin::AllPluginsLoaded()
{
	KZ::mode::LoadModePlugins();
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
	else if (strcmp(iface, KZ_UTILS_INTERFACE) == 0)
	{
		*ret = META_IFACE_OK;
		return g_pKZUtils;
	}
	*ret = META_IFACE_FAILED;

	return NULL;
}

internal int Hook_ProcessUsercmds_Pre(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

internal int Hook_ProcessUsercmds_Post(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	RETURN_META_VALUE(MRES_IGNORED, 0);
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

CGameEntitySystem *GameEntitySystem()
{
	return interfaces::pGameResourceServiceServer->GetGameEntitySystem();
}

internal void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
	if (!g_pEntitySystem)
	{
		g_pEntitySystem = GameEntitySystem();
		assert(g_pEntitySystem);
		SH_ADD_HOOK(CEntitySystem, Spawn, g_pEntitySystem, SH_STATIC(Hook_CEntitySystem_Spawn_Post), true);
	}
	RETURN_META(MRES_IGNORED);
}

internal void Hook_ClientCommand(CPlayerSlot slot, const CCommand &args)
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
	if (!gpGlobals)
	{
		gpGlobals = interfaces::pEngine->GetServerGlobals();
	}
}

internal bool Hook_FireEvent(IGameEvent *event, bool bDontBroadcast)
{
	if (event)
	{
		//META_CONPRINTF("%s fired!\n", event->GetName());
		if (V_stricmp(event->GetName(), "player_death") == 0)
		{
			CEntityInstance *instance = event->GetPlayerPawn("userid");
			g_pKZPlayerManager->ToPlayer(instance->GetEntityIndex())->quietService->SendFullUpdate();
		}
	}
	RETURN_META_VALUE(MRES_IGNORED, true);
}

internal void Hook_DispatchConCommand(ConCommandHandle cmd, const CCommandContext &ctx, const CCommand &args)
{
	META_RES mres = scmd::OnDispatchConCommand(cmd, ctx, args);

	RETURN_META(mres);
}

internal void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients,
	INetworkSerializable *pEvent, const void *pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	KZ::quiet::OnPostEvent(pEvent, pData, clients);
}