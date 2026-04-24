
#include "utils/utils.h"
#include "utils/hooks.h"
#include "utils/addresses.h"
#include "utils/simplecmds.h"
#include "utils/gamesystem.h"
#include "utils/gameconfig.h"
#include "utils/ctimer.h"

#include "entityclass.h"
#include "gamesystems/spawngroup_manager.h"
#include "steam/steam_gameserver.h"
#include "bufferstring.h"
#include "igameeventsystem.h"
#include "igamesystem.h"
#include "entityclass.h"
#include "gamesystems/spawngroup_manager.h"
#include "utils/simplecmds.h"
#include "utils/gamesystem.h"
#include "utils/async_file_io.h"
#include "steam/steam_gameserver.h"

#include "sdk/steamnetworkingsockets.h"

#include "kz/kz.h"
#include "kz/beam/kz_beam.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/option/kz_option.h"
#include "kz/paint/kz_paint.h"
#include "kz/quiet/kz_quiet.h"
#include "kz/timer/kz_timer.h"
#include "kz/timer/submission.h"
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
#include "kz/racing/kz_racing.h"

#include "vprof.h"
#ifdef DEBUG_TPM
#include "fmtstr.h"
#endif

#include "memdbgon.h"

extern CSteamGameServerAPIContext g_steamAPI;
extern CGameConfig *g_pGameConfig;

class GameSessionConfiguration_t
{
};

class EntListener : public IEntityListener
{
	virtual void OnEntityCreated(CEntityInstance *pEntity);
	virtual void OnEntitySpawned(CEntityInstance *pEntity);
	virtual void OnEntityDeleted(CEntityInstance *pEntity);
} entityListener;

static_global bool ignoreTouchEvent {};

static MovementPlayerManager *playerManager;

#ifdef DEBUG_TPM
static bool g_traceShapeEnabled = false;
CUtlVector<TraceHistory> traceHistory;
#endif

// ============================================================
// Entity hooks
// ============================================================

static KHook::Return<void> StartTouchPre(CBaseEntity *pThis, CBaseEntity *pOther)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		return {KHook::Action::Supersede};
	}
	return {KHook::Action::Ignore};
}

static KHook::Return<void> StartTouchPost(CBaseEntity *pThis, CBaseEntity *pOther)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		return {KHook::Action::Supersede};
	}
	return {KHook::Action::Ignore};
}

static KHook::Virtual<CBaseEntity, void, CBaseEntity *> startTouchHook(StartTouchPre, StartTouchPost);

static KHook::Return<void> TouchPre(CBaseEntity *pThis, CBaseEntity *pOther)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	// If it's a player touching trigger_push, we still need to disable jumpstats.
	KZPlayer *player = nullptr;
	if (KZ_STREQI(pThis->GetClassname(), "trigger_push") && KZ_STREQI(pOther->GetClassname(), "player"))
	{
		player = g_pKZPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(pOther));
	}
	else if (KZ_STREQI(pThis->GetClassname(), "player") && KZ_STREQI(pOther->GetClassname(), "trigger_push"))
	{
		player = g_pKZPlayerManager->ToPlayer(static_cast<CCSPlayerPawn *>(pThis));
	}
	if (player)
	{
		player->jumpstatsService->InvalidateJumpstats("Base velocity detected");
	}
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		return {KHook::Action::Supersede};
	}
	return {KHook::Action::Ignore};
}

static KHook::Return<void> TouchPost(CBaseEntity *pThis, CBaseEntity *pOther)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		return {KHook::Action::Supersede};
	}
	return {KHook::Action::Ignore};
}

static KHook::Virtual<CBaseEntity, void, CBaseEntity *> touchHook(TouchPre, TouchPost);

static KHook::Return<void> EndTouchPre(CBaseEntity *pThis, CBaseEntity *pOther)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		return {KHook::Action::Supersede};
	}
	return {KHook::Action::Ignore};
}

static KHook::Return<void> EndTouchPost(CBaseEntity *pThis, CBaseEntity *pOther)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (KZTriggerService::IsManagedByTriggerService(pThis, pOther) && !g_KZPlugin.simulatingPhysics)
	{
		return {KHook::Action::Supersede};
	}
	return {KHook::Action::Ignore};
}

static KHook::Virtual<CBaseEntity, void, CBaseEntity *> endTouchHook(EndTouchPre, EndTouchPost);

void hooks::CallOriginalStartTouch(CBaseEntity *pThis, CBaseEntity *pOther)
{
	startTouchHook.CallOriginal(pThis, pOther);
}

void hooks::CallOriginalTouch(CBaseEntity *pThis, CBaseEntity *pOther)
{
	touchHook.CallOriginal(pThis, pOther);
}

void hooks::CallOriginalEndTouch(CBaseEntity *pThis, CBaseEntity *pOther)
{
	endTouchHook.CallOriginal(pThis, pOther);
}

static KHook::Return<void> TeleportPre(CBaseEntity *pThis, const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (pThis->IsPawn())
	{
		MovementPlayer *player = g_pKZPlayerManager->ToPlayer(static_cast<CBasePlayerPawn *>(pThis));
		player->OnTeleport(newPosition, newAngles, newVelocity);
	}
	return {KHook::Action::Ignore};
}

static KHook::Virtual<CBaseEntity, void, const Vector *, const QAngle *, const Vector *> teleportHook(TeleportPre, nullptr);

static KHook::Return<void> ChangeTeamPost(CCSPlayerController *pThis, i32 team)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = g_pKZPlayerManager->ToPlayer(pThis);
	if (player)
	{
		player->OnChangeTeamPost(team);
	}
	return {KHook::Action::Ignore};
}

static KHook::Virtual<CCSPlayerController, void, i32> changeTeamHook(nullptr, ChangeTeamPost);

// ============================================================
// ISource2GameEntities hooks
// ============================================================

static KHook::Return<void> CheckTransmitPost(ISource2GameEntities *pThis, CCheckTransmitInfo **pInfos, int infoCount, CBitVec<16384> &unk1,
											 CBitVec<16384> &unk2, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies,
											 int nEntities)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	KZ::quiet::OnCheckTransmit(pInfos, infoCount);
	KZProfileService::OnCheckTransmit();
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2GameEntities, void, CCheckTransmitInfo **, int, CBitVec<16384> &, CBitVec<16384> &, const Entity2Networkable_t **,
					  const uint16 *, int>
	checkTransmitHook(nullptr, CheckTransmitPost);

// ============================================================
// ISource2Server hooks
// ============================================================

