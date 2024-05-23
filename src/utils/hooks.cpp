#include "hooks.h"
#include "addresses.h"
#include "bufferstring.h"
#include "igameeventsystem.h"
#include "igamesystem.h"
#include "utils/simplecmds.h"

#include "cs2kz.h"
#include "ctimer.h"
#include "kz/quiet/kz_quiet.h"
#include "kz/timer/kz_timer.h"
#include "utils/utils.h"
#include "entityclass.h"

class GameSessionConfiguration_t
{
};

class EntListener : public IEntityListener
{
	virtual void OnEntitySpawned(CEntityInstance *pEntity);
	virtual void OnEntityDeleted(CEntityInstance *pEntity);
} entityListener;

// CBaseEntity
SH_DECL_MANUALHOOK1_void(StartTouch, 0, 0, 0, CBaseEntity *);
internal void Hook_OnStartTouch(CBaseEntity *pOther);
internal void Hook_OnStartTouchPost(CBaseEntity *pOther);
SH_DECL_MANUALHOOK1_void(Touch, 0, 0, 0, CBaseEntity *);
internal void Hook_OnTouch(CBaseEntity *pOther);
internal void Hook_OnTouchPost(CBaseEntity *pOther);
SH_DECL_MANUALHOOK1_void(EndTouch, 0, 0, 0, CBaseEntity *);
internal void Hook_OnEndTouch(CBaseEntity *pOther);
internal void Hook_OnEndTouchPost(CBaseEntity *pOther);
SH_DECL_MANUALHOOK3_void(Teleport, 0, 0, 0, const Vector *, const QAngle *, const Vector *);
internal void Hook_OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity);

// CCSPlayerController
internal int changeTeamHook {};
SH_DECL_MANUALHOOK1_void(ChangeTeam, 0, 0, 0, int);
internal void Hook_OnChangeTeamPost(i32 team);

// ISource2GameEntities
SH_DECL_HOOK7_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, false, CCheckTransmitInfo **, int, CBitVec<16384> &,
				   const Entity2Networkable_t **, const uint16 *, int, bool);
internal void Hook_CheckTransmit(CCheckTransmitInfo **pInfo, int, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables,
								 const uint16 *pEntityIndicies, int nEntities, bool bEnablePVSBits);

// ISource2Server
SH_DECL_HOOK3_void(ISource2Server, GameFrame, SH_NOATTRIB, false, bool, bool, bool);
internal void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
SH_DECL_HOOK0_void(ISource2Server, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
internal void Hook_GameServerSteamAPIActivated();
SH_DECL_HOOK0_void(ISource2Server, GameServerSteamAPIDeactivated, SH_NOATTRIB, 0);
internal void Hook_GameServerSteamAPIDeactivated();

// ISource2GameClients
SH_DECL_HOOK6(ISource2GameClients, ClientConnect, SH_NOATTRIB, false, bool, CPlayerSlot, const char *, uint64, const char *, bool, CBufferString *);
internal bool Hook_ClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1,
								 CBufferString *pRejectReason);

SH_DECL_HOOK6_void(ISource2GameClients, OnClientConnected, SH_NOATTRIB, false, CPlayerSlot, const char *, uint64, const char *, const char *, bool);
internal void Hook_OnClientConnected(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress,
									 bool bFakePlayer);

SH_DECL_HOOK1_void(ISource2GameClients, ClientFullyConnect, SH_NOATTRIB, false, CPlayerSlot);
internal void Hook_ClientFullyConnect(CPlayerSlot slot);

SH_DECL_HOOK4_void(ISource2GameClients, ClientPutInServer, SH_NOATTRIB, false, CPlayerSlot, char const *, int, uint64);
internal void Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid);

SH_DECL_HOOK4_void(ISource2GameClients, ClientActive, SH_NOATTRIB, false, CPlayerSlot, bool, const char *, uint64);
internal void Hook_ClientActive(CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid);

SH_DECL_HOOK5_void(ISource2GameClients, ClientDisconnect, SH_NOATTRIB, false, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64,
				   const char *);
internal void Hook_ClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID);

SH_DECL_HOOK1_void(ISource2GameClients, ClientVoice, SH_NOATTRIB, false, CPlayerSlot);
internal void Hook_ClientVoice(CPlayerSlot slot);

