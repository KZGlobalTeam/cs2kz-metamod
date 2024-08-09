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

#include "tier0/memdbgon.h"

#define FCVAR_FLAGS_TO_REMOVE (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_MISSING0 | FCVAR_MISSING1 | FCVAR_MISSING2 | FCVAR_MISSING3)

#define RESOLVE_SIG(gameConfig, name, type, variable) \
	type *variable = (decltype(variable))gameConfig->ResolveSignature(name); \
	if (!variable) \
	{ \
		Warning("Failed to find address for %s!\n", #name); \
		return false; \
	}

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

	// Convoluted way of having GameEventManager regardless of lateloading
	if (!(interfaces::pGameEventManager = (IGameEventManager2 *)g_pGameConfig->ResolveSignatureFromMov("GameEventManager")))
	{
		return false;
	}

	RESOLVE_SIG(g_pGameConfig, "TracePlayerBBox", TracePlayerBBox_t, TracePlayerBBox);
	RESOLVE_SIG(g_pGameConfig, "InitGameTrace", InitGameTrace_t, InitGameTrace);
	RESOLVE_SIG(g_pGameConfig, "InitPlayerMovementTraceFilter", InitPlayerMovementTraceFilter_t, InitPlayerMovementTraceFilter);
	RESOLVE_SIG(g_pGameConfig, "GetLegacyGameEventListener", GetLegacyGameEventListener_t, GetLegacyGameEventListener);
	RESOLVE_SIG(g_pGameConfig, "SnapViewAngles", SnapViewAngles_t, SnapViewAngles);
	RESOLVE_SIG(g_pGameConfig, "EmitSound", EmitSoundFunc_t, EmitSound);
	RESOLVE_SIG(g_pGameConfig, "CCSPlayerController_SwitchTeam", SwitchTeam_t, SwitchTeam);
	RESOLVE_SIG(g_pGameConfig, "CBasePlayerController_SetPawn", SetPawn_t, SetPawn);

	g_pKZUtils = new KZUtils(TracePlayerBBox, InitGameTrace, InitPlayerMovementTraceFilter, GetLegacyGameEventListener, SnapViewAngles, EmitSound,
							 SwitchTeam, SetPawn);

	utils::UnlockConVars();
	utils::UnlockConCommands();

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

	ConVar *pCvar = nullptr;
	ConVarHandle hCvarHandle;
	hCvarHandle.Set(0);

	// Can't use FindFirst/Next here as it would skip cvars with certain flags, so just loop through the handles
	do
	{
		pCvar = g_pCVar->GetConVar(hCvarHandle);

		hCvarHandle.Set(hCvarHandle.Get() + 1);

		if (!pCvar || !(pCvar->flags & FCVAR_FLAGS_TO_REMOVE))
		{
			continue;
		}

		pCvar->flags &= ~FCVAR_FLAGS_TO_REMOVE;
	} while (pCvar);
}

void utils::UnlockConCommands()
{
	if (!g_pCVar)
	{
		return;
	}

	ConCommand *pConCommand = nullptr;
	ConCommand *pInvalidCommand = g_pCVar->GetCommand(ConCommandHandle());
	ConCommandHandle hConCommandHandle;
	hConCommandHandle.Set(0);

	do
	{
		pConCommand = g_pCVar->GetCommand(hConCommandHandle);

		hConCommandHandle.Set(hConCommandHandle.Get() + 1);

		if (!pConCommand || pConCommand == pInvalidCommand || !(pConCommand->GetFlags() & FCVAR_FLAGS_TO_REMOVE))
		{
			continue;
		}

		pConCommand->RemoveFlags(FCVAR_FLAGS_TO_REMOVE);
	} while (pConCommand && pConCommand != pInvalidCommand);
}

CBasePlayerController *utils::GetController(CBaseEntity *entity)
{
	CCSPlayerController *controller = nullptr;
	if (!V_stricmp(entity->GetClassname(), "observer"))
	{
		CBasePlayerPawn *pawn = static_cast<CBasePlayerPawn *>(entity);
		if (!pawn->m_hController().IsValid() || pawn->m_hController.Get() == 0)
		{
			for (i32 i = 0; i <= g_pKZUtils->GetServerGlobals()->maxClients; i++)
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

void utils::SendConVarValue(CPlayerSlot slot, ConVar *conVar, const char *value)
{
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	auto msg = netmsg->AllocateMessage()->ToPB<CNETMsg_SetConVar>();
	CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
	cvar->set_name(conVar->m_pszName);
	cvar->set_value(value);
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;
}

void utils::SendMultipleConVarValues(CPlayerSlot slot, ConVar **conVar, const char **value, u32 size)
{
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
	auto msg = netmsg->AllocateMessage()->ToPB<CNETMsg_SetConVar>();
	for (u32 i = 0; i < size; i++)
	{
		CMsg_CVars_CVar *cvar = msg->mutable_convars()->add_cvars();
		cvar->set_name(conVar[i]->m_pszName);
		cvar->set_value(value[i]);
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
	filter.m_nCollisionGroup = COLLISION_GROUP_PLAYER_MOVEMENT;
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