static KHook::Return<void> GameFramePre(ISource2Server *pThis, bool simulating, bool bFirstTick, bool bLastTick)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_KZPlugin.serverGlobals = *(g_pKZUtils->GetGlobals());
	g_pKZPlayerManager->PerformAuthChecks();
	RunSubmission::CheckAll();
	BaseRequest::CheckRequests();
	KZTelemetryService::ActiveCheck();
	KZBeamService::UpdateBeams();
	KZPaintService::OnGameFrame();
	KZProfileService::OnGameFrame();
	KZ::replaysystem::OnGameFrame();
	KZRacingService::BroadcastRaceInfo();
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2Server, void, bool, bool, bool> gameFrameHook(GameFramePre, nullptr);

static KHook::Return<void> GameServerSteamAPIActivatedPre(ISource2Server *pThis)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_steamAPI.Init();
	g_pKZPlayerManager->OnSteamAPIActivated();
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2Server, void> gameServerSteamAPIActivatedHook(GameServerSteamAPIActivatedPre, nullptr);

static KHook::Return<void> GameServerSteamAPIDeactivatedPre(ISource2Server *pThis)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2Server, void> gameServerSteamAPIDeactivatedHook(GameServerSteamAPIDeactivatedPre, nullptr);

// ============================================================
// ISource2GameClients hooks
// ============================================================

static KHook::Return<bool> ClientConnectPre(ISource2GameClients *pThis, CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID,
											bool unk1, CBufferString *pRejectReason)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_pKZPlayerManager->OnClientConnect(slot, pszName, xuid, pszNetworkID, unk1, pRejectReason);
	return {KHook::Action::Ignore, true};
}

static KHook::Virtual<ISource2GameClients, bool, CPlayerSlot, const char *, uint64, const char *, bool, CBufferString *>
	clientConnectHook(ClientConnectPre, nullptr);

static KHook::Return<void> OnClientConnectedPre(ISource2GameClients *pThis, CPlayerSlot slot, const char *pszName, uint64 xuid,
												const char *pszNetworkID, const char *pszAddress, bool bFakePlayer)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_pKZPlayerManager->OnClientConnected(slot, pszName, xuid, pszNetworkID, pszAddress, bFakePlayer);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2GameClients, void, CPlayerSlot, const char *, uint64, const char *, const char *, bool>
	onClientConnectedHook(OnClientConnectedPre, nullptr);

static KHook::Return<void> ClientFullyConnectPre(ISource2GameClients *pThis, CPlayerSlot slot)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_pKZPlayerManager->OnClientFullyConnect(slot);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2GameClients, void, CPlayerSlot> clientFullyConnectHook(ClientFullyConnectPre, nullptr);

static KHook::Return<void> ClientPutInServerPre(ISource2GameClients *pThis, CPlayerSlot slot, char const *pszName, int type, uint64 xuid)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_pKZPlayerManager->OnClientPutInServer(slot, pszName, type, xuid);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2GameClients, void, CPlayerSlot, char const *, int, uint64> clientPutInServerHook(ClientPutInServerPre, nullptr);

static KHook::Return<void> ClientActivePost(ISource2GameClients *pThis, CPlayerSlot slot, bool bLoadGame, const char *pszName, uint64 xuid)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_pKZPlayerManager->OnClientActive(slot, bLoadGame, pszName, xuid);
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (player->GetPlayerPawn())
	{
		hooks::AddEntityHooks(player->GetPlayerPawn());
	}
	else
	{
		Warning("[KZ] WARNING: Player pawn for slot %i not found!\n", slot.Get());
	}
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2GameClients, void, CPlayerSlot, bool, const char *, uint64> clientActiveHook(nullptr, ClientActivePost);

static KHook::Return<void> ClientDisconnectPost(ISource2GameClients *pThis, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName,
												uint64 xuid, const char *pszNetworkID)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (player->GetController())
	{
		player->GetController()->m_LastTimePlayerWasDisconnectedForPawnsRemove().SetTime(0.01f);
		player->GetController()->SwitchTeam(0);
	}
	if (player->GetPlayerPawn())
	{
		hooks::RemoveEntityHooks(player->GetPlayerPawn());
	}
	else
	{
		Warning("WARNING: Player pawn for slot %i not found!\n", slot.Get());
	}
	player->timerService->OnClientDisconnect();
	player->recordingService->OnClientDisconnect();
	player->optionService->OnClientDisconnect();
	player->racingService->OnClientDisconnect();
	player->globalService->OnClientDisconnect();
	g_pKZPlayerManager->OnClientDisconnect(slot, reason, pszName, xuid, pszNetworkID);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2GameClients, void, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *>
	clientDisconnectHook(nullptr, ClientDisconnectPost);

static KHook::Return<void> ClientVoicePre(ISource2GameClients *pThis, CPlayerSlot slot)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_pKZPlayerManager->OnClientVoice(slot);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2GameClients, void, CPlayerSlot> clientVoiceHook(ClientVoicePre, nullptr);

static KHook::Return<void> ClientCommandPre(ISource2GameClients *pThis, CPlayerSlot slot, const CCommand &args)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (KZ::misc::CheckBlockedRadioCommands(args[0]))
	{
		return {KHook::Action::Supersede};
	}
	if (scmd::OnClientCommand(slot, args))
	{
		return {KHook::Action::Supersede};
	}
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ISource2GameClients, void, CPlayerSlot, const CCommand &> clientCommandHook(ClientCommandPre, nullptr);

// ============================================================
// INetworkServerService hooks
// ============================================================

static KHook::Return<void> StartupServerPost(INetworkServerService *pThis, const GameSessionConfiguration_t &config, ISource2WorldSession *,
											 const char *)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_KZPlugin.AddonInit();
	KZ::course::ClearCourses();
	KZ::mapapi::Init();
	KZ::replaysystem::Init();
	return {KHook::Action::Ignore};
}

static KHook::Virtual<INetworkServerService, void, const GameSessionConfiguration_t &, ISource2WorldSession *, const char *>
	startupServerHook(nullptr, StartupServerPost);

// ============================================================
// IGameEventManager2 hooks
// ============================================================

static KHook::Return<bool> FireEventPre(IGameEventManager2 *pThis, IGameEvent *event, bool bDontBroadcast)
{
	VPROF_BUDGET(__func__, "CS2KZ");
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
	return {KHook::Action::Ignore, true};
}

static KHook::Virtual<IGameEventManager2, bool, IGameEvent *, bool> fireEventHook(FireEventPre, nullptr);

// ============================================================
// ICvar hooks
// ============================================================

static KHook::Return<void> DispatchConCommandPre(ICvar *pThis, ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (KZ::misc::CheckBlockedRadioCommands(args[0]))
	{
		return {KHook::Action::Supersede};
	}
	if (KZOptionService::GetOptionInt("overridePlayerChat", true))
	{
		KZ::misc::ProcessConCommand(cmd, ctx, args);
	}
	if (scmd::OnDispatchConCommand(cmd, ctx, args))
	{
		return {KHook::Action::Supersede};
	}
	return {KHook::Action::Ignore};
}