SH_DECL_HOOK2_void(ISource2GameClients, ClientCommand, SH_NOATTRIB, false, CPlayerSlot, const CCommand &);
internal void Hook_ClientCommand(CPlayerSlot slot, const CCommand &args);

// INetworkServerService
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t &, ISource2WorldSession *, const char *);
internal void Hook_StartupServer(const GameSessionConfiguration_t &config, ISource2WorldSession *, const char *);

// IGameEventManager2
SH_DECL_HOOK2(IGameEventManager2, FireEvent, SH_NOATTRIB, false, bool, IGameEvent *, bool);
internal bool Hook_FireEvent(IGameEvent *event, bool bDontBroadcast);

// ICvar
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandHandle, const CCommandContext &, const CCommand &);
internal void Hook_DispatchConCommand(ConCommandHandle cmd, const CCommandContext &ctx, const CCommand &args);

// IGameEventSystem
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const uint64 *, INetworkSerializable *,
				   const void *, unsigned long, NetChannelBufType_t);
internal void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkSerializable *pEvent,
							 const void *pData, unsigned long nSize, NetChannelBufType_t bufType);

// CEntitySystem
SH_DECL_HOOK2_void(CEntitySystem, Spawn, SH_NOATTRIB, false, int, const EntitySpawnInfo_t *);
internal void Hook_CEntitySystem_Spawn_Post(int nCount, const EntitySpawnInfo_t *pInfo);

// INetworkGameServer
internal int activateServerHook {};
SH_DECL_HOOK0(INetworkGameServer, ActivateServer, SH_NOATTRIB, false, bool);
internal bool Hook_ActivateServer();

// IGameSystem
internal int serverGamePostSimulateHook {};
SH_DECL_HOOK1_void(IGameSystem, ServerGamePostSimulate, SH_NOATTRIB, false, const EventServerGamePostSimulate_t *);
internal void Hook_ServerGamePostSimulate(const EventServerGamePostSimulate_t *);

internal bool ignoreTouchEvent {};

void hooks::Initialize()
{
	SH_MANUALHOOK_RECONFIGURE(StartTouch, g_pGameConfig->GetOffset("StartTouch"), 0, 0);
	SH_MANUALHOOK_RECONFIGURE(Touch, g_pGameConfig->GetOffset("Touch"), 0, 0);
	SH_MANUALHOOK_RECONFIGURE(EndTouch, g_pGameConfig->GetOffset("EndTouch"), 0, 0);
	SH_MANUALHOOK_RECONFIGURE(Teleport, g_pGameConfig->GetOffset("Teleport"), 0, 0);

	SH_MANUALHOOK_RECONFIGURE(ChangeTeam, g_pGameConfig->GetOffset("ControllerChangeTeam"), 0, 0);

	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_STATIC(Hook_CheckTransmit), true);

	SH_ADD_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_ADD_HOOK(ISource2Server, GameServerSteamAPIActivated, interfaces::pServer, SH_STATIC(Hook_GameServerSteamAPIActivated), false);
	SH_ADD_HOOK(ISource2Server, GameServerSteamAPIDeactivated, interfaces::pServer, SH_STATIC(Hook_GameServerSteamAPIDeactivated), false);

	SH_ADD_HOOK(ISource2GameClients, ClientConnect, g_pSource2GameClients, SH_STATIC(Hook_ClientConnect), false);
	SH_ADD_HOOK(ISource2GameClients, OnClientConnected, g_pSource2GameClients, SH_STATIC(Hook_OnClientConnected), false);
	SH_ADD_HOOK(ISource2GameClients, ClientFullyConnect, g_pSource2GameClients, SH_STATIC(Hook_ClientFullyConnect), false);
	SH_ADD_HOOK(ISource2GameClients, ClientPutInServer, g_pSource2GameClients, SH_STATIC(Hook_ClientPutInServer), false);
	SH_ADD_HOOK(ISource2GameClients, ClientActive, g_pSource2GameClients, SH_STATIC(Hook_ClientActive), false);
	SH_ADD_HOOK(ISource2GameClients, ClientDisconnect, g_pSource2GameClients, SH_STATIC(Hook_ClientDisconnect), true);
	SH_ADD_HOOK(ISource2GameClients, ClientVoice, g_pSource2GameClients, SH_STATIC(Hook_ClientVoice), false);
	SH_ADD_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);

	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_STATIC(Hook_StartupServer), true);

	SH_ADD_HOOK(IGameEventManager2, FireEvent, interfaces::pGameEventManager, SH_STATIC(Hook_FireEvent), false);

	SH_ADD_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_STATIC(Hook_DispatchConCommand), false);

	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, interfaces::pGameEventSystem, SH_STATIC(Hook_PostEvent), false);
	// clang-format off
	activateServerHook = SH_ADD_DVPHOOK(
		INetworkGameServer, 
		ActivateServer,
		(INetworkGameServer *)modules::engine->FindVirtualTable("CNetworkGameServer"),
		SH_STATIC(Hook_ActivateServer), 
		true
	);
	serverGamePostSimulateHook = SH_ADD_DVPHOOK(
		IGameSystem, 
		ServerGamePostSimulate, 
		(IGameSystem *)modules::server->FindVirtualTable("CEntityDebugGameSystem"),
		SH_STATIC(Hook_ServerGamePostSimulate), 
		true
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

	SH_REMOVE_HOOK(CEntitySystem, Spawn, GameEntitySystem(), SH_STATIC(Hook_CEntitySystem_Spawn_Post), true);

	SH_REMOVE_HOOK_ID(activateServerHook);

	SH_REMOVE_HOOK_ID(changeTeamHook);

	GameEntitySystem()->RemoveListenerEntity(&entityListener);
}

