#include "hooks.h"

#include "utils/simplecmds.h"

#include "kz/quiet/kz_quiet.h"

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
	INetworkSerializable *, const void *, unsigned long, NetChannelBufType_t);
SH_DECL_HOOK1_void(CBaseEntity2, StartTouch, SH_NOATTRIB, false, CBaseEntity2 *);
SH_DECL_HOOK1_void(CBaseEntity2, Touch, SH_NOATTRIB, false, CBaseEntity2 *);
SH_DECL_HOOK1_void(CBaseEntity2, EndTouch, SH_NOATTRIB, false, CBaseEntity2 *);

void hooks::Initialize()
{
	SH_ADD_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);
	SH_ADD_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_STATIC(Hook_CheckTransmit), true);
	SH_ADD_HOOK(ISource2GameClients, ClientPutInServer, g_pSource2GameClients, SH_STATIC(Hook_ClientPutInServer), false);
	SH_ADD_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_STATIC(Hook_StartupServer), true);
	SH_ADD_HOOK(IGameEventManager2, FireEvent, interfaces::pGameEventManager, SH_STATIC(Hook_FireEvent), false);
	SH_ADD_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_STATIC(Hook_DispatchConCommand), false);
	SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, interfaces::pGameEventSystem, SH_STATIC(Hook_PostEvent), false);
}

void hooks::Cleanup()
{
	SH_REMOVE_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);
	SH_REMOVE_HOOK(ISource2Server, GameFrame, interfaces::pServer, SH_STATIC(Hook_GameFrame), false);
	SH_REMOVE_HOOK(CEntitySystem, Spawn, g_pEntitySystem, SH_STATIC(Hook_CEntitySystem_Spawn_Post), true);
	SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_STATIC(Hook_CheckTransmit), true);
	SH_REMOVE_HOOK(ISource2GameClients, ClientPutInServer, g_pSource2GameClients, SH_STATIC(Hook_ClientPutInServer), false);
	SH_REMOVE_HOOK(INetworkServerService, StartupServer, g_pNetworkServerService, SH_STATIC(Hook_StartupServer), true);
	SH_REMOVE_HOOK(IGameEventManager2, FireEvent, interfaces::pGameEventManager, SH_STATIC(Hook_FireEvent), false);
	SH_REMOVE_HOOK(ICvar, DispatchConCommand, g_pCVar, SH_STATIC(Hook_DispatchConCommand), false);
	SH_REMOVE_HOOK(IGameEventSystem, PostEventAbstract, interfaces::pGameEventSystem, SH_STATIC(Hook_PostEvent), false);
}