static KHook::Virtual<ICvar, void, ConCommandRef, const CCommandContext &, const CCommand &> dispatchConCommandHook(DispatchConCommandPre, nullptr);

// ============================================================
// IGameEventSystem hooks
// ============================================================

static KHook::Return<void> PostEventPre(IGameEventSystem *pThis, CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients,
										INetworkMessageInternal *pEvent, const CNetMessage *pData, unsigned long nSize, NetChannelBufType_t bufType)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	KZ::quiet::OnPostEvent(pEvent, pData, clients);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<IGameEventSystem, void, CSplitScreenSlot, bool, int, const uint64 *, INetworkMessageInternal *, const CNetMessage *,
					  unsigned long, NetChannelBufType_t>
	postEventHook(PostEventPre, nullptr);

// ============================================================
// CEntitySystem hooks
// ============================================================

static KHook::Return<void> SpawnPre(CEntitySystem *pThis, int nCount, const EntitySpawnInfo_t *pInfo)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	KZ::mapapi::OnSpawn(nCount, pInfo);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<CEntitySystem, void, int, const EntitySpawnInfo_t *> entitySystemSpawnHook(SpawnPre, nullptr);

// ============================================================
// CSpawnGroupMgrGameSystem hooks
// ============================================================

static KHook::Return<ILoadingSpawnGroup *> CreateLoadingSpawnGroupPre(CSpawnGroupMgrGameSystem *pThis, SpawnGroupHandle_t hSpawnGroup,
																	  bool bSynchronouslySpawnEntities, bool bConfirmResourcesLoaded,
																	  const CUtlVector<const CEntityKeyValues *> *pKeyValues)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	KZ::mapapi::OnCreateLoadingSpawnGroupHook(pKeyValues);
	return {KHook::Action::Ignore, nullptr};
}

static KHook::Virtual<CSpawnGroupMgrGameSystem, ILoadingSpawnGroup *, SpawnGroupHandle_t, bool, bool, const CUtlVector<const CEntityKeyValues *> *>
	createLoadingSpawnGroupHook(CreateLoadingSpawnGroupPre, nullptr);

// ============================================================
// CNetworkGameServerBase hooks
// ============================================================

static KHook::Return<bool> ActivateServerPost(CNetworkGameServerBase *pThis)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (!interfaces::pEngine->IsDedicatedServer())
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(0));
		player->Reset();
	}
	u64 id = g_pKZUtils->GetCurrentMapWorkshopID();
	u64 size = g_pKZUtils->GetCurrentMapSize();

	KZ_LOG_INFO(LogChannel::General, "Loading map %s, workshop ID %llu, size %llu\n", g_pKZUtils->GetCurrentMapVPK().Get(), id, size);

	RunSubmission::Clear();
	KZ::misc::OnActivateServer();
	KZDatabaseService::SetupMap();
	KZRecordingService::OnActivateServer();
	KZRacingService::OnActivateServer();
	KZGlobalService::OnActivateServer();

	char md5[33];
	g_pKZUtils->GetCurrentMapMD5(md5, sizeof(md5));
	KZ_LOG_INFO(LogChannel::General, "Map file md5: %s\n", md5);

	return {KHook::Action::Ignore, true};
}

static KHook::Virtual<CNetworkGameServerBase, bool> activateServerHook(nullptr, ActivateServerPost);

static KHook::Return<CServerSideClientBase *> ConnectClientPre(CNetworkGameServerBase *pThis, const char *pszName, ns_address *pAddr,
															   uint32 steam_handle, C2S_CONNECT_Message *pConnectMsg, const char *pszChallenge,
															   const byte *pAuthTicket, int nAuthTicketLength, bool bIsLowViolence)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_pKZPlayerManager->OnConnectClient(pszName, pAddr, steam_handle, pConnectMsg, pszChallenge, pAuthTicket, nAuthTicketLength, bIsLowViolence);
	return {KHook::Action::Ignore, nullptr};
}

static KHook::Return<CServerSideClientBase *> ConnectClientPost(CNetworkGameServerBase *pThis, const char *pszName, ns_address *pAddr,
																uint32 steam_handle, C2S_CONNECT_Message *pConnectMsg, const char *pszChallenge,
																const byte *pAuthTicket, int nAuthTicketLength, bool bIsLowViolence)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	g_pKZPlayerManager->OnConnectClientPost(pszName, pAddr, steam_handle, pConnectMsg, pszChallenge, pAuthTicket, nAuthTicketLength, bIsLowViolence);
	return {KHook::Action::Ignore, nullptr};
}

static KHook::Virtual<CNetworkGameServerBase, CServerSideClientBase *, const char *, ns_address *, uint32, C2S_CONNECT_Message *, const char *,
					  const byte *, int, bool>
	connectClientHook(ConnectClientPre, ConnectClientPost);

// ============================================================
// IGameSystem hooks (CEntityDebugGameSystem vtable)
// ============================================================

static KHook::Return<void> ServerGamePostSimulatePost(IGameSystem *pThis, const EventServerGamePostSimulate_t *)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	ProcessTimers();
	KZRecordingService::ProcessFileWriteCompletion();
	if (g_asyncFileIO)
	{
		g_asyncFileIO->RunFrame();
	}
	KZRacingService::OnServerGamePostSimulate();
	KZGlobalService::OnServerGamePostSimulate();
	return {KHook::Action::Ignore};
}

static KHook::Virtual<IGameSystem, void, const EventServerGamePostSimulate_t *> serverGamePostSimulateHook(nullptr, ServerGamePostSimulatePost);

static KHook::Return<void> BuildGameSessionManifestPost(IGameSystem *pThis, const EventBuildGameSessionManifest_t *msg)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	Warning("[CS2KZ] IGameSystem::BuildGameSessionManifest\n");
	IEntityResourceManifest *pResourceManifest = msg->m_pResourceManifest;
	if (g_KZPlugin.IsAddonMounted())
	{
		Warning("[CS2KZ] Precache kz soundevents \n");
		pResourceManifest->AddResource(KZ_WORKSHOP_ADDON_SNDEVENT_FILE);
	}
	pResourceManifest->AddResource("particles/ui/hud/ui_map_def_utility_trail.vpcf");
	pResourceManifest->AddResource("particles/ui/annotation/ui_annotation_line_segment.vpcf");
	return {KHook::Action::Ignore};
}

static KHook::Virtual<IGameSystem, void, const EventBuildGameSessionManifest_t *> buildGameSessionManifestHook(nullptr, BuildGameSessionManifestPost);

// ============================================================
// CCSPlayer_MovementServices virtual hooks
// ============================================================