// Entity hooks
internal void AddEntityHooks(CBaseEntity *entity)
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
			g_pKZPlayerManager->ToPlayer(pawn)->pendingEndTouchTriggers.RemoveAll();
			g_pKZPlayerManager->ToPlayer(pawn)->pendingStartTouchTriggers.RemoveAll();
			g_pKZPlayerManager->ToPlayer(pawn)->touchedTriggers.RemoveAll();
		}
	}
}

internal void RemoveEntityHooks(CBaseEntity *entity)
{
	if (V_strstr(entity->GetClassname(), "trigger_") || !V_stricmp(entity->GetClassname(), "player"))
	{
		SH_REMOVE_MANUALHOOK(StartTouch, entity, SH_STATIC(Hook_OnStartTouch), false);
		SH_REMOVE_MANUALHOOK(Touch, entity, SH_STATIC(Hook_OnTouch), false);
		SH_REMOVE_MANUALHOOK(EndTouch, entity, SH_STATIC(Hook_OnEndTouch), false);
		SH_REMOVE_MANUALHOOK(StartTouch, entity, SH_STATIC(Hook_OnStartTouchPost), true);
		SH_REMOVE_MANUALHOOK(Touch, entity, SH_STATIC(Hook_OnTouchPost), true);
		SH_REMOVE_MANUALHOOK(EndTouch, entity, SH_STATIC(Hook_OnEndTouchPost), true);
		if (V_strstr(entity->GetClassname(), "trigger_"))
		{
			for (u32 i = 0; i <= MAXPLAYERS; i++)
			{
				KZPlayer *player = static_cast<KZPlayer *>(g_pKZPlayerManager->players[i]);
				player->pendingEndTouchTriggers.FindAndRemove(entity->GetRefEHandle());
				player->pendingStartTouchTriggers.FindAndRemove(entity->GetRefEHandle());
				player->touchedTriggers.FindAndRemove(entity->GetRefEHandle());
			}
		}
		else
		{
			SH_REMOVE_MANUALHOOK(Teleport, static_cast<CCSPlayerPawn *>(entity), SH_STATIC(Hook_OnTeleport), false);
		}
	}
}

