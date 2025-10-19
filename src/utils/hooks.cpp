#include "hooks.h"
#include "addresses.h"
#include "bufferstring.h"
#include "igameeventsystem.h"
#include "igamesystem.h"
#include "entityclass.h"
#include "gamesystems/spawngroup_manager.h"
#include "utils/simplecmds.h"
#include "utils/gamesystem.h"
#include "steam/steam_gameserver.h"

#include "cs2kz.h"
#include "ctimer.h"
#include "kz/kz.h"
#include "kz/beam/kz_beam.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/option/kz_option.h"
#include "kz/quiet/kz_quiet.h"
#include "kz/timer/kz_timer.h"
#include "kz/timer/announce.h"
#include "kz/timer/queries/base_request.h"
#include "kz/telemetry/kz_telemetry.h"
#include "kz/trigger/kz_trigger.h"
#include "kz/db/kz_db.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/global/kz_global.h"
#include "kz/profile/kz_profile.h"
#include "kz/pistol/kz_pistol.h"
#include "kz/recording/kz_recording.h"
#include "kz/replays/kz_replaysystem.h"
#include "utils/utils.h"
#include "sdk/entity/cbasetrigger.h"
#include "sdk/usercmd.h"

#include "vprof.h"

#include "memdbgon.h"

extern CSteamGameServerAPIContext g_steamAPI;

class GameSessionConfiguration_t
{
};

class EntListener : public IEntityListener
{
	virtual void OnEntityCreated(CEntityInstance *pEntity);
	virtual void OnEntitySpawned(CEntityInstance *pEntity);
	virtual void OnEntityDeleted(CEntityInstance *pEntity);
} entityListener;

// CBaseEntity
SH_DECL_MANUALHOOK1_void(StartTouch, 0, 0, 0, CBaseEntity *);
static_function void Hook_OnStartTouch(CBaseEntity *pOther);
static_function void Hook_OnStartTouchPost(CBaseEntity *pOther);
SH_DECL_MANUALHOOK1_void(Touch, 0, 0, 0, CBaseEntity *);
static_function void Hook_OnTouch(CBaseEntity *pOther);
static_function void Hook_OnTouchPost(CBaseEntity *pOther);
SH_DECL_MANUALHOOK1_void(EndTouch, 0, 0, 0, CBaseEntity *);
static_function void Hook_OnEndTouch(CBaseEntity *pOther);
static_function void Hook_OnEndTouchPost(CBaseEntity *pOther);
SH_DECL_MANUALHOOK3_void(Teleport, 0, 0, 0, const Vector *, const QAngle *, const Vector *);
static_function void Hook_OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity);

// CCSPlayerController
static_global int changeTeamHook {};
SH_DECL_MANUALHOOK1_void(ChangeTeam, 0, 0, 0, int);
static_function void Hook_OnChangeTeamPost(i32 team);

// ISource2GameEntities
SH_DECL_HOOK7_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, false, CCheckTransmitInfo **, int, CBitVec<16384> &, CBitVec<16384> &,
				   const Entity2Networkable_t **, const uint16 *, int);
static_function void Hook_CheckTransmit(CCheckTransmitInfo **pInfo, int, CBitVec<16384> &, CBitVec<16384> &,
										const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntities);

// ISource2Server
SH_DECL_HOOK3_void(ISource2Server, GameFrame, SH_NOATTRIB, false, bool, bool, bool);
static_function void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
SH_DECL_HOOK0_void(ISource2Server, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
static_function void Hook_GameServerSteamAPIActivated();
SH_DECL_HOOK0_void(ISource2Server, GameServerSteamAPIDeactivated, SH_NOATTRIB, 0);
static_function void Hook_GameServerSteamAPIDeactivated();

// ISource2GameClients
SH_DECL_HOOK6(ISource2GameClients, ClientConnect, SH_NOATTRIB, false, bool, CPlayerSlot, const char *, uint64, const char *, bool, CBufferString *);
static_function bool Hook_ClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1,
										CBufferString *pRejectReason);

SH_DECL_HOOK6_void(ISource2GameClients, OnClientConnected, SH_NOATTRIB, false, CPlayerSlot, const char *, uint64, const char *, const char *, bool);
static_function void Hook_OnClientConnected(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress,
											bool bFakePlayer);