static KHook::Return<void> PlayerRunCommandPre(CCSPlayer_MovementServices *pThis, PlayerCommand *pCmd)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(pThis);
	KZ::replaysystem::OnPlayerRunCommandPre(player, pCmd);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<CCSPlayer_MovementServices, void, PlayerCommand *> playerRunCommandHook(PlayerRunCommandPre, nullptr);

static KHook::Return<void> FinishMovePre(CCSPlayer_MovementServices *pThis, PlayerCommand *pCmd, CMoveData *pMoveData)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(pThis);
	KZ::replaysystem::OnFinishMovePre(player, pMoveData);
	return {KHook::Action::Ignore};
}

static KHook::Virtual<CCSPlayer_MovementServices, void, PlayerCommand *, CMoveData *> finishMoveHook(FinishMovePre, nullptr);

// ============================================================
// Signature-based hooks
// ============================================================

static KHook::Return<int> RecvServerBrowserPacketPost(RecvPktInfo_t &info, void *pSock)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	return {KHook::Action::Ignore};
}

static KHook::Function<int, RecvPktInfo_t &, void *> RecvServerBrowserPacket(nullptr, RecvServerBrowserPacketPost);

#ifdef DEBUG_TPM
static KHook::Return<bool> TraceShapePost(const void *physicsQuery, const Ray_t &ray, const Vector &start, const Vector &end,
										  const CTraceFilter *pTraceFilter, trace_t *pm)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (!g_traceShapeEnabled)
	{
		return {KHook::Action::Ignore};
	}
	bool ret = *(bool *)KHook::GetOriginalValuePtr();
	f32 error;
	Vector velocity;
	for (u32 i = 0; i < 2; i++)
	{
		if (g_pKZPlayerManager->ToPlayer(i) && g_pKZPlayerManager->ToPlayer(i)->GetMoveServices())
		{
			error = g_pKZPlayerManager->ToPlayer(i)->GetMoveServices()->m_flAccumulatedJumpError();
			velocity = g_pKZPlayerManager->ToPlayer(i)->currentMoveData->m_vecVelocity;
			break;
		}
	}
	traceHistory.AddToTail({start, end, ray, pm->DidHit(), pm->m_vStartPos, pm->m_vEndPos, pm->m_vHitNormal, pm->m_vHitPoint, pm->m_flHitOffset,
							pm->m_flFraction, error, velocity});
	return {KHook::Action::Ignore};
}
#endif

static KHook::Function<bool, const void *, const Ray_t &, const Vector &, const Vector &, const CTraceFilter *, trace_t *> TraceShape(
#ifdef DEBUG_TPM
	nullptr, TraceShapePost
#else
	nullptr, nullptr
#endif
);

static KHook::Return<void> CPhysicsGameSystemFrameBoundaryPost(void *pThis)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	KZ::misc::OnPhysicsGameSystemFrameBoundary(pThis);
	return {KHook::Action::Ignore};
}

static KHook::Function<void, void *> CPhysicsGameSystemFrameBoundary(nullptr, CPhysicsGameSystemFrameBoundaryPost);

static KHook::Return<void> PhysicsSimulatePre(CCSPlayerController *controller)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (controller->m_bIsHLTV)
	{
		return {KHook::Action::Supersede};
	}
	g_KZPlugin.simulatingPhysics = true;
	playerManager->ToPlayer(controller)->OnPhysicsSimulate();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> PhysicsSimulatePost(CCSPlayerController *controller)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	if (controller->m_bIsHLTV)
	{
		return {KHook::Action::Ignore};
	}
	MovementPlayer *player = playerManager->ToPlayer(controller);
	player->OnPhysicsSimulatePost();
	g_KZPlugin.simulatingPhysics = false;
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayerController, void> PhysicsSimulate(PhysicsSimulatePre, PhysicsSimulatePost);

static KHook::Return<i32> ProcessUsercmdsPre(CCSPlayerController *controller, PlayerCommand *cmds, int numcmds, bool paused, float margin);

static KHook::Member<CCSPlayerController, i32, PlayerCommand *, int, bool, float> ProcessUsercmds(ProcessUsercmdsPre, nullptr);

static KHook::Return<i32> ProcessUsercmdsPre(CCSPlayerController *controller, PlayerCommand *cmds, int numcmds, bool paused, float margin)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(controller);
	player->OnProcessUsercmds(cmds, numcmds);
	auto retValue = ProcessUsercmds.CallOriginal(controller, cmds, numcmds, paused, margin);
	player->OnProcessUsercmdsPost(cmds, numcmds);
	return {KHook::Action::Supersede, retValue};
}

static KHook::Return<void> SetupMovePre(CCSPlayer_MovementServices *ms, PlayerCommand *pc, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->currentMoveData = mv;
	player->moveDataPre = CMoveData(*mv);
	player->OnSetupMove(pc);
	return {KHook::Action::Ignore};
}

static KHook::Return<void> SetupMovePost(CCSPlayer_MovementServices *ms, PlayerCommand *pc, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnSetupMovePost(pc);
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, PlayerCommand *, CMoveData *> SetupMove(SetupMovePre, SetupMovePost);

static KHook::Return<void> ProcessMovementPre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->currentMoveData = mv;
	player->moveDataPre = CMoveData(*mv);
	player->OnProcessMovement();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> ProcessMovementPost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->moveDataPost = CMoveData(*mv);
	player->OnProcessMovementPost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *> ProcessMovement(ProcessMovementPre, ProcessMovementPost);

static KHook::Return<bool> PlayerMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnPlayerMove();
	return {KHook::Action::Ignore, false};
}

static KHook::Return<bool> PlayerMovePost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnPlayerMovePost();
	return {KHook::Action::Ignore, false};
}

static KHook::Member<CCSPlayer_MovementServices, bool, CMoveData *> PlayerMove(PlayerMovePre, PlayerMovePost);

static KHook::Return<void> CheckParametersPre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCheckParameters();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> CheckParametersPost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCheckParametersPost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *> CheckParameters(CheckParametersPre, CheckParametersPost);

static KHook::Return<bool> CanMovePre(CCSPlayerPawnBase *pawn)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(pawn)->OnCanMove();
	return {KHook::Action::Ignore, false};
}

static KHook::Return<bool> CanMovePost(CCSPlayerPawnBase *pawn)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(pawn)->OnCanMovePost();
	return {KHook::Action::Ignore, false};
}

static KHook::Member<CCSPlayerPawnBase, bool> CanMove(CanMovePre, CanMovePost);

static KHook::Return<void> FullWalkMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv, bool ground)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnFullWalkMove(ground);
	return {KHook::Action::Ignore};
}

