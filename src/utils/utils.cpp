#include "networkbasetypes.pb.h"

#include "addresses.h"
#include "gameconfig.h"
#include "utils.h"
#include "convar.h"
#include "tier0/dbg.h"
#include "interfaces/interfaces.h"
#include "igameeventsystem.h"
#include "sdk/recipientfilters.h"
#include "public/networksystem/inetworkmessages.h"

#include "module.h"
#include "detours.h"
#include "virtual.h"

#include "tier0/memdbgon.h"

#define FCVAR_FLAGS_TO_REMOVE (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_MISSING0 | FCVAR_MISSING1 | FCVAR_MISSING2 | FCVAR_MISSING3)

#define RESOLVE_SIG(gameConfig, name, type, variable) \
	type *variable = (decltype(variable))gameConfig->ResolveSignature(name);	\
	if (!variable) { Warning("Failed to find address for %s!\n", #name); return false; }

CGameConfig *g_pGameConfig = NULL;
KZUtils *g_pKZUtils = NULL;

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
	// Initialize this later because we didn't have game config before.
	interfaces::pGameEventManager = (IGameEventManager2 *)(CALL_VIRTUAL(uintptr_t, g_pGameConfig->GetOffset("GameEventManager"), interfaces::pServer) - 8);

	RESOLVE_SIG(g_pGameConfig, "TracePlayerBBox", TracePlayerBBox_t, TracePlayerBBox);
	RESOLVE_SIG(g_pGameConfig, "InitGameTrace", InitGameTrace_t, InitGameTrace);
	RESOLVE_SIG(g_pGameConfig, "InitPlayerMovementTraceFilter", InitPlayerMovementTraceFilter_t, InitPlayerMovementTraceFilter);
	RESOLVE_SIG(g_pGameConfig, "GetLegacyGameEventListener", GetLegacyGameEventListener_t, GetLegacyGameEventListener);
	RESOLVE_SIG(g_pGameConfig, "SnapViewAngles", SnapViewAngles_t, SnapViewAngles);
	RESOLVE_SIG(g_pGameConfig, "EmitSound", EmitSoundFunc_t, EmitSound);
	RESOLVE_SIG(g_pGameConfig, "CCSPlayerController_SwitchTeam", SwitchTeam_t, SwitchTeam);
	RESOLVE_SIG(g_pGameConfig, "CBasePlayerController_SetPawn", SetPawn_t, SetPawn);

	g_pKZUtils = new KZUtils(
		TracePlayerBBox,
		InitGameTrace,
		InitPlayerMovementTraceFilter,
		GetLegacyGameEventListener,
		SnapViewAngles,
		EmitSound,
		SwitchTeam,
		SetPawn
	);

	utils::UnlockConVars();
	utils::UnlockConCommands();
	InitDetours();
	return true;
}

void utils::Cleanup()
{
	FlushAllDetours();
}

CBaseEntity2 *utils::FindEntityByClassname(CEntityInstance *start, const char *name)
{
	if (!GameEntitySystem())
	{
		return NULL;
	}
	EntityInstanceByClassIter_t iter(start, name);

	return static_cast<CBaseEntity2 * >(iter.Next());
}

void utils::UnlockConVars()
{
	if (!g_pCVar)
		return;

	ConVar *pCvar = nullptr;
	ConVarHandle hCvarHandle;
	hCvarHandle.Set(0);

	// Can't use FindFirst/Next here as it would skip cvars with certain flags, so just loop through the handles
	do
	{
		pCvar = g_pCVar->GetConVar(hCvarHandle);

		hCvarHandle.Set(hCvarHandle.Get() + 1);

		if (!pCvar || !(pCvar->flags & FCVAR_FLAGS_TO_REMOVE))
			continue;

		pCvar->flags &= ~FCVAR_FLAGS_TO_REMOVE;
	} while (pCvar);

}

void utils::UnlockConCommands()
{
	if (!g_pCVar)
		return;

	ConCommand *pConCommand = nullptr;
	ConCommand *pInvalidCommand = g_pCVar->GetCommand(ConCommandHandle());
	ConCommandHandle hConCommandHandle;
	hConCommandHandle.Set(0);

	do
	{
		pConCommand = g_pCVar->GetCommand(hConCommandHandle);

		hConCommandHandle.Set(hConCommandHandle.Get() + 1);

		if (!pConCommand || pConCommand == pInvalidCommand || !(pConCommand->GetFlags() & FCVAR_FLAGS_TO_REMOVE))
			continue;

		pConCommand->RemoveFlags(FCVAR_FLAGS_TO_REMOVE);
	} while (pConCommand && pConCommand != pInvalidCommand);
}

CBasePlayerController *utils::GetController(CBaseEntity2 *entity)
{
	CCSPlayerController *controller = nullptr;

	if (entity->IsPawn())
	{
		CBasePlayerPawn *pawn = static_cast<CBasePlayerPawn *>(entity);
		if (!pawn->m_hController().IsValid())
		{
			// Seems like the pawn lost its controller, we can try looping through the controllers to find this pawn instead.
			for (i32 i = 0; i <= g_pKZUtils->GetServerGlobals()->maxClients; i++)
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
	CBaseEntity2 *ent = static_cast<CBaseEntity2 *>(GameEntitySystem()->GetBaseEntity(CEntityIndex(slot.Get() + 1)));
	if (!ent)
	{
		return nullptr;
	}
	return ent->IsController() ? static_cast<CBasePlayerController *>(ent) : nullptr;
}


CPlayerSlot utils::GetEntityPlayerSlot(CBaseEntity2 *entity)
{
	CBasePlayerController *controller = utils::GetController(entity);
	if (!controller)
	{
		return -1;
	}
	else return controller->m_pEntity->m_EHandle.GetEntryIndex() - 1;
}

i32 utils::FormatTimerText(i32 ticks, char *buffer, i32 bufferSize)
{
	f64 time = ticks * ENGINE_FIXED_TICK_INTERVAL;
	i32 milliseconds = (i32)(time * 1000.0) % 1000;
	i32 seconds = ((i32)time) % 60;
	i32 minutes = (i32)(time / 60.0) % 60;
	i32 hours = (i32)(time / 3600.0);
	if (hours == 0)
	{
		return snprintf(buffer, bufferSize, "%02i:%02i.%03i", minutes, seconds, milliseconds);
	}
	else
	{
		return snprintf(buffer, bufferSize, "%02i:%02i:%02i.%03i", hours, minutes, seconds, milliseconds);
	}
}

void utils::PlaySoundToClient(CPlayerSlot player, const char *sound, f32 volume)
{
	u64 unknown = 0;
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
		a -= 360.0;
	else if (a < -180.0)
		a += 360.0;
	return a;
}

f32 utils::GetAngleDifference(const f32 source, const f32 target, const f32 c, bool relative)
{
	if (relative)
		return fmod((fmod(target - source, 2 * c) + 3 * c), 2 * c) - c;
	return fmod(fabs(target - source) + c, 2 * c) - c;
}

void utils::SendConVarValue(CPlayerSlot slot, ConVar *conVar, const char *value)
{
	INetworkSerializable *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	CNETMsg_SetConVar *msg = new CNETMsg_SetConVar;
	CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
	cvar->set_name(conVar->m_pszName);
	cvar->set_value(value);
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
}

void utils::SendMultipleConVarValues(CPlayerSlot slot, ConVar **conVar, const char **value, u32 size)
{
	INetworkSerializable *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	CNETMsg_SetConVar *msg = new CNETMsg_SetConVar;
	for (u32 i = 0; i < size; i++)
	{
		CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
		cvar->set_name(conVar[i]->m_pszName);
		cvar->set_value(value[i]);
	}
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
}

bool utils::IsSpawnValid(const Vector &origin)
{
	bbox_t bounds = {{-16.0f, -16.0f, 0.0f}, {16.0f, 16.0f, 72.0f}};
	CTraceFilterS2 filter;
	filter.attr.m_bHitSolid = true;
	filter.attr.m_bHitSolidRequiresGenerateContacts = true;
	filter.attr.m_bShouldIgnoreDisabledPairs = true;
	filter.attr.m_nCollisionGroup = COLLISION_GROUP_PLAYER_MOVEMENT;
	filter.attr.m_nInteractsWith = 0x2c3011;
	filter.attr.m_bUnkFlag3 = true;
	filter.attr.m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
	filter.attr.m_nInteractsAs = 0x40000;
	trace_t_s2 tr;
	g_pKZUtils->TracePlayerBBox(origin, origin, bounds, &filter, tr);
	if (tr.fraction != 1.0 || tr.startsolid)
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
	CBaseEntity2 *spawnEntity = NULL;
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