void hooks::HookEntities()
{
	FOR_EACH_VEC(hooks::entityTouchHooks, i)
	{
		SH_REMOVE_HOOK_ID(hooks::entityTouchHooks[i]);
	}
	hooks::entityTouchHooks.Purge();
	for (CEntityIdentity *entID = GameEntitySystem()->m_EntityList.m_pFirstActiveEntity; entID != NULL; entID = entID->m_pNext)
	{
		if (V_strstr(entID->GetClassname(), "trigger_") || !V_stricmp(entID->GetClassname(), "player"))
		{
			CBaseEntity2 *ent = static_cast<CBaseEntity2 *>(entID->m_pInstance);
			hooks::entityTouchHooks.AddToTail(SH_ADD_HOOK(CBaseEntity2, StartTouch, ent, SH_STATIC(OnStartTouch), false));
			hooks::entityTouchHooks.AddToTail(SH_ADD_HOOK(CBaseEntity2, Touch, ent, SH_STATIC(OnTouch), false));
			hooks::entityTouchHooks.AddToTail(SH_ADD_HOOK(CBaseEntity2, EndTouch, ent, SH_STATIC(OnEndTouch), false));
			if (!V_stricmp(entID->GetClassname(), "player"))
			{
				g_pPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(entID->m_pInstance))->pendingEndTouchTriggers.RemoveAll();
				g_pPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(entID->m_pInstance))->pendingStartTouchTriggers.RemoveAll();
				g_pPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(entID->m_pInstance))->touchedTriggers.RemoveAll();
			}
			META_CONPRINTF("Hooked %i %s!\n", entID->GetEntityIndex(), entID->GetClassname());
		}
	}
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
		else if (V_stricmp(event->GetName(), "round_start") == 0)
		{
			interfaces::pEngine->ServerCommand("sv_full_alltalk 1");
			hooks::HookEntities();
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

internal void OnStartTouch(CBaseEntity2 *pOther)
{
	CBaseEntity2 *pThis = META_IFACEPTR(CBaseEntity2);
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
	MovementPlayer *player = g_pPlayerManager->ToPlayer(pawn);
	META_CONPRINTF("[%.1f] StartTouch: %s - %s %i!\n", g_pKZUtils->GetServerGlobals()->curtime * 64.0f, pThis->GetClassname(), pOther->GetClassname(), player->touchedTriggers.Count());
	// Don't start touch this trigger twice.
	if (player->touchedTriggers.HasElement(trigger->GetRefEHandle()))
	{
		RETURN_META(MRES_SUPERCEDE);
	}
	// StartTouch is a two way interaction. Are we waiting for this trigger?
	if (player->pendingStartTouchTriggers.HasElement(trigger->GetRefEHandle()))
	{
		player->touchedTriggers.AddToTail(trigger->GetRefEHandle());
		player->pendingStartTouchTriggers.FindAndRemove(trigger->GetRefEHandle());
		META_CONPRINTF("%s finish starttouch with %s!\n", pThis->GetClassname(), pOther->GetClassname());
		RETURN_META(MRES_IGNORED);
	}
	// Must be a new interaction!
	player->pendingStartTouchTriggers.AddToTail(trigger->GetRefEHandle());
	META_CONPRINTF("%s initiate starttouch with %s!\n", pThis->GetClassname(), pOther->GetClassname());
	RETURN_META(MRES_IGNORED);
}

internal void OnTouch(CBaseEntity2 *pOther)
{
	CBaseEntity2 *pThis = META_IFACEPTR(CBaseEntity2);
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
	MovementPlayer *player = g_pPlayerManager->ToPlayer(pawn);
	META_CONPRINTF("[%.1f] Touch: %s - %s %i!\n", g_pKZUtils->GetServerGlobals()->curtime * 64.0f, pThis->GetClassname(), pOther->GetClassname(), player->touchedTriggers.Count());
	if (player->touchedTriggers.HasElement(trigger->GetRefEHandle()))
	{
		META_CONPRINTF("%s touches %s!\n", pThis->GetClassname(), pOther->GetClassname());
		RETURN_META(MRES_IGNORED);
	}
	// Can't "touch" what isn't in the touch list.
	RETURN_META(MRES_SUPERCEDE);
}

internal void OnEndTouch(CBaseEntity2 *pOther)
{
	CBaseEntity2 *pThis = META_IFACEPTR(CBaseEntity2);
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
	MovementPlayer *player = g_pPlayerManager->ToPlayer(pawn);
	META_CONPRINTF("[%.1f] EndTouch: %s - %s %i!\n", g_pKZUtils->GetServerGlobals()->curtime * 64.0f, pThis->GetClassname(), pOther->GetClassname(), player->touchedTriggers.Count());
	if (player->touchedTriggers.FindAndRemove(trigger->GetRefEHandle()))
	{
		META_CONPRINTF("%s initiates endtouch %s!\n", pThis->GetClassname(), pOther->GetClassname());
		player->pendingEndTouchTriggers.AddToTail(trigger->GetRefEHandle());
		RETURN_META(MRES_IGNORED);
	}
	if (player->pendingEndTouchTriggers.FindAndRemove(trigger->GetRefEHandle()))
	{
		META_CONPRINTF("%s finish endtouch %s!\n", pThis->GetClassname(), pOther->GetClassname());
		RETURN_META(MRES_IGNORED);
	}
	// Can't end touch on something we never touched in the first place.
	RETURN_META(MRES_SUPERCEDE);
}