static KHook::Return<void> FullWalkMovePost(CCSPlayer_MovementServices *ms, CMoveData *mv, bool ground)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnFullWalkMovePost(ground);
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *, bool> FullWalkMove(FullWalkMovePre, FullWalkMovePost);

static KHook::Return<bool> MoveInitPre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnMoveInit();
	return {KHook::Action::Ignore, false};
}

static KHook::Return<bool> MoveInitPost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnMoveInitPost();
	return {KHook::Action::Ignore, false};
}

static KHook::Member<CCSPlayer_MovementServices, bool, CMoveData *> MoveInit(MoveInitPre, MoveInitPost);

static KHook::Return<bool> CheckWaterPre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCheckWater();
	return {KHook::Action::Ignore, false};
}

static KHook::Return<bool> CheckWaterPost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCheckWaterPost();
	return {KHook::Action::Ignore, false};
}

static KHook::Member<CCSPlayer_MovementServices, bool, CMoveData *> CheckWater(CheckWaterPre, CheckWaterPost);

static KHook::Return<void> WaterMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnWaterMove();
#ifdef WATER_FIX
	if (player->enableWaterFix)
	{
		player->ignoreNextCategorizePosition = true;
	}
#endif
	return {KHook::Action::Ignore};
}

static KHook::Return<void> WaterMovePost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnWaterMovePost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *> WaterMove(WaterMovePre, WaterMovePost);

static KHook::Return<void> CheckVelocityPre(CCSPlayer_MovementServices *ms, CMoveData *mv, const char *a3)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCheckVelocity(a3);
	return {KHook::Action::Ignore};
}

static KHook::Return<void> CheckVelocityPost(CCSPlayer_MovementServices *ms, CMoveData *mv, const char *a3)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCheckVelocityPost(a3);
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *, const char *> CheckVelocity(CheckVelocityPre, CheckVelocityPost);

static KHook::Return<void> DuckPre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnDuck();
	player->processingDuck = true;
	return {KHook::Action::Ignore};
}

static KHook::Return<void> DuckPost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->processingDuck = false;
	player->OnDuckPost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *> Duck(DuckPre, DuckPost);

static KHook::Return<bool> CanUnduckPre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCanUnduck();
	return {KHook::Action::Ignore, false};
}

static KHook::Return<bool> CanUnduckPost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	bool canUnduck = *(bool *)KHook::GetOriginalValuePtr();
	player->OnCanUnduckPost(canUnduck);
	return {KHook::Action::Ignore, canUnduck};
}

static KHook::Member<CCSPlayer_MovementServices, bool, CMoveData *> CanUnduck(CanUnduckPre, CanUnduckPost);

static KHook::Return<bool> LadderMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv);

static KHook::Member<CCSPlayer_MovementServices, bool, CMoveData *> LadderMove(LadderMovePre, nullptr);

static KHook::Return<bool> LadderMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnLadderMove();
	Vector oldVelocity = mv->m_vecVelocity;
	MoveType_t oldMoveType = player->GetPlayerPawn()->m_MoveType();
	bool result = LadderMove.CallOriginal(ms, mv);
	if (player->GetPlayerPawn()->m_lifeState() != LIFE_DEAD && !result && oldMoveType == MOVETYPE_LADDER)
	{
		player->SetMoveType(MOVETYPE_WALK, false);
	}
	if (!result && oldMoveType == MOVETYPE_LADDER)
	{
		player->RegisterTakeoff(false, true, &player->lastValidLadderOrigin);
		player->OnChangeMoveType(MOVETYPE_LADDER);
	}
	else if (result && oldMoveType != MOVETYPE_LADDER && player->GetPlayerPawn()->m_MoveType() == MOVETYPE_LADDER
			 && !(player->GetPlayerPawn()->m_fFlags & FL_ONGROUND))
	{
		player->RegisterLanding(oldVelocity, false);
		player->OnChangeMoveType(MOVETYPE_WALK);
	}
	else if (result && oldMoveType == MOVETYPE_LADDER && player->GetPlayerPawn()->m_MoveType() == MOVETYPE_WALK)
	{
		player->RegisterTakeoff(player->IsButtonPressed(IN_JUMP), true);
		player->possibleLadderHop = true;
		player->OnChangeMoveType(MOVETYPE_LADDER);
	}
	if (result && player->GetPlayerPawn()->m_MoveType() == MOVETYPE_LADDER)
	{
		player->GetOrigin(&player->lastValidLadderOrigin);
	}
	player->OnLadderMovePost();
	return {KHook::Action::Supersede, result};
}

