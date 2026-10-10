
#include "utils.h"
#include "cs2kz.h"
#include "player/player.h"
#include "sdk/entity/cbasetrigger.h"
#include "sdk/recipientfilters.h"
#include "sdk/navphysicsinterface.h"
#include "addresses.h"
#include "logging.h"
#include "gameconfig.h"

#include "module.h"
#include "virtual.h"

#include "networkbasetypes.pb.h"

#include "convar.h"
#include "igameeventsystem.h"
#include "gametrace.h"
#include "bspflags.h"
#include "filesystem.h"
#include "interfaces/interfaces.h"
#include "networksystem/inetworkmessages.h"
#include "steam/steam_gameserver.h"

#include "tier0/dbg.h"
#include "tier0/memdbgon.h"

#define FCVAR_FLAGS_TO_REMOVE (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_DEFENSIVE)

#define RESOLVE_SIG(gameConfig, name, type, variable, result) \
	type *variable = (decltype(variable))gameConfig->ResolveSignature(name); \
	if (!variable) \
	{ \
		KZ_LOG_WARN(LogChannel::General, "Failed to find address for %s!\n", #name); \
		result = false; \
	}

CGameConfig *g_pGameConfig = NULL;
KZUtils *g_pKZUtils = NULL;
extern CSteamGameServerAPIContext g_steamAPI;

#define SERVER_VERSION_KEY "ServerVersion="
static_global u32 serverVersion;

bool utils::Initialize(ISmmAPI *ismm, char *error, size_t maxlen)
{
	CBufferStringGrowable<256> gamedirpath;
	interfaces::pEngine->GetGameDir(gamedirpath);

	std::string gamedirname = CGameConfig::GetDirectoryName(gamedirpath.Get());
	const char *gamedataPath = "addons/cs2kz/gamedata/cs2kz-core.games.txt";

	g_pGameConfig = new CGameConfig(gamedirname, gamedataPath);
	char conf_error[255] = "";
	if (!g_pGameConfig->Init(g_pFullFileSystem, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlen, "Could not read %s: %s", g_pGameConfig->GetPath().c_str(), conf_error);
		KZ_LOG_WARN(LogChannel::General, "%s\n", error);
		return false;
	}

	// Convoluted way of having GameEventManager regardless of lateloading
	if (!(interfaces::pGameEventManager = (IGameEventManager2 *)g_pGameConfig->ResolveSignatureFromMov("GameEventManager")))
	{
		snprintf(error, maxlen, "Failed to resolve signature: GameEventManager");
		KZ_LOG_WARN(LogChannel::General, "%s\n", error);
		return false;
	}

	bool sigResolved = true;
	RESOLVE_SIG(g_pGameConfig, "GetLegacyGameEventListener", GetLegacyGameEventListener_t, GetLegacyGameEventListener, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "SnapViewAngles", SnapViewAngles_t, SnapViewAngles, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "EmitSound", EmitSoundFunc_t, EmitSound, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "CCSPlayerController_SwitchTeam", SwitchTeam_t, SwitchTeam, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "CBasePlayerController_SetPawn", SetPawn_t, SetPawn, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "CreateEntityByName", CreateEntityByName_t, CreateEntityByName, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "DispatchSpawn", DispatchSpawn_t, DispatchSpawn, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "RemoveEntity", RemoveEntity_t, RemoveEntity, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "DebugDrawMesh", DebugDrawMesh_t, DebugDrawMesh, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "CreateBot", CreateBot_t, CreateBot, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "SetOrAddAttributeValueByName", SetOrAddAttributeValueByName_t, SetOrAddAttributeValueByName, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "SetModel", SetModel_t, SetModel, sigResolved);
	RESOLVE_SIG(g_pGameConfig, "DecalTrace", DecalTrace_t, DecalTrace, sigResolved);

	if (!sigResolved)
	{
		snprintf(error, maxlen, "Failed to resolve one or more signatures.");
		KZ_LOG_WARN(LogChannel::General, "%s\n", error);
		return false;
	}
	g_pKZUtils = new KZUtils(GetLegacyGameEventListener, SnapViewAngles, EmitSound, SwitchTeam, SetPawn, CreateEntityByName, DispatchSpawn,
							 RemoveEntity, DebugDrawMesh, CreateBot, SetOrAddAttributeValueByName, SetModel, DecalTrace);

	utils::UnlockConVars();
	utils::UnlockConCommands();
	utils::UpdateServerVersion();
	return true;
}

void utils::Cleanup()
{
	delete g_pGameConfig;
	g_pGameConfig = NULL;
	delete g_pKZUtils;
	g_pKZUtils = NULL;
	modules::Cleanup();
}

CBaseEntity *utils::FindEntityByClassname(CEntityInstance *start, const char *name)
{
	if (!GameEntitySystem())
	{
		return NULL;
	}
	EntityInstanceByClassIter_t iter(start, name);

	return static_cast<CBaseEntity *>(iter.Next());
}

void utils::UnlockConVars()
{
	if (!g_pCVar)
	{
		return;
	}

	ConVarRefAbstract ref(ConVarRef((u16)0));

	// Can't use FOR_EACH_CONVAR here (?) as it would skip cvars with certain flags, so just loop through the handles
	while (ref.IsValidRef())
	{
		if (ref.IsConVarDataAvailable())
		{
			ref.RemoveFlags(FCVAR_FLAGS_TO_REMOVE);
		}
		ref = {ConVarRef(ref.GetAccessIndex() + 1)};
	}
}

void utils::UnlockConCommands()
{
	if (!g_pCVar)
	{
		return;
	}

	ConCommandRef conCommand = {(u16)0};
	ConCommandData *conCommandData = conCommand.GetRawData();
	ConCommandData *invalidData = g_pCVar->GetConCommandData(ConCommandRef());
	while (conCommandData != invalidData)
	{
		if (conCommand.IsFlagSet(FCVAR_FLAGS_TO_REMOVE))
		{
			conCommand.RemoveFlags(FCVAR_FLAGS_TO_REMOVE);
		}
		conCommand = {(uint16)(conCommand.GetAccessIndex() + 1)};
		conCommandData = conCommand.GetRawData();
	}
}

CBasePlayerController *utils::GetController(CBaseEntity *entity)
{
	CCSPlayerController *controller = nullptr;
	if (!V_stricmp(entity->GetClassname(), "observer"))
	{
		CBasePlayerPawn *pawn = static_cast<CBasePlayerPawn *>(entity);
		if (!pawn->m_hController().IsValid() || pawn->m_hController.Get() == 0)
		{
			for (i32 i = 0; i <= MAXPLAYERS; i++)
			{
				controller = (CCSPlayerController *)utils::GetController(CPlayerSlot(i));
				if (controller && controller->m_hObserverPawn() && controller->m_hObserverPawn().Get() == entity)
				{
					return controller;
				}
			}
			return nullptr;
		}
		return pawn->m_hController.Get();
	}
	if (entity->IsPawn())
	{
		CBasePlayerPawn *pawn = static_cast<CBasePlayerPawn *>(entity);
		if (!pawn->m_hController().IsValid() || pawn->m_hController.Get() == 0)
		{
			// Seems like the pawn lost its controller, we can try looping through the controllers to find this pawn instead.
			for (i32 i = 0; i <= MAXPLAYERS; i++)
			{
				controller = (CCSPlayerController *)utils::GetController(CPlayerSlot(i));
				if (controller && controller->m_hPlayerPawn() && controller->m_hPlayerPawn().Get() == entity)
				{
					return controller;
				}
			}
			return nullptr;
		}
		return pawn->m_hController.Get();
	}
	else if (entity->IsController())
	{
		return static_cast<CBasePlayerController *>(entity);
	}
	else
	{
		return nullptr;
	}
}

CBasePlayerController *utils::GetController(CPlayerSlot slot)
{
	if (!GameEntitySystem() || slot.Get() < 0 || slot.Get() > MAXPLAYERS)
	{
		return nullptr;
	}
	CBaseEntity *ent = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(CEntityIndex(slot.Get() + 1)));
	if (!ent)
	{
		return nullptr;
	}
	return ent->IsController() ? static_cast<CBasePlayerController *>(ent) : nullptr;
}