void EntListener::OnEntitySpawned(CEntityInstance *pEntity)
{
	if (V_strstr(pEntity->GetClassname(), "trigger_"))
	{
		AddEntityHooks(static_cast<CBaseEntity *>(pEntity));
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
internal void Hook_OnStartTouch(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	CCSPlayerPawn *pawn = NULL;
	CBaseTrigger *trigger = NULL;
	if (!V_stricmp(pThis->GetClassname(), "player"))
	{
		pawn = static_cast<CCSPlayerPawn *>(pThis);
		trigger = static_cast<CBaseTrigger *>(pOther);
	}
	else
	{
		pawn = static_cast<CCSPlayerPawn *>(pOther);
		trigger = static_cast<CBaseTrigger *>(pThis);
	}
	if (V_stricmp(pawn->GetClassname(), "player") != 0 || !V_strstr(trigger->GetClassname(), "trigger_"))
	{
		RETURN_META(MRES_IGNORED);
	}
	MovementPlayer *player = g_pKZPlayerManager->ToPlayer(pawn);
	if (!player->OnTriggerStartTouch(trigger))
	{
		ignoreTouchEvent = true;
		RETURN_META(MRES_SUPERCEDE);
	}
	// Don't start touch this trigger twice.
	if (player->touchedTriggers.HasElement(trigger->GetRefEHandle()))
	{
		ignoreTouchEvent = true;
		RETURN_META(MRES_SUPERCEDE);
	}
	// StartTouch is a two way interaction. Are we waiting for this trigger?
	if (player->pendingStartTouchTriggers.HasElement(trigger->GetRefEHandle()))
	{
		player->touchedTriggers.AddToTail(trigger->GetRefEHandle());
		player->pendingStartTouchTriggers.FindAndRemove(trigger->GetRefEHandle());
		RETURN_META(MRES_IGNORED);
	}
	// Must be a new interaction!
	player->pendingStartTouchTriggers.AddToTail(trigger->GetRefEHandle());
	RETURN_META(MRES_IGNORED);
}

internal void Hook_OnStartTouchPost(CBaseEntity *pOther)
{
	if (ignoreTouchEvent)
	{
		ignoreTouchEvent = false;
		RETURN_META(MRES_SUPERCEDE);
	}

	ignoreTouchEvent = false;
	if (V_stricmp(pOther->GetClassname(), "player"))
	{
		RETURN_META(MRES_IGNORED);
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(pOther));
	// This pawn have no controller attached to it. Ignore.
	if (!player)
	{
		RETURN_META(MRES_IGNORED);
	}
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	if (player && !V_stricmp(pThis->GetClassname(), "trigger_multiple"))
	{
		CBaseTrigger *trigger = static_cast<CBaseTrigger *>(pThis);
		if (trigger->IsEndZone())
		{
			player->EndZoneStartTouch();
		}
		else if (trigger->IsStartZone())
		{
			player->StartZoneStartTouch();
		}
	}
	RETURN_META(MRES_IGNORED);
}

internal void Hook_OnTouch(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	CCSPlayerPawn *pawn = NULL;
	CBaseTrigger *trigger = NULL;
	if (!V_stricmp(pThis->GetClassname(), "player"))
	{
		pawn = static_cast<CCSPlayerPawn *>(pThis);
		trigger = static_cast<CBaseTrigger *>(pOther);
	}
	else
	{
		pawn = static_cast<CCSPlayerPawn *>(pOther);
		trigger = static_cast<CBaseTrigger *>(pThis);
	}
	if (V_stricmp(pawn->GetClassname(), "player") != 0 || !V_strstr(trigger->GetClassname(), "trigger_"))
	{
		RETURN_META(MRES_IGNORED);
	}
	MovementPlayer *player = g_pKZPlayerManager->ToPlayer(pawn);
	// This pawn have no controller attached to it. Ignore.
	if (!player)
	{
		RETURN_META(MRES_IGNORED);
	}
	if (!player->OnTriggerTouch(trigger))
	{
		ignoreTouchEvent = true;
		RETURN_META(MRES_SUPERCEDE);
	}

	if (player->touchedTriggers.HasElement(trigger->GetRefEHandle()))
	{
		RETURN_META(MRES_IGNORED);
	}
	// Can't "touch" what isn't in the touch list.
	ignoreTouchEvent = true;
	RETURN_META(MRES_SUPERCEDE);
}

internal void Hook_OnTouchPost(CBaseEntity *pOther)
{
	if (ignoreTouchEvent)
	{
		ignoreTouchEvent = false;
		RETURN_META(MRES_SUPERCEDE);
	}
	ignoreTouchEvent = false;
	RETURN_META(MRES_IGNORED);
}

internal void Hook_OnEndTouch(CBaseEntity *pOther)
{
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	CCSPlayerPawn *pawn = NULL;
	CBaseTrigger *trigger = NULL;
	if (!V_stricmp(pThis->GetClassname(), "player"))
	{
		pawn = static_cast<CCSPlayerPawn *>(pThis);
		trigger = static_cast<CBaseTrigger *>(pOther);
	}
	else
	{
		pawn = static_cast<CCSPlayerPawn *>(pOther);
		trigger = static_cast<CBaseTrigger *>(pThis);
	}
	if (V_stricmp(pawn->GetClassname(), "player") != 0 || !V_strstr(trigger->GetClassname(), "trigger_"))
	{
		RETURN_META(MRES_IGNORED);
	}
	MovementPlayer *player = g_pKZPlayerManager->ToPlayer(pawn);
	// This pawn have no controller attached to it. Ignore.
	if (!player)
	{
		RETURN_META(MRES_IGNORED);
	}
	if (!player->OnTriggerEndTouch(trigger))
	{
		ignoreTouchEvent = true;
		RETURN_META(MRES_SUPERCEDE);
	}
	if (player->touchedTriggers.FindAndRemove(trigger->GetRefEHandle()))
	{
		player->pendingEndTouchTriggers.AddToTail(trigger->GetRefEHandle());
		RETURN_META(MRES_IGNORED);
	}
	if (player->pendingEndTouchTriggers.FindAndRemove(trigger->GetRefEHandle()))
	{
		RETURN_META(MRES_IGNORED);
	}
	// Can't end touch on something we never touched in the first place.
	ignoreTouchEvent = true;
	RETURN_META(MRES_SUPERCEDE);
}

internal void Hook_OnEndTouchPost(CBaseEntity *pOther)
{
	if (ignoreTouchEvent)
	{
		ignoreTouchEvent = false;
		RETURN_META(MRES_SUPERCEDE);
	}

	ignoreTouchEvent = false;
	if (V_stricmp(pOther->GetClassname(), "player"))
	{
		RETURN_META(MRES_IGNORED);
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(pOther));
	// This pawn have no controller attached to it. Ignore.
	if (!player)
	{
		RETURN_META(MRES_IGNORED);
	}
	CBaseEntity *pThis = META_IFACEPTR(CBaseEntity);
	if (player && !V_stricmp(pThis->GetClassname(), "trigger_multiple") && static_cast<CBaseTrigger *>(pThis)->IsStartZone())
	{
		player->StartZoneEndTouch();
	}
	RETURN_META(MRES_IGNORED);
}

internal void Hook_OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
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
internal void Hook_OnChangeTeamPost(i32 team)
{
	CCSPlayerController *controller = META_IFACEPTR(CCSPlayerController);
	MovementPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (player)
	{
		player->OnChangeTeamPost(team);
	}
}

// ISource2GameEntities
internal void Hook_CheckTransmit(CCheckTransmitInfo **pInfo, int infoCount, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables,
								 const uint16 *pEntityIndicies, int nEntities, bool bEnablePVSBits)
{
	KZ::quiet::OnCheckTransmit(pInfo, infoCount);
	RETURN_META(MRES_IGNORED);
}

// ISource2Server
internal void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick)
{
	g_KZPlugin.serverGlobals = *(g_pKZUtils->GetGlobals());
	local_persist int entitySystemHook {};
	if (GameEntitySystem() && !entitySystemHook)
	{
		entitySystemHook = SH_ADD_HOOK(CEntitySystem, Spawn, GameEntitySystem(), SH_STATIC(Hook_CEntitySystem_Spawn_Post), true);
	}
	RETURN_META(MRES_IGNORED);
}

internal void Hook_GameServerSteamAPIActivated() {}

internal void Hook_GameServerSteamAPIDeactivated() {}

// ISource2GameClients
internal bool Hook_ClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1,
								 CBufferString *pRejectReason)
{
	g_pKZPlayerManager->OnClientConnect(slot, pszName, xuid, pszNetworkID, unk1, pRejectReason);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

internal void Hook_OnClientConnected(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, const char *pszAddress,
									 bool bFakePlayer)
{
	g_pKZPlayerManager->OnClientConnected(slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer);
	RETURN_META(MRES_IGNORED);
}

internal void Hook_ClientFullyConnect(CPlayerSlot slot)
{
	g_pKZPlayerManager->OnClientFullyConnect(slot);
	RETURN_META(MRES_IGNORED);
}

internal void Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid)
{
	g_pKZPlayerManager->OnClientPutInServer(slot, pszName, type, xuid);
	RETURN_META(MRES_IGNORED);
}