static KHook::Return<void> CheckJumpButtonLegacyPre(CCSPlayerLegacyJump *legacy, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	CCSPlayer_MovementServices *ms = legacy->m_pMovementServices;
	MovementPlayer *player = playerManager->ToPlayer(ms);
#ifdef WATER_FIX
	if (player->enableWaterFix && ms->pawn->m_MoveType() == MOVETYPE_WALK && ms->pawn->m_flWaterLevel() > 0.5f && ms->pawn->m_fFlags & FL_ONGROUND)
	{
		if (ms->m_nButtons().m_pButtonStates[0] & IN_JUMP)
		{
			ms->m_nButtons().m_pButtonStates[1] |= IN_JUMP;
		}
	}
#endif
	player->OnCheckJumpButtonLegacy();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> CheckJumpButtonLegacyPost(CCSPlayerLegacyJump *legacy, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(legacy->m_pMovementServices)->OnCheckJumpButtonLegacyPost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayerLegacyJump, void, CMoveData *> CheckJumpButtonLegacy(CheckJumpButtonLegacyPre, CheckJumpButtonLegacyPost);

static KHook::Return<void> CheckJumpButtonModernPre(CCSPlayerModernJump *modern, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(modern->m_pMovementServices)->OnCheckJumpButtonModern();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> CheckJumpButtonModernPost(CCSPlayerModernJump *modern, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(modern->m_pMovementServices)->OnCheckJumpButtonModernPost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayerModernJump, void, CMoveData *> CheckJumpButtonModern(CheckJumpButtonModernPre, CheckJumpButtonModernPost);

static KHook::Return<void> OnJumpLegacyPre(CCSPlayerLegacyJump *legacy, CMoveData *mv);

static KHook::Member<CCSPlayerLegacyJump, void, CMoveData *> OnJumpLegacy(OnJumpLegacyPre, nullptr);

static KHook::Return<void> OnJumpLegacyPre(CCSPlayerLegacyJump *legacy, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	CCSPlayer_MovementServices *ms = legacy->m_pMovementServices;
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnJumpLegacy();
	Vector oldOutWishVel = mv->m_outWishVel;
	OnJumpLegacy.CallOriginal(legacy, mv);
	if (mv->m_outWishVel != oldOutWishVel)
	{
		player->inPerf = (!player->takeoffFromLadder && !player->oldWalkMoved);
		player->RegisterTakeoff(true, player->possibleLadderHop);
		player->OnStopTouchGround();
	}
	player->OnJumpLegacyPost();
	return {KHook::Action::Supersede};
}

static KHook::Return<void> OnJumpModernPre(CCSPlayerModernJump *modern, CMoveData *mv);

static KHook::Member<CCSPlayerModernJump, void, CMoveData *> OnJumpModern(OnJumpModernPre, nullptr);

static KHook::Return<void> OnJumpModernPre(CCSPlayerModernJump *modern, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	CCSPlayer_MovementServices *ms = modern->m_pMovementServices;
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->OnJumpModern();
	Vector oldOutWishVel = mv->m_outWishVel;
	OnJumpModern.CallOriginal(modern, mv);
	if (mv->m_outWishVel != oldOutWishVel)
	{
		player->inPerf = (!player->takeoffFromLadder && !player->oldWalkMoved);
		player->RegisterTakeoff(true, player->possibleLadderHop);
		player->OnStopTouchGround();
	}
	player->OnJumpModernPost();
	return {KHook::Action::Supersede};
}

static KHook::Return<void> AirMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnAirMove();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> AirMovePost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnAirMovePost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *> AirMove(AirMovePre, AirMovePost);

static KHook::Return<void> AirAcceleratePre(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector &wishdir, f32 wishspeed, f32 accel)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnAirAccelerate(wishdir, wishspeed, accel);
	return {KHook::Action::Ignore};
}

static KHook::Return<void> AirAcceleratePost(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector &wishdir, f32 wishspeed, f32 accel)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnAirAcceleratePost(wishdir, wishspeed, accel);
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *, Vector &, f32, f32> AirAccelerate(AirAcceleratePre, AirAcceleratePost);

static KHook::Return<void> FrictionPre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnFriction();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> FrictionPost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnFrictionPost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *> Friction(FrictionPre, FrictionPost);

static KHook::Return<void> WalkMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnWalkMove();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> WalkMovePost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
	player->walkMoved = true;
	player->OnWalkMovePost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *> WalkMove(WalkMovePre, WalkMovePost);

static KHook::Return<void> TryPlayerMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t *pFirstTrace,
											bool *bIsSurfing);

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *, Vector *, trace_t *, bool *> TryPlayerMove(TryPlayerMovePre, nullptr);

static KHook::Return<void> TryPlayerMovePre(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
#ifdef DEBUG_TPM
	traceHistory.RemoveAll();
	f32 initialError = ms->m_flAccumulatedJumpError();
	Vector initialVelocity = mv->m_vecVelocity;
	g_traceShapeEnabled = true;
	player->OnTryPlayerMove(pFirstDest, pFirstTrace, bIsSurfing);
	Vector oldVelocity = mv->m_vecVelocity;
	i32 count = traceHistory.Count();
	TryPlayerMove.CallOriginal(ms, mv, pFirstDest, pFirstTrace, bIsSurfing);
	if (traceHistory.Count() != count)
	{
		for (i32 i = 0; i + count < traceHistory.Count(); i++)
		{
			if (traceHistory[i].end != traceHistory[i + count].end)
			{
				META_CONPRINTF("Trace not matching! Previous traces (initial error %f, initial velocity %s):\n", initialError,
							   VecToString(initialVelocity));
				for (i32 j = 0; j <= i; j++)
				{
					META_CONPRINTF("Pred %f %f %f -> %f %f %f, error %f, velocity %s ", traceHistory[j].start.x, traceHistory[j].start.y,
								   traceHistory[j].start.z, traceHistory[j].end.x, traceHistory[j].end.y, traceHistory[j].end.z,
								   traceHistory[j].error, VecToString(traceHistory[j].velocity));
					if (traceHistory[j].didHit)
					{
						META_CONPRINTF("hit %s (normal %s, hitpoint %s)\n", VecToString(traceHistory[j].m_vEndPos),
									   VecToString(traceHistory[j].m_vHitNormal), VecToString(traceHistory[j].m_vHitPoint));
					}
					else
					{
						META_CONPRINTF("missed\n");
					}
					META_CONPRINTF("Real %f %f %f -> %f %f %f, error %f, velocity %s ", traceHistory[j + count].start.x,
								   traceHistory[j + count].start.y, traceHistory[j + count].start.z, traceHistory[j + count].end.x,
								   traceHistory[j + count].end.y, traceHistory[j + count].end.z, traceHistory[j + count].error,
								   VecToString(traceHistory[j + count].velocity));
					if (traceHistory[j + count].didHit)
					{
						META_CONPRINTF("hit %s (normal %s, hitpoint %s)\n", VecToString(traceHistory[j + count].m_vEndPos),
									   VecToString(traceHistory[j + count].m_vHitNormal), VecToString(traceHistory[j + count].m_vHitPoint));
					}
					else
					{
						META_CONPRINTF("missed\n");
					}
				}
				break;
			}
		}
	}
	g_traceShapeEnabled = false;
#else
	player->OnTryPlayerMove(pFirstDest, pFirstTrace, bIsSurfing);
	Vector oldVelocity = mv->m_vecVelocity;
	TryPlayerMove.CallOriginal(ms, mv, pFirstDest, pFirstTrace, bIsSurfing);
#endif
	if (mv->m_vecVelocity != oldVelocity)
	{
		player->SetCollidingWithWorld();
	}
	player->OnTryPlayerMovePost(pFirstDest, pFirstTrace, bIsSurfing);
	return {KHook::Action::Supersede};
}

static KHook::Return<void> CategorizePositionPre(CCSPlayer_MovementServices *ms, CMoveData *mv, bool bStayOnGround);

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *, bool> CategorizePosition(CategorizePositionPre, nullptr);

static KHook::Return<void> CategorizePositionPre(CCSPlayer_MovementServices *ms, CMoveData *mv, bool bStayOnGround)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer *player = playerManager->ToPlayer(ms);
#ifdef WATER_FIX
	if (player->enableWaterFix && player->ignoreNextCategorizePosition)
	{
		player->ignoreNextCategorizePosition = false;
		return {KHook::Action::Supersede};
	}
#endif
	player->OnCategorizePosition(bStayOnGround);
	Vector oldVelocity = mv->m_vecVelocity;
	bool oldOnGround = !!(player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND);

	CategorizePosition.CallOriginal(ms, mv, bStayOnGround);

	bool ground = !!(player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND);
	if (oldOnGround != ground)
	{
		if (ground)
		{
			player->RegisterLanding(oldVelocity);
			player->OnStartTouchGround();
		}
		else
		{
			player->RegisterTakeoff(false);
			player->OnStopTouchGround();
		}
	}
	player->OnCategorizePositionPost(bStayOnGround);
	return {KHook::Action::Supersede};
}

static KHook::Return<void> CheckFallingPre(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCheckFalling();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> CheckFallingPost(CCSPlayer_MovementServices *ms, CMoveData *mv)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(ms)->OnCheckFallingPost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayer_MovementServices, void, CMoveData *> CheckFalling(CheckFallingPre, CheckFallingPost);

static KHook::Return<void> PostThinkPre(CCSPlayerPawnBase *pawn)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(pawn)->OnPostThink();
	return {KHook::Action::Ignore};
}

static KHook::Return<void> PostThinkPost(CCSPlayerPawnBase *pawn)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	playerManager->ToPlayer(pawn)->OnPostThinkPost();
	return {KHook::Action::Ignore};
}