CPlayerSlot utils::GetEntityPlayerSlot(CBaseEntity *entity)
{
	CBasePlayerController *controller = utils::GetController(entity);
	if (!controller)
	{
		return -1;
	}
	else
	{
		return controller->m_pEntity->m_EHandle.GetEntryIndex() - 1;
	}
}

void utils::PlaySoundToClient(CPlayerSlot player, const char *sound, f32 volume)
{
	if (strncmp(sound, "kz.", strlen("kz.")) == 0 && !g_KZPlugin.IsAddonMounted())
	{
		return;
	}

	if (g_KZPlugin.unloading)
	{
		return;
	}

	CSingleRecipientFilter filter(player.Get());
	EmitSound_t soundParams;
	soundParams.m_pSoundName = sound;
	soundParams.m_flVolume = volume;
	g_pKZUtils->EmitSound(filter, player.Get() + 1, soundParams);
}

f32 utils::NormalizeDeg(f32 a)
{
	a = fmod(a, 360.0);
	if (a >= 180.0)
	{
		a -= 360.0;
	}
	else if (a < -180.0)
	{
		a += 360.0;
	}
	return a;
}

f32 utils::GetAngleDifference(const f32 source, const f32 target, const f32 c, bool relative)
{
	if (relative)
	{
		return fmod((fmod(target - source, 2 * c) + 3 * c), 2 * c) - c;
	}
	return fmod(fabs(target - source) + c, 2 * c) - c;
}