SH_DECL_HOOK1_void(ISource2GameClients, ClientFullyConnect, SH_NOATTRIB, false, CPlayerSlot);
static_function void Hook_ClientFullyConnect(CPlayerSlot slot);

SH_DECL_HOOK4_void(ISource2GameClients, ClientPutInServer, SH_NOATTRIB, false, CPlayerSlot, char const *, int, uint64);
static_function void Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid);

SH_DECL_HOOK4_void(ISource2GameClients, ClientActive, SH_NOATTRIB, false, CPlayerSlot, bool, const char *, uint64);
static_function void Hook_ClientActive(CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid);

SH_DECL_HOOK5_void(ISource2GameClients, ClientDisconnect, SH_NOATTRIB, false, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64,
				   const char *);
static_function void Hook_ClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid,
										   const char *pszNetworkID);

SH_DECL_HOOK1_void(ISource2GameClients, ClientVoice, SH_NOATTRIB, false, CPlayerSlot);
static_function void Hook_ClientVoice(CPlayerSlot slot);

SH_DECL_HOOK2_void(ISource2GameClients, ClientCommand, SH_NOATTRIB, false, CPlayerSlot, const CCommand &);
static_function void Hook_ClientCommand(CPlayerSlot slot, const CCommand &args);

// INetworkServerService
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t &, ISource2WorldSession *, const char *);
static_function void Hook_StartupServer(const GameSessionConfiguration_t &config, ISource2WorldSession *, const char *);

// IGameEventManager2
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, false, bool, IGameEvent *, bool);
static_function bool Hook_FireEvent(IGameEvent *event, bool bDontBroadcast);

// ICvar
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandRef, const CCommandContext &, const CCommand &);
static_function void Hook_DispatchConCommand(ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args);

// IGameEventSystem
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64 *, INetworkMessageInternal *,
				   const CNetMessage *, unsigned long, NetChannelBufType_t);
static_function void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent,
									const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType);

// CEntitySystem
static_global int entitySystemHook {};
SH_DECL_HOOK2_void(CEntitySystem, Spawn, SH_NOATTRIB, false, int, const EntitySpawnInfo_t *);
static_function void Hook_CEntitySystem_Spawn(int nCount, const EntitySpawnInfo_t *pInfo);

// CSpawnGroupMgrGameSystem
static_global int createLoadingSpawnGroupHook {};
SH_DECL_HOOK4(CSpawnGroupMgrGameSystem, CreateLoadingSpawnGroup, SH_NOATTRIB, 0, ILoadingSpawnGroup *, SpawnGroupHandle_t, bool, bool,
			  const CUtlVector<const CEntityKeyValues *> *);
static_function ILoadingSpawnGroup *Hook_OnCreateLoadingSpawnGroupHook(SpawnGroupHandle_t hSpawnGroup, bool bSynchronouslySpawnEntities,
																	   bool bConfirmResourcesLoaded,
																	   const CUtlVector<const CEntityKeyValues *> *pKeyValues);
// INetworkGameServer
static_global int activateServerHook {};
SH_DECL_HOOK0(CNetworkGameServerBase, ActivateServer, SH_NOATTRIB, false, bool);
static_function bool Hook_ActivateServer();

static_global int clientConnectHook {};
static_global int clientConnectPostHook {};
SH_DECL_HOOK8(CNetworkGameServerBase, ConnectClient, SH_NOATTRIB, false, CServerSideClientBase *, const char *, ns_address *, uint32,
			  C2S_CONNECT_Message *, const char *, const byte *, int, bool);
static_function CServerSideClientBase *Hook_ConnectClient(const char *, ns_address *, uint32, C2S_CONNECT_Message *, const char *, const byte *, int,
														  bool);
static_function CServerSideClientBase *Hook_ConnectClientPost(const char *, ns_address *, uint32, C2S_CONNECT_Message *, const char *, const byte *,
															  int, bool);

// IGameSystem
static_global int serverGamePostSimulateHook {};
SH_DECL_HOOK1_void(IGameSystem, ServerGamePostSimulate, SH_NOATTRIB, false, const EventServerGamePostSimulate_t *);
static_function void Hook_ServerGamePostSimulate(const EventServerGamePostSimulate_t *);