static KHook::Member<CCSPlayerPawnBase, void> PostThink(PostThinkPre, PostThinkPost);

// ============================================================
// hooks::Initialize
// ============================================================
void hooks::Initialize()
{
	playerManager = static_cast<MovementPlayerManager *>(g_pPlayerManager);

	// Entity hooks
	startTouchHook.Configure(g_pGameConfig->GetOffset("StartTouch"));
	touchHook.Configure(g_pGameConfig->GetOffset("Touch"));
	endTouchHook.Configure(g_pGameConfig->GetOffset("EndTouch"));
	teleportHook.Configure(g_pGameConfig->GetOffset("Teleport"));
	changeTeamHook.Configure(g_pGameConfig->GetOffset("ControllerChangeTeam"));
	playerRunCommandHook.Configure(g_pGameConfig->GetOffset("PlayerRunCommand"));
	finishMoveHook.Configure(g_pGameConfig->GetOffset("FinishMove"));

	// Interface hooks
	checkTransmitHook.Configure(&ISource2GameEntities::CheckTransmit);
	checkTransmitHook.Add(g_pSource2GameEntities);

	gameFrameHook.Configure(&ISource2Server::GameFrame);
	gameFrameHook.Add(interfaces::pServer);

	gameServerSteamAPIActivatedHook.Configure(&ISource2Server::GameServerSteamAPIActivated);
	gameServerSteamAPIActivatedHook.Add(interfaces::pServer);

	gameServerSteamAPIDeactivatedHook.Configure(&ISource2Server::GameServerSteamAPIDeactivated);
	gameServerSteamAPIDeactivatedHook.Add(interfaces::pServer);

	clientConnectHook.Configure(&ISource2GameClients::ClientConnect);
	clientConnectHook.Add(g_pSource2GameClients);

	onClientConnectedHook.Configure(&ISource2GameClients::OnClientConnected);
	onClientConnectedHook.Add(g_pSource2GameClients);

	clientFullyConnectHook.Configure(&ISource2GameClients::ClientFullyConnect);
	clientFullyConnectHook.Add(g_pSource2GameClients);

	clientPutInServerHook.Configure(&ISource2GameClients::ClientPutInServer);
	clientPutInServerHook.Add(g_pSource2GameClients);

	clientActiveHook.Configure(&ISource2GameClients::ClientActive);
	clientActiveHook.Add(g_pSource2GameClients);

	clientDisconnectHook.Configure(&ISource2GameClients::ClientDisconnect);
	clientDisconnectHook.Add(g_pSource2GameClients);

	clientVoiceHook.Configure(&ISource2GameClients::ClientVoice);
	clientVoiceHook.Add(g_pSource2GameClients);

	clientCommandHook.Configure(&ISource2GameClients::ClientCommand);
	clientCommandHook.Add(g_pSource2GameClients);

	startupServerHook.Configure(&INetworkServerService::StartupServer);
	startupServerHook.Add(g_pNetworkServerService);

	fireEventHook.Configure(&IGameEventManager2::FireEvent);
	fireEventHook.Add(interfaces::pGameEventManager);

	dispatchConCommandHook.Configure(&ICvar::DispatchConCommand);
	dispatchConCommandHook.Add(g_pCVar);

	postEventHook.Configure(&IGameEventSystem::PostEventAbstract);
	postEventHook.Add(interfaces::pGameEventSystem);

	// Hooks by searching virtual tables
	{
		void *vtable = modules::engine->FindVirtualTable("CNetworkGameServer");
		activateServerHook.Configure(&CNetworkGameServerBase::ActivateServer);
		activateServerHook.AddGlobal((CNetworkGameServerBase *)&vtable);

		connectClientHook.Configure(&CNetworkGameServerBase::ConnectClient);
		connectClientHook.AddGlobal((CNetworkGameServerBase *)&vtable);
	}

	{
		void *vtable = modules::server->FindVirtualTable("CEntityDebugGameSystem");
		serverGamePostSimulateHook.Configure(&IGameSystem::OnServerGamePostSimulate);
		serverGamePostSimulateHook.AddGlobal((IGameSystem *)&vtable);

		buildGameSessionManifestHook.Configure(&IGameSystem::OnBuildGameSessionManifest);
		buildGameSessionManifestHook.AddGlobal((IGameSystem *)&vtable);
	}

	{
		void *vtable = modules::server->FindVirtualTable("CGameEntitySystem");
		entitySystemSpawnHook.Configure(&CEntitySystem::Spawn);
		entitySystemSpawnHook.AddGlobal((CEntitySystem *)&vtable);
	}

	{
		void *vtable = modules::server->FindVirtualTable("CSpawnGroupMgrGameSystem");
		createLoadingSpawnGroupHook.Configure(&CSpawnGroupMgrGameSystem::CreateLoadingSpawnGroup);
		createLoadingSpawnGroupHook.AddGlobal((CSpawnGroupMgrGameSystem *)&vtable);
	}

	{
		void *vtable = modules::server->FindVirtualTable("CCSPlayer_MovementServices");
		playerRunCommandHook.AddGlobal((CCSPlayer_MovementServices *)&vtable);
		finishMoveHook.AddGlobal((CCSPlayer_MovementServices *)&vtable);
	}

	// Signature-based hooks
	RecvServerBrowserPacket.Configure((int (*)(RecvPktInfo_t &, void *))g_pGameConfig->ResolveSignature("RecvServerBrowserPacket"));
	CPhysicsGameSystemFrameBoundary.Configure((void (*)(void *))g_pGameConfig->ResolveSignature("CPhysicsGameSystemFrameBoundary"));
#ifdef DEBUG_TPM
	TraceShape.Configure((bool (*)(const void *, const Ray_t &, const Vector &, const Vector &, const CTraceFilter *,
								   trace_t *))g_pGameConfig->ResolveSignature("TraceShape"));
#endif

	PhysicsSimulate.Configure(g_pGameConfig->ResolveSignature("PhysicsSimulate"));
	ProcessUsercmds.Configure(g_pGameConfig->ResolveSignature("ProcessUsercmds"));
	SetupMove.Configure(g_pGameConfig->ResolveSignature("SetupMove"));
	ProcessMovement.Configure(g_pGameConfig->ResolveSignature("ProcessMovement"));
	PlayerMove.Configure(g_pGameConfig->ResolveSignature("PlayerMove"));
	CheckParameters.Configure(g_pGameConfig->ResolveSignature("CheckParameters"));
	CanMove.Configure(g_pGameConfig->ResolveSignature("CanMove"));
	FullWalkMove.Configure(g_pGameConfig->ResolveSignature("FullWalkMove"));
	MoveInit.Configure(g_pGameConfig->ResolveSignature("MoveInit"));
	CheckWater.Configure(g_pGameConfig->ResolveSignature("CheckWater"));
	WaterMove.Configure(g_pGameConfig->ResolveSignature("WaterMove"));
	CheckVelocity.Configure(g_pGameConfig->ResolveSignature("CheckVelocity"));
	Duck.Configure(g_pGameConfig->ResolveSignature("Duck"));
	CanUnduck.Configure(g_pGameConfig->ResolveSignature("CanUnduck"));
	LadderMove.Configure(g_pGameConfig->ResolveSignature("LadderMove"));
	CheckJumpButtonLegacy.Configure(g_pGameConfig->ResolveSignature("CheckJumpButtonLegacy"));
	CheckJumpButtonModern.Configure(g_pGameConfig->ResolveSignature("CheckJumpButtonModern"));
	OnJumpLegacy.Configure(g_pGameConfig->ResolveSignature("OnJumpLegacy"));
	OnJumpModern.Configure(g_pGameConfig->ResolveSignature("OnJumpModern"));
	AirMove.Configure(g_pGameConfig->ResolveSignature("AirMove"));
	AirAccelerate.Configure(g_pGameConfig->ResolveSignature("AirAccelerate"));
	Friction.Configure(g_pGameConfig->ResolveSignature("Friction"));
	WalkMove.Configure(g_pGameConfig->ResolveSignature("WalkMove"));
	TryPlayerMove.Configure(g_pGameConfig->ResolveSignature("TryPlayerMove"));
	CategorizePosition.Configure(g_pGameConfig->ResolveSignature("CategorizePosition"));
	CheckFalling.Configure(g_pGameConfig->ResolveSignature("CheckFalling"));
	PostThink.Configure(g_pGameConfig->ResolveSignature("PostThink"));
}