internal void Hook_ClientActive(CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid)
{
	g_pKZPlayerManager->OnClientActive(slot, bLoadGame, pszName, xuid);
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (player->GetPlayerPawn())
	{
		AddEntityHooks(player->GetPlayerPawn());
	}
	else
	{
		Warning("WARNING: Player pawn for slot %i not found!\n", slot.Get());
	}
	RETURN_META(MRES_IGNORED);
}

internal void Hook_ClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID)
{
	g_pKZPlayerManager->OnClientDisconnect(slot, reason, pszName, xuid, pszNetworkID);
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (player->GetPlayerPawn())
	{
		RemoveEntityHooks(player->GetPlayerPawn());
	}
	else
	{
		Warning("WARNING: Player pawn for slot %i not found!\n", slot.Get());
	}
	player->timerService->OnClientDisconnect();
	RETURN_META(MRES_IGNORED);
}

internal void Hook_ClientVoice(CPlayerSlot slot)
{
	g_pKZPlayerManager->OnClientVoice(slot);
}

internal void Hook_ClientCommand(CPlayerSlot slot, const CCommand &args)
{
	if (META_RES result = scmd::OnClientCommand(slot, args))
	{
		RETURN_META(result);
	}
	RETURN_META(MRES_IGNORED);
}