static_global int buildGameSessionManifestHookID {};
SH_DECL_HOOK1_void(IGameSystem, BuildGameSessionManifest, SH_NOATTRIB, false, const EventBuildGameSessionManifest_t *);
static_function void Hook_BuildGameSessionManifest(const EventBuildGameSessionManifest_t *msg);

static_global bool ignoreTouchEvent {};

// CCSPlayer_MovementServices
static_global int playerRunCommandHook {};
SH_DECL_MANUALHOOK1_void(PlayerRunCommand, 0, 0, 0, PlayerCommand *);
static_function void Hook_OnPlayerRunCommand(PlayerCommand *pCmd);

static_global int finishMoveHook {};
SH_DECL_MANUALHOOK2_void(FinishMove, 0, 0, 0, PlayerCommand *, CMoveData *);
static_function void Hook_OnFinishMove(PlayerCommand *pCmd, CMoveData *pMoveData);

void hooks::Initialize()
{
	SH_MANUALHOOK_RECONFIGURE(StartTouch, g_pGameConfig->GetOffset("StartTouch"), 0, 0);
	SH_MANUALHOOK_RECONFIGURE(Touch, g_pGameConfig->GetOffset("Touch"), 0, 0);
	SH_MANUALHOOK_RECONFIGURE(EndTouch, g_pGameConfig->GetOffset("EndTouch"), 0, 0);
	SH_MANUALHOOK_RECONFIGURE(Teleport, g_pGameConfig->GetOffset("Teleport"), 0, 0);

	SH_MANUALHOOK_RECONFIGURE(ChangeTeam, g_pGameConfig->GetOffset("ControllerChangeTeam"), 0, 0);

	SH_MANUALHOOK_RECONFIGURE(PlayerRunCommand, g_pGameConfig->GetOffset("PlayerRunCommand"), 0, 0);
	SH_MANUALHOOK_RECONFIGURE(FinishMove, g_pGameConfig->GetOffset("FinishMove"), 0, 0);

	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_STATIC(Hook_CheckTransmit), true);

	SH_ADD_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_ADD_HOOK(ISource2Server, GameServerSteamAPIActivated, interfaces::pServer, SH_STATIC(Hook_GameServerSteamAPIActivated), false);
	SH_ADD_HOOK(ISource2Server, GameServerSteamAPIDeactivated, interfaces::pServer, SH_STATIC(Hook_GameServerSteamAPIDeactivated), false);

	SH_ADD_HOOK(ISource2GameClients, ClientConnect, g_pSource2GameClients, SH_STATIC(Hook_ClientConnect), false);
	SH_ADD_HOOK(ISource2GameClients, OnClientConnected, g_pSource2GameClients, SH_STATIC(Hook_OnClientConnected), false);
	SH_ADD_HOOK(ISource2GameClients, ClientFullyConnect, g_pSource2GameClients, SH_STATIC(Hook_ClientFullyConnect), false);
	SH_ADD_HOOK(ISource2GameClients, ClientPutInServer, g_pSource2GameClients, SH_STATIC(Hook_ClientPutInServer), false);
	SH_ADD_HOOK(ISource2GameClients, ClientActive, g_pSource2GameClients, SH_STATIC(Hook_ClientActive), true);
	SH_ADD_HOOK(ISource2GameClients, ClientDisconnect, g_pSource2GameClients, SH_STATIC(Hook_ClientDisconnect), true);
	SH_ADD_HOOK(ISource2GameClients, ClientVoice, g_pSource2GameClients, SH_STATIC(Hook_ClientVoice), false);
	SH_ADD_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);

	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_STATIC(Hook_StartupServer), true);

	SH_ADD_HOOK(IGameEventManager2, FireEvent, interfaces::pGameEventManager, SH_STATIC(Hook_FireEvent), false);

	SH_ADD_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_STATIC(Hook_DispatchConCommand), false);

	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, interfaces::pGameEventSystem, SH_STATIC(Hook_PostEvent), false);
	// clang-format off
	activateServerHook = SH_ADD_DVPHOOK(
		CNetworkGameServerBase, 
		ActivateServer,
		(CNetworkGameServerBase *)modules::engine->FindVirtualTable("CNetworkGameServer"),
		SH_STATIC(Hook_ActivateServer), 
		true
	);

	clientConnectHook = SH_ADD_DVPHOOK(
		CNetworkGameServerBase, 
		ConnectClient,
		(CNetworkGameServerBase *)modules::engine->FindVirtualTable("CNetworkGameServer"),
		SH_STATIC(Hook_ConnectClient), 
		true
	);
	clientConnectPostHook = SH_ADD_DVPHOOK(
		CNetworkGameServerBase, 
		ConnectClient,
		(CNetworkGameServerBase *)modules::engine->FindVirtualTable("CNetworkGameServer"),
		SH_STATIC(Hook_ConnectClientPost), 
		true
	);
	serverGamePostSimulateHook = SH_ADD_DVPHOOK(
		IGameSystem, 
		ServerGamePostSimulate, 
		(IGameSystem *)modules::server->FindVirtualTable("CEntityDebugGameSystem"),
		SH_STATIC(Hook_ServerGamePostSimulate), 
		true
	);
	buildGameSessionManifestHookID = SH_ADD_DVPHOOK(
		IGameSystem, 
		BuildGameSessionManifest, 
		(IGameSystem *)modules::server->FindVirtualTable("CEntityDebugGameSystem"), 
		SH_STATIC(Hook_BuildGameSessionManifest), 
		true
	);
	
	entitySystemHook = SH_ADD_DVPHOOK(
		CEntitySystem, 
		Spawn, 
		(CEntitySystem *)modules::server->FindVirtualTable("CGameEntitySystem"), 
		SH_STATIC(Hook_CEntitySystem_Spawn), 
		false
	);
	
	createLoadingSpawnGroupHook = SH_ADD_DVPHOOK(
		CSpawnGroupMgrGameSystem, 
		CreateLoadingSpawnGroup, 
		(CSpawnGroupMgrGameSystem *)modules::server->FindVirtualTable("CSpawnGroupMgrGameSystem"), 
		SH_STATIC(Hook_OnCreateLoadingSpawnGroupHook), 
		false
	);
	CCSPlayer_MovementServices *moveServicesVtbl = (CCSPlayer_MovementServices *)modules::server->FindVirtualTable("CCSPlayer_MovementServices");
	playerRunCommandHook = SH_ADD_MANUALDVPHOOK(
	 	PlayerRunCommand, 
	 	moveServicesVtbl, 
	 	SH_STATIC(Hook_OnPlayerRunCommand), 
	 	false
	);

	finishMoveHook = SH_ADD_MANUALDVPHOOK(
		FinishMove,
		moveServicesVtbl,
		SH_STATIC(Hook_OnFinishMove),
		false
	);
	// clang-format on
}