void hooks::Cleanup()
{
	startTouchHook.ClearHooks();
	touchHook.ClearHooks();
	endTouchHook.ClearHooks();
	teleportHook.ClearHooks();
	changeTeamHook.ClearHooks();

	checkTransmitHook.ClearHooks();
	gameFrameHook.ClearHooks();
	gameServerSteamAPIActivatedHook.ClearHooks();
	gameServerSteamAPIDeactivatedHook.ClearHooks();
	clientConnectHook.ClearHooks();
	onClientConnectedHook.ClearHooks();
	clientFullyConnectHook.ClearHooks();
	clientPutInServerHook.ClearHooks();
	clientActiveHook.ClearHooks();
	clientDisconnectHook.ClearHooks();
	clientVoiceHook.ClearHooks();
	clientCommandHook.ClearHooks();
	startupServerHook.ClearHooks();
	fireEventHook.ClearHooks();
	dispatchConCommandHook.ClearHooks();
	postEventHook.ClearHooks();
	activateServerHook.ClearHooks();
	connectClientHook.ClearHooks();
	serverGamePostSimulateHook.ClearHooks();
	buildGameSessionManifestHook.ClearHooks();
	entitySystemSpawnHook.ClearHooks();
	createLoadingSpawnGroupHook.ClearHooks();
	playerRunCommandHook.ClearHooks();
	finishMoveHook.ClearHooks();

	if (GameEntitySystem())
	{
		GameEntitySystem()->RemoveListenerEntity(&entityListener);
	}
}

// ============================================================
// Entity hook management
// ============================================================
void hooks::AddEntityHooks(CBaseEntity *entity)
{
	if (!V_stricmp(entity->GetClassname(), "cs_player_controller"))
	{
		changeTeamHook.Add(static_cast<CCSPlayerController *>(entity));
	}
	else if (KZTriggerService::IsValidTrigger(entity) || !V_stricmp(entity->GetClassname(), "player"))
	{
		startTouchHook.Add(entity);
		touchHook.Add(entity);
		endTouchHook.Add(entity);
		CCSPlayerPawn *pawn = static_cast<CCSPlayerPawn *>(entity);
		if (!V_stricmp(entity->GetClassname(), "player") && g_pKZPlayerManager->ToPlayer(pawn))
		{
			teleportHook.Add(entity);
		}
	}
}

void hooks::RemoveEntityHooks(CBaseEntity *entity)
{
	if (KZTriggerService::IsValidTrigger(entity) || !V_stricmp(entity->GetClassname(), "player"))
	{
		startTouchHook.Remove(entity);
		touchHook.Remove(entity);
		endTouchHook.Remove(entity);
		if (!KZTriggerService::IsValidTrigger(entity))
		{
			teleportHook.Remove(entity);
		}
	}
}

void EntListener::OnEntityCreated(CEntityInstance *pEntity) {}

void EntListener::OnEntitySpawned(CEntityInstance *pEntity)
{
	if (KZTriggerService::IsValidTrigger(static_cast<CBaseEntity *>(pEntity)))
	{
		CBaseTrigger *trigger = static_cast<CBaseTrigger *>(pEntity);
		trigger->m_fEffects() &= ~EF_NODRAW;
		hooks::AddEntityHooks(static_cast<CBaseEntity *>(pEntity));
		KZ::mapapi::CheckEndTimerTrigger((CBaseTrigger *)pEntity);
	}
}

void EntListener::OnEntityDeleted(CEntityInstance *pEntity)
{
	if (KZTriggerService::IsValidTrigger(static_cast<CBaseEntity *>(pEntity)))
	{
		hooks::RemoveEntityHooks(static_cast<CBaseEntity *>(pEntity));
	}
}

void hooks::HookEntities()
{
	startTouchHook.ClearHooks();
	touchHook.ClearHooks();
	endTouchHook.ClearHooks();
	teleportHook.ClearHooks();

	GameEntitySystem()->RemoveListenerEntity(&entityListener);
	for (CEntityIdentity *entID = GameEntitySystem()->m_EntityList.m_pFirstActiveEntity; entID != NULL; entID = entID->m_pNext)
	{
		hooks::AddEntityHooks(static_cast<CBaseEntity *>(entID->m_pInstance));
	}
	GameEntitySystem()->AddListenerEntity(&entityListener);
}