// INetworkServerService
internal void Hook_StartupServer(const GameSessionConfiguration_t &config, ISource2WorldSession *, const char *)
{
	g_KZPlugin.AddonInit();
	RETURN_META(MRES_IGNORED);
}

// IGameEventManager2
internal bool Hook_FireEvent(IGameEvent *event, bool bDontBroadcast)
{
	if (event)
	{
		if (V_stricmp(event->GetName(), "player_death") == 0)
		{
			CEntityInstance *instance = event->GetPlayerPawn("userid");
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(instance->GetEntityIndex());
			if (player)
			{
				player->timerService->OnPlayerDeath();
				player->quietService->SendFullUpdate();
			}
		}
		else if (V_stricmp(event->GetName(), "round_start") == 0)
		{
			interfaces::pEngine->ServerCommand("sv_full_alltalk 1");
			KZTimerService::OnRoundStart();
			hooks::HookEntities();
		}
		else if (V_stricmp(event->GetName(), "player_team") == 0)
		{
			event->SetBool("silent", true);
		}
		else if (V_stricmp(event->GetName(), "player_spawn") == 0)
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
internal void Hook_DispatchConCommand(ConCommandHandle cmd, const CCommandContext &ctx, const CCommand &args)
{
	META_RES mres = scmd::OnDispatchConCommand(cmd, ctx, args);

	RETURN_META(mres);
}

// IGameEventSystem
internal void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients, INetworkSerializable *pEvent,
							 const void *pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	KZ::quiet::OnPostEvent(pEvent, pData, clients);
}

// CEntitySystem
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

// INetworkGameServer
internal bool Hook_ActivateServer()
{
	local_persist bool infiniteAmmoUnlocked {};
	if (!infiniteAmmoUnlocked)
	{
		infiniteAmmoUnlocked = true;
		auto cvarHandle = g_pCVar->FindConVar("sv_infinite_ammo");
		if (cvarHandle.IsValid())
		{
			g_pCVar->GetConVar(cvarHandle)->flags &= ~FCVAR_CHEAT;
		}
		else
		{
			META_CONPRINTF("Warning: sv_infinite_ammo is not found!\n");
		}
	}

	interfaces::pEngine->ServerCommand("exec cs2kz.cfg");
	RETURN_META_VALUE(MRES_IGNORED, 1);
}

// IGameSystem
internal void Hook_ServerGamePostSimulate(const EventServerGamePostSimulate_t *)
{
	ProcessTimers();
}