void hooks::Cleanup()
{
	SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_STATIC(Hook_CheckTransmit), true);

	SH_REMOVE_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_REMOVE_HOOK(ISource2Server, GameServerSteamAPIActivated, interfaces::pServer, SH_STATIC(Hook_GameServerSteamAPIActivated), false);
	SH_REMOVE_HOOK(ISource2Server, GameServerSteamAPIDeactivated, interfaces::pServer, SH_STATIC(Hook_GameServerSteamAPIDeactivated), false);

	SH_REMOVE_HOOK(ISource2GameClients, ClientConnect, g_pSource2GameClients, SH_STATIC(Hook_ClientConnect), false);
	SH_REMOVE_HOOK(ISource2GameClients, OnClientConnected, g_pSource2GameClients, SH_STATIC(Hook_OnClientConnected), false);
	SH_REMOVE_HOOK(ISource2GameClients, ClientFullyConnect, g_pSource2GameClients, SH_STATIC(Hook_ClientFullyConnect), false);
	SH_REMOVE_HOOK(ISource2GameClients, ClientPutInServer, g_pSource2GameClients, SH_STATIC(Hook_ClientPutInServer), false);
	SH_REMOVE_HOOK(ISource2GameClients, ClientActive, g_pSource2GameClients, SH_STATIC(Hook_ClientActive), false);
	SH_REMOVE_HOOK(ISource2GameClients, ClientDisconnect, g_pSource2GameClients, SH_STATIC(Hook_ClientDisconnect), true);
	SH_REMOVE_HOOK(ISource2GameClients, ClientVoice, g_pSource2GameClients, SH_STATIC(Hook_ClientVoice), false);
	SH_REMOVE_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);

	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_STATIC(Hook_StartupServer), true);

	SH_REMOVE_HOOK(IGameEventManager2, FireEvent, interfaces::pGameEventManager, SH_STATIC(Hook_FireEvent), false);

	SH_REMOVE_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_STATIC(Hook_DispatchConCommand), false);

	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, interfaces::pGameEventSystem, SH_STATIC(Hook_PostEvent), false);

	SH_REMOVE_HOOK_ID(activateServerHook);

	SH_REMOVE_HOOK_ID(clientConnectHook);
	SH_REMOVE_HOOK_ID(clientConnectPostHook);

	SH_REMOVE_HOOK_ID(changeTeamHook);

	SH_REMOVE_HOOK_ID(buildGameSessionManifestHookID);

	SH_REMOVE_HOOK_ID(entitySystemHook);

	SH_REMOVE_HOOK_ID(createLoadingSpawnGroupHook);

	SH_REMOVE_HOOK_ID(playerRunCommandHook);
	SH_REMOVE_HOOK_ID(finishMoveHook);

	if (GameEntitySystem())
	{
		GameEntitySystem()->RemoveListenerEntity(&entityListener);
	}
}