bool utils::SetConVarValue(CPlayerSlot slot, const char *name, const char *value, bool replicate, bool triggerCallback)
{
	if (!name || !value)
	{
		assert(0);
		return false;
	}

	ConVarRefAbstract cvarRef(name);
	if (!cvarRef.IsValidRef() || !cvarRef.IsConVarDataAvailable())
	{
		assert(0);
		KZ_LOG_WARN(LogChannel::General, "Failed to find %s!\n", name);
		return false;
	}

	if (triggerCallback)
	{
		cvarRef.SetString(value);
	}
	else
	{
		CVValue_t newValue;
		cvarRef.TypeTraits()->Construct(&newValue);
		cvarRef.TypeTraits()->StringToValue(value, &newValue);
		cvarRef.TypeTraits()->Copy(cvarRef.GetConVarData()->Value(-1), newValue);
		cvarRef.TypeTraits()->Destruct(&newValue);
	}

	if (replicate)
	{
		SendConVarValue(slot, cvarRef, value);
	}

	return true;
}

bool utils::SetConVarValue(CPlayerSlot slot, ConVarRefAbstract conVarRef, const char *value, bool replicate, bool triggerCallback)
{
	if (!conVarRef.IsValidRef() || !conVarRef.IsConVarDataAvailable())
	{
		assert(0);
		// If the ref is not valid, then the name is not available here, so this will silently fail.
		return false;
	}

	if (triggerCallback)
	{
		conVarRef.SetString(value);
	}
	else
	{
		CVValue_t newValue;
		conVarRef.TypeTraits()->Construct(&newValue);
		conVarRef.TypeTraits()->StringToValue(value, &newValue);
		conVarRef.TypeTraits()->Copy(conVarRef.GetConVarData()->Value(-1), newValue);
		conVarRef.TypeTraits()->Destruct(&newValue);
	}

	if (replicate)
	{
		SendConVarValue(slot, conVarRef, value);
	}

	return true;
}

