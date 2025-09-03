#pragma once

#include "common.h"
#include "sdk/datatypes.h"
#include "playerslot.h"
#include "vector.h"
#include "igameeventsystem.h"

class CGameConfig;
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
class BotProfile;

// temporary botprofile classes
#include "utllinkedlist.h"
#include "utlvector.h"

class BotProfile
{
public:
	const char *botName = "";
	float aggression = 0.0f;
	float skill = 0.0f;
	float teamwork = 0.0f;
	float aimFocusInitial = 0.0f;
	float aimFocusDecay = 0.0f;
	float aimFocusOffsetScale = 0.0f;
	float aimFocusInterval = 0.0f;
	u16 weaponPreferences[16] = {0xFFFF};
	int weaponPreferencesCount = 0;
	int cost = 0;
	int skin = 0;
	u8 difficultyFlags = 0;
	int voicePitch = 0;
	float reactionTime = 0.0f;
	float attackDelay = 0.0f;
	int teamNum = CS_TEAM_T;
	bool preferSilencer = false;
	int voiceBank = 0;
	float lookAngleMaxAccelNormal = 0.0f;
	float lookAngleStiffnessNormal = 0.0f;
	float lookAngleDampingNormal = 0.0f;
	float lookAngleMaxAccelAttacking = 0.0f;
	float lookAngleStiffnessAttacking = 0.0f;
	float lookAngleDampingAttacking = 0.0f;
	CUtlVector<BotProfile *> profileTemplates;

	static inline BotProfile *PersistentProfile(int teamNum)
	{
		static BotProfile profile = BotProfile();
		teamNum == CS_TEAM_T ? profile.teamNum = CS_TEAM_T : profile.teamNum = CS_TEAM_CT;
		return &profile;
	}
};

struct SndOpEventGuid_t;
struct EmitSound_t;

typedef IGameEventListener2 *GetLegacyGameEventListener_t(CPlayerSlot slot);
typedef void SnapViewAngles_t(CBasePlayerPawn *pawn, const QAngle &angle);
typedef CBaseEntity *FindEntityByClassname_t(CEntitySystem *, CEntityInstance *, const char *);
typedef SndOpEventGuid_t EmitSoundFunc_t(IRecipientFilter &filter, CEntityIndex ent, const EmitSound_t &params);
typedef void TracePlayerBBox_t(const Vector &start, const Vector &end, const bbox_t &bounds, CTraceFilter *filter, trace_t &pm);
typedef void SwitchTeam_t(CCSPlayerController *controller, int team);
typedef void SetPawn_t(CBasePlayerController *controller, CCSPlayerPawn *pawn, bool, bool, bool, bool);
typedef CBaseEntity *CreateEntityByName_t(const char *className, int iForceEdictIndex);
typedef void DispatchSpawn_t(CBaseEntity *pEntity, CEntityKeyValues *pEntityKeyValues);
typedef void RemoveEntity_t(CEntityInstance *);
typedef void DebugDrawMesh_t(CTransform &transform, Ray_t &ray, i32 r, i32 g, i32 b, i32 a, bool solid, bool ignoreZ, f32 duration);
typedef CCSPlayerController *CreateBot_t(BotProfile *botProfile, i32 teamNumber);

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
	KZUtils(TracePlayerBBox_t *TracePlayerBBox, GetLegacyGameEventListener_t *GetLegacyGameEventListener, SnapViewAngles_t *SnapViewAngles,
			EmitSoundFunc_t *EmitSound, SwitchTeam_t *SwitchTeam, SetPawn_t *SetPawn, CreateEntityByName_t *CreateEntityByName,
			DispatchSpawn_t *DispatchSpawn, RemoveEntity_t *RemoveEntity, DebugDrawMesh_t *DebugDrawMesh, CreateBot_t *CreateBot)
		: TracePlayerBBox(TracePlayerBBox), GetLegacyGameEventListener(GetLegacyGameEventListener), SnapViewAngles(SnapViewAngles),
		  EmitSound(EmitSound), SwitchTeam(SwitchTeam), SetPawn(SetPawn), CreateEntityByName(CreateEntityByName), DispatchSpawn(DispatchSpawn),
		  RemoveEntity(RemoveEntity), DebugDrawMesh(DebugDrawMesh), CreateBot(CreateBot)
	{
	}

	TracePlayerBBox_t *const TracePlayerBBox;
	GetLegacyGameEventListener_t *const GetLegacyGameEventListener;
	SnapViewAngles_t *const SnapViewAngles;
	EmitSoundFunc_t *const EmitSound;
	SwitchTeam_t *const SwitchTeam;
	SetPawn_t *const SetPawn;
	CreateEntityByName_t *const CreateEntityByName;
	DispatchSpawn_t *const DispatchSpawn;
	RemoveEntity_t *const RemoveEntity;
	DebugDrawMesh_t *const DebugDrawMesh;
	CreateBot_t *const CreateBot;

	virtual CGameConfig *GetGameConfig();
	virtual const CGlobalVars *GetServerGlobals();
	virtual CGlobalVars *GetGlobals();

	virtual CBaseEntity *FindEntityByClassname(CEntityInstance *start, const char *name);

	virtual CBasePlayerController *GetController(CBaseEntity *entity);
	virtual CBasePlayerController *GetController(CPlayerSlot slot);

	virtual CPlayerSlot GetEntityPlayerSlot(CBaseEntity *entity);

	// Convar stuff.
	virtual void SendConVarValue(CPlayerSlot slot, ConVarRefAbstract conVar, const char *value);
	virtual void SendMultipleConVarValues(CPlayerSlot slot, ConVarRefAbstract **conVars, const char **values, u32 size);

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

	virtual CUtlString GetCurrentMapName(bool *success = NULL);
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

	// Get the real and connected player count.
	virtual u32 GetPlayerCount();

	// Draw debug overlays. Listen server only.
	virtual void AddTriangleOverlay(Vector const &p1, Vector const &p2, Vector const &p3, u8 r, u8 g, u8 b, u8 a, bool noDepthTest, f64 flDuration);
	virtual void ClearOverlays();

	BotProfile *GetBotProfile(u8 difficulty = 2, i32 teamNumber = CS_TEAM_CT, i32 weaponClass = 2);
};

extern KZUtils *g_pKZUtils;