// Entity hooks
static_function void AddEntityHooks(CBaseEntity *entity)
{
	if (!V_stricmp(entity->GetClassname(), "cs_player_controller") && !changeTeamHook)
	{
		changeTeamHook = SH_ADD_MANUALVPHOOK(ChangeTeam, entity, SH_STATIC(Hook_OnChangeTeamPost), true);
	}
	else if (V_strstr(entity->GetClassname(), "trigger_") || !V_stricmp(entity->GetClassname(), "player"))
	{
		hooks::entityTouchHooks.AddToTail(SH_ADD_MANUALHOOK(StartTouch, entity, SH_STATIC(Hook_OnStartTouch), false));
		hooks::entityTouchHooks.AddToTail(SH_ADD_MANUALHOOK(Touch, entity, SH_STATIC(Hook_OnTouch), false));
		hooks::entityTouchHooks.AddToTail(SH_ADD_MANUALHOOK(EndTouch, entity, SH_STATIC(Hook_OnEndTouch), false));
		hooks::entityTouchHooks.AddToTail(SH_ADD_MANUALHOOK(StartTouch, entity, SH_STATIC(Hook_OnStartTouchPost), true));
		hooks::entityTouchHooks.AddToTail(SH_ADD_MANUALHOOK(Touch, entity, SH_STATIC(Hook_OnTouchPost), true));
		hooks::entityTouchHooks.AddToTail(SH_ADD_MANUALHOOK(EndTouch, entity, SH_STATIC(Hook_OnEndTouchPost), true));
		CCSPlayerPawn *pawn = static_cast<CCSPlayerPawn *>(entity);
		if (!V_stricmp(entity->GetClassname(), "player") && g_pKZPlayerManager->ToPlayer(pawn))
		{
			hooks::entityTouchHooks.AddToTail(SH_ADD_MANUALHOOK(Teleport, pawn, SH_STATIC(Hook_OnTeleport), false));
		}
	}
}

static_function void RemoveEntityHooks(CBaseEntity *entity)
{
	if (V_strstr(entity->GetClassname(), "trigger_") || !V_stricmp(entity->GetClassname(), "player"))
	{
		SH_REMOVE_MANUALHOOK(StartTouch, entity, SH_STATIC(Hook_OnStartTouch), false);
		SH_REMOVE_MANUALHOOK(Touch, entity, SH_STATIC(Hook_OnTouch), false);
		SH_REMOVE_MANUALHOOK(EndTouch, entity, SH_STATIC(Hook_OnEndTouch), false);
		SH_REMOVE_MANUALHOOK(StartTouch, entity, SH_STATIC(Hook_OnStartTouchPost), true);
		SH_REMOVE_MANUALHOOK(Touch, entity, SH_STATIC(Hook_OnTouchPost), true);
		SH_REMOVE_MANUALHOOK(EndTouch, entity, SH_STATIC(Hook_OnEndTouchPost), true);
		if (!V_strstr(entity->GetClassname(), "trigger_"))
		{
			SH_REMOVE_MANUALHOOK(Teleport, static_cast<CCSPlayerPawn *>(entity), SH_STATIC(Hook_OnTeleport), false);
		}
	}
}