bool utils::SetConVarValue(CPlayerSlot slot, ConVarRefAbstract conVarRef, const CVValue_t *value, bool replicate, bool triggerCallback)
{
	if (!conVarRef.IsValidRef() || !conVarRef.IsConVarDataAvailable())
	{
		assert(0);
		// If the ref is not valid, then the name is not available here, so this will silently fail.
		return false;
	}

	CBufferString buf;
	conVarRef.TypeTraits()->ValueToString(value, buf);
	if (triggerCallback)
	{
		conVarRef.SetString(buf);
	}
	else
	{
		conVarRef.TypeTraits()->Copy(conVarRef.GetConVarData()->Value(-1), *value);
	}

	if (replicate)
	{
		SendConVarValue(slot, conVarRef, buf.Get());
	}

	return true;
}

void utils::SendConVarValue(CPlayerSlot slot, const char *conVar, const char *value)
{
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	auto msg = netmsg->AllocateMessage()->ToPB<CNETMsg_SetConVar>();
	CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
	cvar->set_name(conVar);
	cvar->set_value(value);
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;
}

void utils::SendMultipleConVarValues(CPlayerSlot slot, const char **cvars, const char **values, u32 size)
{
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	auto msg = netmsg->AllocateMessage()->ToPB<CNETMsg_SetConVar>();
	for (u32 i = 0; i < size; i++)
	{
		CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
		cvar->set_name(cvars[i]);
		cvar->set_value(values[i]);
	}
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;
}

void utils::SendConVarValue(CPlayerSlot slot, ConVarRefAbstract conVarRef, const char *value)
{
	if (!conVarRef.IsValidRef() || !conVarRef.IsConVarDataAvailable())
	{
		return;
	}
	utils::SendConVarValue(slot, conVarRef.GetName(), value);
}

void utils::SendConVarValue(CPlayerSlot slot, ConVarRefAbstract conVarRef, const CVValue_t *value)
{
	if (!conVarRef.IsValidRef() || !conVarRef.IsConVarDataAvailable())
	{
		return;
	}
	CBufferString valueStr;
	conVarRef.GetConVarData()->TypeTraits()->ValueToString(value, valueStr);
	utils::SendConVarValue(slot, conVarRef.GetName(), valueStr.Get());
}

void utils::SendMultipleConVarValues(CPlayerSlot slot, ConVarRefAbstract **conVarRefs, const char **values, u32 size)
{
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	auto msg = netmsg->AllocateMessage()->ToPB<CNETMsg_SetConVar>();
	for (u32 i = 0; i < size; i++)
	{
		if (!conVarRefs[i]->IsValidRef())
		{
			delete msg;
			return;
		}
		CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
		cvar->set_name(conVarRefs[i]->GetName());
		cvar->set_value(values[i]);
	}
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;
}

void utils::SendMultipleConVarValues(CPlayerSlot slot, ConVarRefAbstract **conVarRefs, const CVValue_t *values, u32 size)
{
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	auto msg = netmsg->AllocateMessage()->ToPB<CNETMsg_SetConVar>();
	for (u32 i = 0; i < size; i++)
	{
		if (!conVarRefs[i]->IsValidRef())
		{
			delete msg;
			return;
		}
		CBufferString buf;
		conVarRefs[i]->TypeTraits()->ValueToString(&values[i], buf);
		CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
		cvar->set_name(conVarRefs[i]->GetName());
		cvar->set_value(buf.Get());
	}
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;
}

