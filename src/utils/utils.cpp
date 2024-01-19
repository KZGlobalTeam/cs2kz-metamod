#include "networkbasetypes.pb.h"

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

#define RESOLVE_SIG(module, sig, variable) variable = (decltype(variable))module->FindSignature((const byte *)sig.data, sig.length); \
	if (!variable) { Warning("Failed to find address for %s!\n", #sig); return false; }

InitPlayerMovementTraceFilter_t *utils::InitPlayerMovementTraceFilter = NULL;
InitGameTrace_t *utils::InitGameTrace = NULL;
GetLegacyGameEventListener_t *utils::GetLegacyGameEventListener = NULL;
SnapViewAngles_t *utils::SnapViewAngles = NULL;
EmitSoundFunc_t *utils::EmitSound = NULL;
TracePlayerBBox_t *utils::TracePlayerBBox = NULL;
FindEntityByClassname_t *FindEntityByClassnameFunc = NULL;


void modules::Initialize()
{
	modules::engine = new CModule(ROOTBIN, "engine2");
	modules::tier0 = new CModule(ROOTBIN, "tier0");
	modules::server = new CModule(GAMEBIN, "server");
	modules::schemasystem = new CModule(ROOTBIN, "schemasystem");
	modules::steamnetworkingsockets = new CModule(ROOTBIN, "steamnetworkingsockets");
}

bool interfaces::Initialize(ISmmAPI *ismm, char *error, size_t maxlen)
{
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, interfaces::pGameResourceServiceServer, CGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2GameClients, ISource2GameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities, SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, interfaces::pEngine, IVEngineServer2, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetServerFactory, interfaces::pServer, ISource2Server, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_CURRENT(GetEngineFactory, interfaces::pSchemaSystem, CSchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, interfaces::pGameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	interfaces::pGameEventManager = (IGameEventManager2 *)(CALL_VIRTUAL(uintptr_t, offsets::GetEventManager, interfaces::pServer) - 8);

	return true;
}

bool utils::Initialize(ISmmAPI *ismm, char *error, size_t maxlen)
{
	modules::Initialize();
	if (!interfaces::Initialize(ismm, error, maxlen))
	{
		return false;
	}

	utils::UnlockConVars();
	utils::UnlockConCommands();


	RESOLVE_SIG(modules::server, sigs::NetworkStateChanged, schema::NetworkStateChanged);
	RESOLVE_SIG(modules::server, sigs::StateChanged, schema::StateChanged);

	RESOLVE_SIG(modules::server, sigs::TracePlayerBBox, utils::TracePlayerBBox);
	RESOLVE_SIG(modules::server, sigs::InitGameTrace, utils::InitGameTrace);
	RESOLVE_SIG(modules::server, sigs::InitPlayerMovementTraceFilter, utils::InitPlayerMovementTraceFilter);
	RESOLVE_SIG(modules::server, sigs::GetLegacyGameEventListener, utils::GetLegacyGameEventListener);
	RESOLVE_SIG(modules::server, sigs::SnapViewAngles, utils::SnapViewAngles);
	RESOLVE_SIG(modules::server, sigs::EmitSound, utils::EmitSound);
	RESOLVE_SIG(modules::server, sigs::FindEntityByClassname, FindEntityByClassnameFunc);

	InitDetours();
	return true;
}

void utils::Cleanup()
{
	FlushAllDetours();
}

CBaseEntity2 *utils::FindEntityByClassname(CEntityInstance *start, const char *name)
{
	if (!g_pEntitySystem)
	{
		return NULL;
	}
	return FindEntityByClassnameFunc(g_pEntitySystem, start, name);
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
	if (!g_pEntitySystem || slot.Get() < 0 || slot.Get() > MAXPLAYERS)
	{
		return nullptr;
	}
	CBaseEntity2 *ent = static_cast<CBaseEntity2 *>(g_pEntitySystem->GetBaseEntity(CEntityIndex(slot.Get() + 1)));
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
	utils::EmitSound(filter, player.Get() + 1, soundParams);
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