void EntListener::OnEntityCreated(CEntityInstance *pEntity) {}

void EntListener::OnEntitySpawned(CEntityInstance *pEntity)
{
	if (V_strstr(pEntity->GetClassname(), "trigger_"))
	{
		CBaseTrigger *trigger = static_cast<CBaseTrigger *>(pEntity);
		trigger->m_fEffects() &= ~EF_NODRAW;
		AddEntityHooks(static_cast<CBaseEntity *>(pEntity));
		KZ::mapapi::CheckEndTimerTrigger((CBaseTrigger *)pEntity);
	}
}

void EntListener::OnEntityDeleted(CEntityInstance *pEntity)
{
	if (V_strstr(pEntity->GetClassname(), "trigger_"))
	{
		RemoveEntityHooks(static_cast<CBaseEntity *>(pEntity));
	}
}

void hooks::HookEntities()
{
	FOR_EACH_VEC(hooks::entityTouchHooks, i)
	{
		SH_REMOVE_HOOK_ID(hooks::entityTouchHooks[i]);
	}
	hooks::entityTouchHooks.Purge();
	GameEntitySystem()->RemoveListenerEntity(&entityListener);
	for (CEntityIdentity *entID = GameEntitySystem()->m_EntityList.m_pFirstActiveEntity; entID != NULL; entID = entID->m_pNext)
	{
		AddEntityHooks(static_cast<CBaseEntity *>(entID->m_pInstance));
	}
	GameEntitySystem()->AddListenerEntity(&entityListener);
}

