#include "networkbasetypes.pb.h"

#include "common.h"
#include "cs2kz.h"
#include "addresses.h"
#include "gameconfig.h"
#include "utils.h"
#include "convar.h"
#include "tier0/dbg.h"
#include "interfaces/interfaces.h"
#include "igameeventsystem.h"
#include "sdk/recipientfilters.h"
#include "public/networksystem/inetworkmessages.h"
#include "gametrace.h"

#include "module.h"
#include "detours.h"
#include "virtual.h"

#include "steam/steam_gameserver.h"
#include "filesystem.h"

#include "tier0/memdbgon.h"

#define FCVAR_FLAGS_TO_REMOVE (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_DEFENSIVE)

#define RESOLVE_SIG(gameConfig, name, type, variable, result) \
	type *variable = (decltype(variable))gameConfig->ResolveSignature(name); \
	if (!variable) \
	{ \
		Warning("Failed to find address for %s!\n", #name); \
		result = false; \
	}

CGameConfig *g_pGameConfig = NULL;
KZUtils *g_pKZUtils = NULL;
extern CSteamGameServerAPIContext g_steamAPI;

#define SERVER_VERSION_KEY "ServerVersion="
static_global u32 serverVersion;

bool utils::Initialize(ISmmAPI *ismm, char *error, size_t maxlen)
{
	modules::Initialize();
	if (!interfaces::Initialize(ismm, error, maxlen))
	{
		return false;
	}

	CBufferStringGrowable<256> gamedirpath;
	interfaces::pEngine->GetGameDir(gamedirpath);

	std::string gamedirname = CGameConfig::GetDirectoryName(gamedirpath.Get());
	const char *gamedataPath = "addons/cs2kz/gamedata/cs2kz-core.games.txt";

	g_pGameConfig = new CGameConfig(gamedirname, gamedataPath);
	char conf_error[255] = "";
	if (!g_pGameConfig->Init(g_pFullFileSystem, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlen, "Could not read %s: %s", g_pGameConfig->GetPath().c_str(), conf_error);
		Warning("%s\n", error);
		return false;
	}

	// Convoluted way of having GameEventManager regardless of lateloading
	if (!(interfaces::pGameEventManager = (IGameEventManager2 *)g_pGameConfig->ResolveSignatureFromMov("GameEventManager")))
	{
		return false;
	}

	bool sigResolved = true;
	RESOLVE_SIG(g_pGameConfig, "TracePlayerBBox", TracePlayerBBox_t, TracePlayerBBox);
	RESOLVE_SIG(g_pGameConfig, "GetLegacyGameEventListener", GetLegacyGameEventListener_t, GetLegacyGameEventListener);
	RESOLVE_SIG(g_pGameConfig, "SnapViewAngles", SnapViewAngles_t, SnapViewAngles);
	RESOLVE_SIG(g_pGameConfig, "EmitSound", EmitSoundFunc_t, EmitSound);
	RESOLVE_SIG(g_pGameConfig, "CCSPlayerController_SwitchTeam", SwitchTeam_t, SwitchTeam);
	RESOLVE_SIG(g_pGameConfig, "CBasePlayerController_SetPawn", SetPawn_t, SetPawn);
	RESOLVE_SIG(g_pGameConfig, "CreateEntityByName", CreateEntityByName_t, CreateEntityByName);
	RESOLVE_SIG(g_pGameConfig, "DispatchSpawn", DispatchSpawn_t, DispatchSpawn);
	RESOLVE_SIG(g_pGameConfig, "RemoveEntity", RemoveEntity_t, RemoveEntity);
	RESOLVE_SIG(g_pGameConfig, "DebugDrawMesh", DebugDrawMesh_t, DebugDrawMesh);
	RESOLVE_SIG(g_pGameConfig, "CreateBot", CreateBot_t, CreateBot);

	if (!sigResolved)
	{
		snprintf(error, maxlen, "Failed to resolve one or more signatures.");
		Warning("%s\n", error);
		return false;
	}
	g_pKZUtils = new KZUtils(TracePlayerBBox, GetLegacyGameEventListener, SnapViewAngles, EmitSound, SwitchTeam, SetPawn, CreateEntityByName,
							 DispatchSpawn, RemoveEntity, DebugDrawMesh, CreateBot);

	utils::UnlockConVars();
	utils::UnlockConCommands();
	utils::UpdateServerVersion();
	InitDetours();
	return true;
}

void utils::Cleanup()
{
	FlushAllDetours();
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
		META_CONPRINTF("Failed to find %s!\n", name);
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

bool utils::IsSpawnValid(const Vector &origin)
{
	bbox_t bounds = {{-16.0f, -16.0f, 0.0f}, {16.0f, 16.0f, 72.0f}};
	CTraceFilter filter;
	filter.m_bHitSolid = true;
	filter.m_bHitSolidRequiresGenerateContacts = true;
	filter.m_bShouldIgnoreDisabledPairs = true;
	filter.m_nCollisionGroup = COLLISION_GROUP_DEBRIS;
	filter.m_nInteractsWith = 0x2c3011;
	filter.m_bUnknown = true;
	filter.m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
	filter.m_nInteractsAs = 0x40000;
	trace_t tr;
	g_pKZUtils->TracePlayerBBox(origin, origin, bounds, &filter, tr);
	if (tr.m_flFraction != 1.0 || tr.m_bStartInSolid)
	{
		return false;
	}
	return true;
}

bool utils::FindValidSpawn(Vector &origin, QAngle &angles)
{
	bool foundValidSpawn = false;
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
	return foundValidSpawn;
}

bool utils::CanSeeBox(Vector origin, Vector mins, Vector maxs)
{
	Vector traceDest;

	for (int i = 0; i < 3; i++)
	{
		mins[i] += 0.03125;
		maxs[i] -= 0.03125;
		traceDest[i] = Clamp(origin[i], mins[i], maxs[i]);
	}
	bbox_t bounds {};
	CTraceFilter filter(MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, false);
	trace_t trace;
	g_pKZUtils->TracePlayerBBox(origin, traceDest, bounds, &filter, trace);

	return !trace.DidHit();
}

bool utils::FindValidPositionAroundCenter(Vector center, Vector distFromCenter, Vector extraOffset, Vector &originDest, QAngle &anglesDest)
{
	Vector testOrigin;
	i32 x, y;

	for (u32 i = 0; i < 3; i++)
	{
		x = i == 2 ? -1 : i;
		for (int j = 0; j < 3; j++)
		{
			y = j == 2 ? -1 : j;
			for (int z = -1; z <= 1; z++)
			{
				testOrigin = center;
				testOrigin[0] = testOrigin[0] + (distFromCenter[0] + extraOffset[0]) * x + 32.0f * x * 0.5;
				testOrigin[1] = testOrigin[1] + (distFromCenter[1] + extraOffset[1]) * y + 32.0f * y * 0.5;
				testOrigin[2] = testOrigin[2] + (distFromCenter[2] + extraOffset[2]) * z + 72.0f * z;

				if (utils::IsSpawnValid(testOrigin) && utils::CanSeeBox(testOrigin, center - distFromCenter, center + distFromCenter))
				{
					originDest = testOrigin;
					// Always look towards the center.
					Vector offsetVector;
					offsetVector[0] = -(distFromCenter[0] + extraOffset[0]) * x;
					offsetVector[1] = -(distFromCenter[1] + extraOffset[1]) * y;
					offsetVector[2] = -(distFromCenter[2] + extraOffset[2]) * z;
					VectorAngles(offsetVector, anglesDest);
					anglesDest[2] = 0.0; // Roll should always be 0.0
					return true;
				}
			}
		}
	}
	return false;
}

bool utils::FindValidPositionForTrigger(CBaseTrigger *trigger, Vector &originDest, QAngle &anglesDest)
{
	if (!trigger)
	{
		return false;
	}
	// Let's just assume this trigger isn't rotated at all...
	Vector mins = trigger->m_pCollision()->m_vecMins();
	Vector maxs = trigger->m_pCollision()->m_vecMaxs();
	Vector origin = trigger->m_CBodyComponent->m_pSceneNode->m_vecAbsOrigin();
	Vector center = origin + (maxs + mins) / 2;
	Vector distFromCenter = (maxs - mins) / 2;

	// If the center or the bottom center is valid (which should be the case most of the time), then we can skip all the complicated stuff.
	Vector bottomCenter = center;
	bottomCenter.z -= distFromCenter.z - 0.03125;
	if (utils::IsSpawnValid(bottomCenter))
	{
		originDest = bottomCenter;
		anglesDest = vec3_angle;
		return true;
	}

	if (utils::IsSpawnValid(center))
	{
		bbox_t bounds = {{-16.0f, -16.0f, 0.0f}, {16.0f, 16.0f, 72.0f}};
		CTraceFilter filter;
		filter.m_bHitSolid = true;
		filter.m_bHitSolidRequiresGenerateContacts = true;
		filter.m_bShouldIgnoreDisabledPairs = true;
		filter.m_nCollisionGroup = COLLISION_GROUP_DEBRIS;
		filter.m_nInteractsWith = 0x2c3011;
		filter.m_bUnknown = true;
		filter.m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
		filter.m_nInteractsAs = 0x40000;
		trace_t tr;
		g_pKZUtils->TracePlayerBBox(center, bottomCenter, bounds, &filter, tr);
		originDest = tr.m_vEndPos;
		anglesDest = vec3_angle;
		return true;
	}

	// TODO: This is mostly ported from GOKZ with no testing done on these. Might need to test this eventually.
	Vector extraOffset = {-33.03125f, -33.03125f, -72.03125f}; // Negative because we want to go inwards.
	if (FindValidPositionAroundCenter(center, distFromCenter, extraOffset, originDest, anglesDest))
	{
		return true;
	}
	// Test the positions right next to the trigger if the tests above fail.
	// This can fail when the trigger has a cover brush over it.
	extraOffset = {0.03125f, 0.03125f, 0.03125f};
	return FindValidPositionAroundCenter(center, distFromCenter, extraOffset, originDest, anglesDest);
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

	META_CONPRINTF("[KZ] Server is empty, triggering map reload...\n");
	utils::ResetMap();
}

void utils::ResetMap()
{
	char cmd[MAX_PATH + 12]; // "changelevel " takes 12 characters
	if (g_pKZUtils->GetCurrentMapWorkshopID() == 0)
	{
		if (!g_pKZUtils->GetGlobals() || !g_pKZUtils->GetGlobals()->mapname.ToCStr() || g_pKZUtils->GetGlobals()->mapname.ToCStr()[0] == 0)
		{
			META_CONPRINTF("[KZ] Warning: Map name is empty, cannot reload the current map! Defaulting to de_dust2...\n");
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