static_global const bbox_t PLAYER_BOUNDS = {{-16.0f, -16.0f, 0.0f}, {16.0f, 16.0f, 72.0f}};
static_global const f32 PLAYER_EYE_HEIGHT = 64.0f;
// Player half width along the axes of a trigger with any yaw, plus some room.
static_global const f32 WALL_PROBE_DISTANCE = 16.0f * 1.41421356f + 1.0f;
// Edges first, then corners.
static_global const i32 WALL_PROBE_DIRECTIONS[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

static_function void InitSpawnTraceFilter(CTraceFilter &filter)
{
	filter.m_bHitSolid = true;
	filter.m_bHitSolidRequiresGenerateContacts = true;
	filter.m_bShouldIgnoreDisabledPairs = true;
	filter.m_nCollisionGroup = COLLISION_GROUP_DEBRIS;
	filter.m_nInteractsWith = 0x2c3011;
	filter.m_bUnknown = true;
	filter.m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
	filter.m_nInteractsAs = 0x40000;
}

// Put the player on the trigger's floor at the given local position, or on whatever is above that floor.
static_function bool FindFloorPosition(CBaseTrigger *trigger, const CTransform &transform, const Vector &local, Vector &feet)
{
	Vector mins = trigger->m_pCollision()->m_vecMins();
	Vector maxs = trigger->m_pCollision()->m_vecMaxs();
	Vector floor = utils::TransformPoint(transform, Vector(local.x, local.y, mins.z)) + Vector(0.0f, 0.0f, 0.03125f);
	if (utils::IsSpawnValid(floor))
	{
		feet = floor;
		return true;
	}
	Vector start = utils::TransformPoint(transform, Vector(local.x, local.y, (mins.z + maxs.z) / 2));
	if (start.z <= floor.z || !utils::IsSpawnValid(start))
	{
		return false;
	}
	CTraceFilter filter;
	InitSpawnTraceFilter(filter);
	trace_t tr;
	INavPhysicsInterface::TraceShape(Ray_t(PLAYER_BOUNDS.mins, PLAYER_BOUNDS.maxs), start, floor, &filter, &tr);
	feet = tr.m_vEndPos;
	return true;
}

static_function Vector GetWallProbe(CBaseTrigger *trigger, i32 probe, bool outside)
{
	Vector mins = trigger->m_pCollision()->m_vecMins();
	Vector maxs = trigger->m_pCollision()->m_vecMaxs();
	Vector local = (mins + maxs) / 2;
	for (i32 axis = 0; axis < 2; axis++)
	{
		f32 halfSize = (maxs[axis] - mins[axis]) / 2;
		f32 offset = outside ? halfSize + WALL_PROBE_DISTANCE : MAX(halfSize - WALL_PROBE_DISTANCE, 0.0f);
		local[axis] += WALL_PROBE_DIRECTIONS[probe][axis] * offset;
	}
	return local;
}

static_function bool CanSeeTrigger(CBaseTrigger *trigger, const CTransform &transform, const Vector &eye)
{
	Vector mins = trigger->m_pCollision()->m_vecMins();
	Vector maxs = trigger->m_pCollision()->m_vecMaxs();
	Vector local = utils::InverseTransformPoint(transform, eye);
	for (i32 axis = 0; axis < 3; axis++)
	{
		local[axis] = Clamp(local[axis], mins[axis] + 0.03125f, maxs[axis] - 0.03125f);
	}
	CTraceFilter filter(MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, false);
	trace_t tr;
	INavPhysicsInterface::TraceLine(eye, utils::TransformPoint(transform, local), &filter, &tr);
	return !tr.DidHit();
}

static_function QAngle GetYawTowards(const Vector &from, const Vector &to)
{
	QAngle angles;
	VectorAngles(Vector(to.x - from.x, to.y - from.y, 0.0f), angles);
	angles.x = 0.0f;
	angles.z = 0.0f;
	return angles;
}

bool utils::IsSpawnValid(const Vector &origin)
{
	CTraceFilter filter;
	InitSpawnTraceFilter(filter);
	trace_t tr;
	INavPhysicsInterface::TraceShape(Ray_t(PLAYER_BOUNDS.mins, PLAYER_BOUNDS.maxs), origin, origin, &filter, &tr);
	if (tr.m_flFraction != 1.0 || tr.m_bStartInSolid)
	{
		return false;
	}
	return true;
}

bool utils::FindValidSpawn(Vector &origin, QAngle &angles, bool ignoreStuckCheck)
{
	bool foundValidSpawn = false;
	bool foundAnySpawn = false;
	bool searchCT = false;
	Vector spawnOrigin;
	QAngle spawnAngles;
	CBaseEntity *spawnEntity = NULL;
	while (!foundValidSpawn)
	{
		if (searchCT)
		{
			spawnEntity = FindEntityByClassname(spawnEntity, "info_player_counterterrorist");
		}
		else
		{
			spawnEntity = FindEntityByClassname(spawnEntity, "info_player_terrorist");
		}

		if (spawnEntity != NULL)
		{
			spawnOrigin = spawnEntity->m_CBodyComponent->m_pSceneNode->m_vecAbsOrigin;
			spawnAngles = spawnEntity->m_CBodyComponent->m_pSceneNode->m_angRotation;
			if (utils::IsSpawnValid(spawnOrigin))
			{
				origin = spawnOrigin;
				angles = spawnAngles;
				foundValidSpawn = true;
				break;
			}
			// Just set the spawn to a found spawn even if it's stuck, if we're ignoring stuck checks.
			if (ignoreStuckCheck)
			{
				origin = spawnOrigin;
				angles = spawnAngles;
				foundAnySpawn = true;
			}
		}
		else if (!searchCT)
		{
			searchCT = true;
		}
		else
		{
			break;
		}
	}
	return foundValidSpawn || foundAnySpawn;
}

bool utils::FindValidPositionForTrigger(CBaseTrigger *trigger, Vector &originDest, QAngle &anglesDest)
{
	if (!trigger)
	{
		return false;
	}
	CGameSceneNode *node = trigger->m_CBodyComponent()->m_pSceneNode();
	CTransform transform(node->m_vecAbsOrigin(), Quaternion(node->m_angAbsRotation()));
	Vector mins = trigger->m_pCollision()->m_vecMins();
	Vector maxs = trigger->m_pCollision()->m_vecMaxs();
	Vector center = (mins + maxs) / 2;
	Vector worldCenter = utils::TransformPoint(transform, center);

	Vector feet;
	if (FindFloorPosition(trigger, transform, center, feet))
	{
		originDest = feet;
		anglesDest = vec3_angle;
		return true;
	}
	for (i32 probe = 0; probe < 8; probe++)
	{
		if (FindFloorPosition(trigger, transform, GetWallProbe(trigger, probe, false), feet))
		{
			originDest = feet;
			anglesDest = GetYawTowards(feet, worldCenter);
			return true;
		}
	}

	// The whole trigger is obstructed (e.g. covered by a brush), try right outside of it.
	for (i32 probe = 0; probe < 8; probe++)
	{
		if (FindFloorPosition(trigger, transform, GetWallProbe(trigger, probe, true), feet)
			&& CanSeeTrigger(trigger, transform, feet + Vector(0.0f, 0.0f, PLAYER_EYE_HEIGHT)))
		{
			originDest = feet;
			anglesDest = GetYawTowards(feet, worldCenter);
			return true;
		}
	}

	// Lastly, on top of whatever covers it.
	Vector top = utils::TransformPoint(transform, Vector(center.x, center.y, maxs.z)) + Vector(0.0f, 0.0f, 0.03125f);
	Vector above = top + Vector(0.0f, 0.0f, PLAYER_BOUNDS.maxs.z);
	if (!utils::IsSpawnValid(above))
	{
		return false;
	}
	CTraceFilter filter;
	InitSpawnTraceFilter(filter);
	trace_t tr;
	INavPhysicsInterface::TraceShape(Ray_t(PLAYER_BOUNDS.mins, PLAYER_BOUNDS.maxs), above, top, &filter, &tr);
	originDest = tr.m_vEndPos;
	anglesDest = vec3_angle;
	return true;
}

void utils::ResetMapIfEmpty()
{
	// There are players in the server already, do not restart
	if (g_pKZUtils->GetPlayerCount() > 0)
	{
		return;
	}

	// Don't restart if the server is just up to map reload loops.
	if (g_pKZUtils->GetGlobals() && g_pKZUtils->GetGlobals()->curtime < 30.0f)
	{
		return;
	}

	// Another way the map reload can loop forever...
	if (CommandLine()->HasParm("-servertime"))
	{
		return;
	}

	KZ_LOG_INFO(LogChannel::General, "Server is empty, triggering map reload...\n");
	utils::ResetMap();
}

void utils::ResetMap()
{
	char cmd[MAX_PATH + 12]; // "changelevel " takes 12 characters
	if (g_pKZUtils->GetCurrentMapWorkshopID() == 0)
	{
		if (!g_pKZUtils->GetGlobals() || !g_pKZUtils->GetGlobals()->mapname.ToCStr() || g_pKZUtils->GetGlobals()->mapname.ToCStr()[0] == 0)
		{
			KZ_LOG_WARN(LogChannel::General, "Warning: Map name is empty, cannot reload the current map! Defaulting to de_dust2...\n");
			V_snprintf(cmd, sizeof(cmd), "changelevel de_dust2");
		}
		else
		{
			V_snprintf(cmd, sizeof(cmd), "changelevel %s", g_pKZUtils->GetGlobals()->mapname.ToCStr());
		}
	}
	else
	{
		V_snprintf(cmd, sizeof(cmd), "host_workshop_map %llu", g_pKZUtils->GetCurrentMapWorkshopID());
	}

	interfaces::pEngine->ServerCommand(cmd);
}

bool utils::IsServerSecure()
{
	if (!CommandLine()->HasParm("-dedicated"))
	{
		return false;
	}
	if (!g_steamAPI.SteamGameServer())
	{
		return !(CommandLine()->HasParm("-insecure") || CommandLine()->HasParm("-tools"));
	}
	return g_steamAPI.SteamGameServer()->BSecure();
}

void utils::UpdateServerVersion()
{
	FileHandle_t fp = g_pFullFileSystem->Open("steam.inf", "r");
	if (fp)
	{
		CUtlString line = g_pFullFileSystem->ReadLine(fp);
		while (!line.IsEmpty())
		{
			if (line.MatchesPattern(CUtlString(SERVER_VERSION_KEY) + "*"))
			{
				serverVersion = atoi(line.Get() + strlen(SERVER_VERSION_KEY));
				break;
			}
			line = g_pFullFileSystem->ReadLine(fp);
		}
		g_pFullFileSystem->Close(fp);
	}
}

u32 utils::GetServerVersion()
{
	return serverVersion;
}

bool utils::WriteBufferToFile(const char *relativePath, const std::vector<char> &buffer)
{
	char absPath[1024];
	V_snprintf(absPath, sizeof(absPath), "%s/csgo/%s", Plat_GetGameDirectory(), relativePath);

	// Ensure directory exists
	char dir[1024];
	V_ExtractFilePath(absPath, dir, sizeof(dir));
	if (dir[0])
	{
		std::filesystem::create_directories(dir);
	}

	char tmpPath[1024];
	V_snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", absPath);

	FILE *fp = fopen(tmpPath, "wb");
	if (!fp)
	{
		KZ_LOG_WARN(LogChannel::General, "Failed to open file for writing: %s\n", tmpPath);
		return false;
	}
	fwrite(buffer.data(), 1, buffer.size(), fp);
	fclose(fp);

#ifdef _WIN32
	if (!MoveFileExA(tmpPath, absPath, MOVEFILE_REPLACE_EXISTING))
#else
	if (rename(tmpPath, absPath) != 0)
#endif
	{
		remove(tmpPath);
		return false;
	}
	return true;
}

bool utils::ReadBufferFromFile(const char *relativePath, std::vector<char> &outBuffer)
{
	char absPath[1024];
	V_snprintf(absPath, sizeof(absPath), "%s/csgo/%s", Plat_GetGameDirectory(), relativePath);

	FILE *fp = fopen(absPath, "rb");
	if (!fp)
	{
		KZ_LOG_WARN(LogChannel::General, "Failed to open file for reading: %s\n", absPath);
		return false;
	}
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	outBuffer.resize(size);
	fread(outBuffer.data(), 1, size, fp);
	fclose(fp);
	return true;
}

void utils::RemoveFile(const char *relativePath)
{
	char absPath[1024];
	V_snprintf(absPath, sizeof(absPath), "%s/csgo/%s", Plat_GetGameDirectory(), relativePath);
	KZ_LOG_DEBUG(LogChannel::General, "Removing %s\n", absPath);
	if (remove(absPath) != 0)
	{
		KZ_LOG_WARN(LogChannel::General, "Failed to remove %s\n", absPath);
	}
}

bool utils::RenameFile(const char *oldRelativePath, const char *newRelativePath)
{
	char absOld[1024], absNew[1024];
	V_snprintf(absOld, sizeof(absOld), "%s/csgo/%s", Plat_GetGameDirectory(), oldRelativePath);
	V_snprintf(absNew, sizeof(absNew), "%s/csgo/%s", Plat_GetGameDirectory(), newRelativePath);
#ifdef _WIN32
	if (!MoveFileExA(absOld, absNew, MOVEFILE_REPLACE_EXISTING))
#else
	if (rename(absOld, absNew) != 0)
#endif
	{
		KZ_LOG_WARN(LogChannel::General, "Failed to rename %s to %s\n", absOld, absNew);
		return false;
	}
	return true;
}

bool utils::ParseColorName(const char *name, Color &out)
{
	struct Entry
	{
		const char *name;
		u8 r, g, b;
	};

	// clang-format off
	static constexpr Entry table[] =
	{
		{"red", 255, 0, 0},
		{"white", 255, 255, 255},
		{"black", 0, 0, 0},
		{"blue", 0, 0, 255},
		{"brown", 165, 42, 42},
		{"green", 0, 128, 0},
		{"yellow", 255, 255, 0},
		{"purple", 128, 0, 128}
	};
	// clang-format on

	for (auto &e : table)
	{
		if (KZ_STREQI(name, e.name))
		{
			out = Color(e.r, e.g, e.b, 255);
			return true;
		}
	}
	return false;
}

bool utils::ParseColorArgs(const CCommand *args, i32 startIdx, Color &out)
{
	// clang-format off
	if (args->ArgC() < startIdx + 3 
		|| !utils::IsNumeric(args->Arg(startIdx + 0)) 
		|| !utils::IsNumeric(args->Arg(startIdx + 1))
		|| !utils::IsNumeric(args->Arg(startIdx + 2)))
	{
		return false;
	}
	// clang-format on
	u8 r = (u8)Clamp(atoi(args->Arg(startIdx + 0)), 0, 255);
	u8 g = (u8)Clamp(atoi(args->Arg(startIdx + 1)), 0, 255);
	u8 b = (u8)Clamp(atoi(args->Arg(startIdx + 2)), 0, 255);
	u8 a = 255;
	if (args->ArgC() >= startIdx + 4 && utils::IsNumeric(args->Arg(startIdx + 3)))
	{
		a = (u8)Clamp(atoi(args->Arg(startIdx + 3)), 0, 255);
	}
	out = Color(r, g, b, a);
	return true;
}

bool utils::ParseSteamID2(std::string_view steamID, u64 &out)
{
	if (steamID.size() <= 10)
	{
		return false;
	}

	// clang-format off

	out = 0b0000000100010000000000000000000100000000000000000000000000000000
		| (atoll(&steamID[10]) << 1)
		| atoll(&steamID[8]);

	// clang-format on

	return true;
}