// CBaseEntity
static_function void Hook_OnStartTouch(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

static_function void Hook_OnStartTouchPost(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		RETURN_META(MRES_SUPERCEDE);
	}
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_OnTouch(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		RETURN_META(MRES_SUPERCEDE);
	}
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_OnTouchPost(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		RETURN_META(MRES_SUPERCEDE);
	}
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_OnEndTouch(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		RETURN_META(MRES_SUPERCEDE);
	}
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_OnEndTouchPost(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		RETURN_META(MRES_SUPERCEDE);
	}
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
{
	CBaseEntity *this_ = META_IFACEPTR(CBaseEntity);
	// Just to be sure.
	if (this_->IsPawn())
	{
		MovementPlayer *player = g_pKZPlayerManager->ToPlayer(static_cast<CBasePlayerPawn *>(this_));
		player->OnTeleport(newPosition, newAngles, newVelocity);
	}
	RETURN_META(MRES_IGNORED);
}

// CCSPlayerController
static_function void Hook_OnChangeTeamPost(i32 team)
{
	CCSPlayerController *controller = META_IFACEPTR(CCSPlayerController);
	MovementPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (player)
	{
		player->OnChangeTeamPost(team);
	}
}

// ISource2GameEntities
static_function void Hook_CheckTransmit(CCheckTransmitInfo **pInfos, int infoCount, CBitVec<16384> &unk1, CBitVec<16384> &,
										const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntities)
{
	KZ::quiet::OnCheckTransmit(pInfos, infoCount);
	KZProfileService::OnCheckTransmit();
	RETURN_META(MRES_IGNORED);
}

// ISource2Server
static_function void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_KZPlugin.serverGlobals = *(g_pKZUtils->GetGlobals());
	RecordAnnounce::Check();
	BaseRequest::CheckRequests();
	KZTelemetryService::ActiveCheck();
	KZBeamService::UpdateBeams();
	KZProfileService::OnGameFrame();
	KZ::replaysystem::OnGameFrame();
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_GameServerSteamAPIActivated()
{
	g_steamAPI.Init();
	g_pKZPlayerManager->OnSteamAPIActivated();
}

static_function void Hook_GameServerSteamAPIDeactivated() {}

// ISource2GameClients
static_function bool Hook_ClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1,
										CBufferString *pRejectReason)
{
	g_pKZPlayerManager->OnClientConnect(slot, pszName, xuid, pszNetworkID, unk1, pRejectReason);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

static_function void Hook_OnClientConnected(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress,
											bool bFakePlayer)
{
	g_pKZPlayerManager->OnClientConnected(slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer);
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_ClientFullyConnect(CPlayerSlot slot)
{
	g_pKZPlayerManager->OnClientFullyConnect(slot);
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid)
{
	g_pKZPlayerManager->OnClientPutInServer(slot, pszName, type, xuid);
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_ClientActive(CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid)
{
	g_pKZPlayerManager->OnClientActive(slot, bLoadGame, pszName, xuid);
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (player->GetPlayerPawn())
	{
		AddEntityHooks(player->GetPlayerPawn());
	}
	else
	{
		Warning("[KZ] WARNING: Player pawn for slot %i not found!\n", slot.Get());
	}
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_ClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid,
										   const char *pszNetworkID)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	// Immediately remove the player off the list. We don't need to keep them around.
	if (player->GetController())
	{
		player->GetController()->m_LastTimePlayerWasDisconnectedForPawnsRemove().SetTime(0.01f);
		player->GetController()->SwitchTeam(0);
	}
	if (player->GetPlayerPawn())
	{
		RemoveEntityHooks(player->GetPlayerPawn());
	}
	else
	{
		Warning("WARNING: Player pawn for slot %i not found!\n", slot.Get());
	}
	player->timerService->OnClientDisconnect();
	player->recordingService->OnClientDisconnect();
	player->optionService->OnClientDisconnect();
	player->globalService->OnClientDisconnect();
	g_pKZPlayerManager->OnClientDisconnect(slot, reason, pszName, xuid, pszNetworkID);
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_ClientVoice(CPlayerSlot slot)
{
	g_pKZPlayerManager->OnClientVoice(slot);
}

static_function void Hook_ClientCommand(CPlayerSlot slot, const CCommand &args)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (META_RES result = KZ::misc::CheckBlockedRadioCommands(args[0]))
	{
		RETURN_META(result);
	}
	if (META_RES result = scmd::OnClientCommand(slot, args))
	{
		RETURN_META(result);
	}
	RETURN_META(MRES_IGNORED);
}

// INetworkServerService
static_function void Hook_StartupServer(const GameSessionConfiguration_t &config, ISource2WorldSession *, const char *)
{
	g_KZPlugin.AddonInit();
	KZ::course::ClearCourses();
	KZ::mapapi::Init();
	KZ::replaysystem::Init();
	RETURN_META(MRES_IGNORED);
}

// IGameEventManager2
static_function bool Hook_FireEvent(IGameEvent *event, bool bDontBroadcast)
{
	if (event)
	{
		if (KZ_STREQI(event->GetName(), "player_death"))
		{
			CEntityInstance *instance = event->GetPlayerPawn("userid");
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(instance->GetEntityIndex());
			if (player)
			{
				player->timerService->OnPlayerDeath();
				player->quietService->SendFullUpdate();
			}
		}
		else if (KZ_STREQI(event->GetName(), "round_prestart"))
		{
			hooks::HookEntities();
			KZ::mapapi::OnRoundPreStart();
		}
		else if (KZ_STREQI(event->GetName(), "round_start"))
		{
			interfaces::pEngine->ServerCommand("sv_full_alltalk 1");
			KZTimerService::OnRoundStart();
			KZ::misc::OnRoundStart();
			KZ::mapapi::OnRoundStart();
			KZ::replaysystem::OnRoundStart();
		}
		else if (KZ_STREQI(event->GetName(), "player_team"))
		{
			event->SetBool("silent", true);
		}
		else if (KZ_STREQI(event->GetName(), "player_spawn"))
		{
			CEntityInstance *instance = event->GetPlayerPawn("userid");
			if (instance)
			{
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(instance->GetEntityIndex());
				if (player)
				{
					player->timerService->OnPlayerSpawn();
				}
			}
		}
	}
	RETURN_META_VALUE(MRES_IGNORED, true);
}

// ICvar
static_function void Hook_DispatchConCommand(ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (META_RES result = KZ::misc::CheckBlockedRadioCommands(args[0]))
	{
		RETURN_META(result);
	}
	if (KZOptionService::GetOptionInt("overridePlayerChat", true))
	{
		KZ::misc::ProcessConCommand(cmd, ctx, args);
	}

	META_RES mres = scmd::OnDispatchConCommand(cmd, ctx, args);
	RETURN_META(mres);
}

// IGameEventSystem
static_function void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkMessageInternal *pEvent,
									const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	KZ::quiet::OnPostEvent(pEvent, pData, clients);
}

// CEntitySystem
static_function void Hook_CEntitySystem_Spawn(int nCount, const EntitySpawnInfo_t *pInfo)
{
	KZ::mapapi::OnSpawn(nCount, pInfo);
}

// INetworkGameServer
static_function bool Hook_ActivateServer()
{
	// The host doesn't disconnect when the map changes.
	if (!interfaces::pEngine->IsDedicatedServer())
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(0));
		player->Reset();
	}
	u64 id = g_pKZUtils->GetCurrentMapWorkshopID();
	u64 size = g_pKZUtils->GetCurrentMapSize();

	META_CONPRINTF("[KZ] Loading map %s, workshop ID %llu, size %llu\n", g_pKZUtils->GetCurrentMapVPK().Get(), id, size);

	RecordAnnounce::Clear();
	KZ::misc::OnServerActivate();
	KZDatabaseService::SetupMap();
	KZGlobalService::OnActivateServer();

	char md5[33];
	g_pKZUtils->GetCurrentMapMD5(md5, sizeof(md5));
	META_CONPRINTF("[KZ] Map file md5: %s\n", md5);

	RETURN_META_VALUE(MRES_IGNORED, 1);
}

