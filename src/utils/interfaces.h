#pragma once

#include "common.h"
#include "sdk/datatypes.h"
#include "playerslot.h"
#include "vector.h"
#include "igameeventsystem.h"

class CGameConfig;
class CTraceFilterPlayerMovementCS;
class CTraceFilter;
class CCSPlayerController;
class CGameResourceService;
class CSchemaSystem;
class IVEngineServer2;
class ISource2Server;
class IGameEventManager2;
class IGameEventSystem;
class CBasePlayerPawn;
class CCSPlayerPawn;
class CBaseEntity;
class CBasePlayerController;
class IGameEventListener2;
class CTimerBase;
class CServerSideClient;
class CCSGameRules;

struct SndOpEventGuid_t;
struct EmitSound_t;

typedef void InitPlayerMovementTraceFilter_t(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactWith,
											 int collisionGroup);
typedef void InitGameTrace_t(trace_t *trace);
typedef IGameEventListener2 *GetLegacyGameEventListener_t(CPlayerSlot slot);
typedef void SnapViewAngles_t(CBasePlayerPawn *pawn, const QAngle &angle);
typedef CBaseEntity *FindEntityByClassname_t(CEntitySystem *, CEntityInstance *, const char *);
typedef SndOpEventGuid_t EmitSoundFunc_t(IRecipientFilter &filter, CEntityIndex ent, const EmitSound_t &params);
typedef void TracePlayerBBox_t(const Vector &start, const Vector &end, const bbox_t &bounds, CTraceFilter *filter, trace_t &pm);
typedef void SwitchTeam_t(CCSPlayerController *controller, int team);
typedef void SetPawn_t(CBasePlayerController *controller, CCSPlayerPawn *pawn, bool, bool, bool);

namespace interfaces
{
	inline CGameResourceService *pGameResourceServiceServer = nullptr;
	inline IVEngineServer2 *pEngine = nullptr;
	inline ISource2Server *pServer = nullptr;
	inline IGameEventManager2 *pGameEventManager = nullptr;
	inline IGameEventSystem *pGameEventSystem = nullptr;

	inline bool Initialize(ISmmAPI *ismm, char *error, size_t maxlen)
	{
		GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
		GET_V_IFACE_CURRENT(GetEngineFactory, interfaces::pGameResourceServiceServer, CGameResourceService,
							GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
		GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2GameClients, ISource2GameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
		GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
		GET_V_IFACE_CURRENT(GetEngineFactory, interfaces::pEngine, IVEngineServer2, INTERFACEVERSION_VENGINESERVER);
		GET_V_IFACE_CURRENT(GetServerFactory, interfaces::pServer, ISource2Server, INTERFACEVERSION_SERVERGAMEDLL);
		GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
		GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
		GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
		GET_V_IFACE_CURRENT(GetEngineFactory, interfaces::pGameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
		GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

		return true;
	}
} // namespace interfaces

#define KZ_UTILS_INTERFACE "KZUtilsInterface"

// Expose some of the utility functions to other plugins.
class KZUtils
{
public:
	KZUtils(TracePlayerBBox_t *TracePlayerBBox, InitGameTrace_t *InitGameTrace, InitPlayerMovementTraceFilter_t *InitPlayerMovementTraceFilter,
			GetLegacyGameEventListener_t *GetLegacyGameEventListener, SnapViewAngles_t *SnapViewAngles, EmitSoundFunc_t *EmitSound,
			SwitchTeam_t *SwitchTeam, SetPawn_t *SetPawn)
		: TracePlayerBBox(TracePlayerBBox), InitGameTrace(InitGameTrace), InitPlayerMovementTraceFilter(InitPlayerMovementTraceFilter),
		  GetLegacyGameEventListener(GetLegacyGameEventListener), SnapViewAngles(SnapViewAngles), EmitSound(EmitSound), SwitchTeam(SwitchTeam),
		  SetPawn(SetPawn)
	{
	}

	TracePlayerBBox_t *const TracePlayerBBox;
	InitGameTrace_t *const InitGameTrace;
	InitPlayerMovementTraceFilter_t *const InitPlayerMovementTraceFilter;
	GetLegacyGameEventListener_t *const GetLegacyGameEventListener;
	SnapViewAngles_t *const SnapViewAngles;
	EmitSoundFunc_t *const EmitSound;
	SwitchTeam_t *const SwitchTeam;
	SetPawn_t *const SetPawn;

	virtual CGameConfig *GetGameConfig();
	virtual const CGlobalVars *GetServerGlobals();
	virtual CGlobalVars *GetGlobals();

	virtual CBaseEntity *FindEntityByClassname(CEntityInstance *start, const char *name);

	virtual CBasePlayerController *GetController(CBaseEntity *entity);
	virtual CBasePlayerController *GetController(CPlayerSlot slot);

	virtual CPlayerSlot GetEntityPlayerSlot(CBaseEntity *entity);

	// Convar stuff.
	virtual void SendConVarValue(CPlayerSlot slot, ConVar *conVar, const char *value);
	virtual void SendMultipleConVarValues(CPlayerSlot slot, ConVar **cvars, const char **values, u32 size);

	// Normalize the angle between -180 and 180.
	virtual f32 NormalizeDeg(f32 a);
	// Gets the difference in angle between 2 angles.
	// c can be PI (for radians) or 180.0 (for degrees);
	virtual f32 GetAngleDifference(const f32 source, const f32 target, const f32 c, bool relative = false);
	virtual CGameEntitySystem *GetGameEntitySystem();

	virtual void AddTimer(CTimerBase *timer, bool preserveMapChange = true);
	virtual void RemoveTimer(CTimerBase *timer);
	virtual CUtlVector<CServerSideClient *> *GetClientList();

	CServerSideClient *GetClientBySlot(CPlayerSlot slot)
	{
		return (GetClientList() && GetController(slot)) ? GetClientList()->Element(slot.Get()) : nullptr;
	}

	virtual u64 GetCurrentMapWorkshopID();
	virtual CUtlString GetCurrentMapVPK();
	virtual CUtlString GetCurrentMapDirectory();
	virtual u64 GetCurrentMapSize();
	// MD5 calculation is not very fast, avoid doing this too often!
	virtual bool UpdateCurrentMapMD5();
	virtual bool GetCurrentMapMD5(char *buffer, i32 size);
	// Must be absolute path.
	virtual bool GetFileMD5(const char *filePath, char *buffer, i32 size);

	// Getting the entity could be expensive, do not spam this function!
	virtual CCSGameRules *GetGameRules();
};

extern KZUtils *g_pKZUtils;