// CNetworkGameServerBase

static_function CServerSideClientBase *Hook_ConnectClient(const char *pszName, ns_address *pAddr, uint32 steam_handle,
														  C2S_CONNECT_Message *pConnectMsg, const char *pszChallenge, const byte *pAuthTicket,
														  int nAuthTicketLength, bool bIsLowViolence)
{
	g_pKZPlayerManager->OnConnectClient(pszName, pAddr, steam_handle, pConnectMsg, pszChallenge, pAuthTicket, nAuthTicketLength, bIsLowViolence);
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

static_function CServerSideClientBase *Hook_ConnectClientPost(const char *pszName, ns_address *pAddr, uint32 steam_handle,
															  C2S_CONNECT_Message *pConnectMsg, const char *pszChallenge, const byte *pAuthTicket,
															  int nAuthTicketLength, bool bIsLowViolence)
{
	g_pKZPlayerManager->OnConnectClientPost(pszName, pAddr, steam_handle, pConnectMsg, pszChallenge, pAuthTicket, nAuthTicketLength, bIsLowViolence);
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

// IGameSystem
static_function void Hook_ServerGamePostSimulate(const EventServerGamePostSimulate_t *)
{
	ProcessTimers();
	KZGlobalService::OnServerGamePostSimulate();
}

static_function void Hook_BuildGameSessionManifest(const EventBuildGameSessionManifest_t *msg)
{
	Warning("[CS2KZ] IGameSystem::BuildGameSessionManifest\n");
	IEntityResourceManifest *pResourceManifest = msg->m_pResourceManifest;
	if (g_KZPlugin.IsAddonMounted())
	{
		Warning("[CS2KZ] Precache kz soundevents \n");
		pResourceManifest->AddResource(KZ_WORKSHOP_ADDON_SNDEVENT_FILE);
	}
	pResourceManifest->AddResource("particles/ui/hud/ui_map_def_utility_trail.vpcf");
	pResourceManifest->AddResource("particles/ui/annotation/ui_annotation_line_segment.vpcf");
}

static_function ILoadingSpawnGroup *Hook_OnCreateLoadingSpawnGroupHook(SpawnGroupHandle_t hSpawnGroup, bool bSynchronouslySpawnEntities,
																	   bool bConfirmResourcesLoaded,
																	   const CUtlVector<const CEntityKeyValues *> *pKeyValues)
{
	KZ::mapapi::OnCreateLoadingSpawnGroupHook(pKeyValues);
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

static_function void Hook_OnPlayerRunCommand(PlayerCommand *pCmd)
{
	CCSPlayer_MovementServices *pThis = META_IFACEPTR(CCSPlayer_MovementServices);
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(pThis);
	KZ::replaysystem::OnPlayerRunCommandPre(player, pCmd);
	RETURN_META(MRES_IGNORED);
}

static_function void Hook_OnFinishMove(PlayerCommand *pCmd, CMoveData *pMoveData)
{
	CCSPlayer_MovementServices *pThis = META_IFACEPTR(CCSPlayer_MovementServices);
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(pThis);
	KZ::replaysystem::OnFinishMovePre(player, pMoveData);
	RETURN_META(MRES_IGNORED);
}
