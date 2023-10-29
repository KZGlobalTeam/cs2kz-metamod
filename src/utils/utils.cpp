
#include "utils.h"
#include "convar.h"
#include "strtools.h"
#include "tier0/dbg.h"
#include "interfaces/interfaces.h"

#include "module.h"
#include "detours.h"
#include "virtual.h"
#include "recipientfilters.h"

#include "tier0/memdbgon.h"

#define FCVAR_FLAGS_TO_REMOVE (FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_MISSING0 | FCVAR_MISSING1 | FCVAR_MISSING2 | FCVAR_MISSING3)

#define RESOLVE_SIG(module, sig, variable) variable = (decltype(variable))module->FindSignature((const byte *)sig.data, sig.length); \
	if (!variable) { Warning("Failed to find address for %s!\n", #sig); return false; }

ClientPrintFilter_t *UTIL_ClientPrintFilter = NULL;
InitPlayerMovementTraceFilter_t *utils::InitPlayerMovementTraceFilter = NULL;
TracePlayerBBoxForGround_t *utils::TracePlayerBBoxForGround = NULL;
InitGameTrace_t *utils::InitGameTrace = NULL;
GetLegacyGameEventListener_t *utils::GetLegacyGameEventListener = NULL;
SnapViewAngles_t *utils::SnapViewAngles = NULL;

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
	
	RESOLVE_SIG(modules::server, sigs::UTIL_ClientPrintFilter, UTIL_ClientPrintFilter);

	RESOLVE_SIG(modules::server, sigs::NetworkStateChanged, schema::NetworkStateChanged);
	RESOLVE_SIG(modules::server, sigs::StateChanged, schema::StateChanged);
	
	RESOLVE_SIG(modules::server, sigs::TracePlayerBBoxForGround, utils::TracePlayerBBoxForGround);
	RESOLVE_SIG(modules::server, sigs::InitGameTrace, utils::InitGameTrace);
	RESOLVE_SIG(modules::server, sigs::InitPlayerMovementTraceFilter, utils::InitPlayerMovementTraceFilter);
	RESOLVE_SIG(modules::server, sigs::GetLegacyGameEventListener, utils::GetLegacyGameEventListener);
	RESOLVE_SIG(modules::server, sigs::SnapViewAngles, utils::SnapViewAngles);

	InitDetours();
	return true;
}

void utils::Cleanup()
{
	FlushAllDetours();
}

CGlobalVars *utils::GetServerGlobals()
{
	return interfaces::pEngine->GetServerGlobals();
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

void utils::SetEntityMoveType(CBaseEntity *entity, MoveType_t movetype)
{
	CALL_VIRTUAL(void, offsets::SetMoveType, entity, movetype);
}

void utils::EntityCollisionRulesChanged(CBaseEntity *entity)
{
	CALL_VIRTUAL(void, offsets::CollisionRulesChanged, entity);
}

bool utils::IsEntityPawn(CBaseEntity *entity)
{
	return CALL_VIRTUAL(bool, offsets::IsEntityPawn, entity);
}

bool utils::IsEntityController(CBaseEntity *entity)
{
	return CALL_VIRTUAL(bool, offsets::IsEntityController, entity);
}

CBasePlayerController *utils::GetController(CBaseEntity *entity)
{
	CBasePlayerController *controller = nullptr;

	if (utils::IsEntityPawn(entity))
	{
		return static_cast<CBasePlayerPawn *>(entity)->m_hController().Get();
	}
	else if (utils::IsEntityController(entity))
	{
		return static_cast<CBasePlayerController*>(entity);
	}
	else
	{
		return nullptr;
	}
}

CBasePlayerController *utils::GetController(CPlayerSlot slot)
{
	return static_cast<CBasePlayerController*>(g_pEntitySystem->GetBaseEntity(CEntityIndex(slot.Get() + 1)));
}

bool utils::IsButtonDown(CInButtonState *buttons, u64 button, bool onlyDown)
{
	if (onlyDown)
	{
		return buttons->m_pButtonStates[0] & button;
	}
	else
	{
		bool multipleKeys = (button & (button - 1));
		if (multipleKeys)
		{
			u64 currentButton = button;
			u64 key = 0;
			if (button)
			{
				while (true)
				{
					if (currentButton & 1)
					{
						u64 keyMask = 1ull << key;
						EInButtonState keyState = (EInButtonState)(keyMask && buttons->m_pButtonStates[0] + (keyMask && buttons->m_pButtonStates[1]) * 2 + (keyMask && buttons->m_pButtonStates[2]) * 4);
						if (keyState > IN_BUTTON_DOWN_UP)
						{
							return true;
						}
					}
					key++;
					currentButton >>= 1;
					if (!currentButton) return !!(buttons->m_pButtonStates[0] & button);
				}
			}
		}
		else
		{
			EInButtonState keyState = (EInButtonState)(button & buttons->m_pButtonStates[0] + (button & buttons->m_pButtonStates[1]) * 2 + (button & buttons->m_pButtonStates[2]) * 4);
			if (keyState > IN_BUTTON_DOWN_UP)
			{
				return true;
			}
			return !!(buttons->m_pButtonStates[0] & button);
		}
	}
}
CPlayerSlot utils::GetEntityPlayerSlot(CBaseEntity *entity)
{
	CBasePlayerController *controller = utils::GetController(entity);
	if (!controller)
	{
		return -1;
	}
	else return controller->m_pEntity->m_EHandle.GetEntryIndex() - 1;
}

#define FORMAT_STRING(buffer) \
	va_list args; \
	va_start(args, format); \
	char buffer[1024]; \
	vsnprintf(buffer, sizeof(buffer), format, args); \
	va_end(args);

void utils::PrintConsole(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	UTIL_ClientPrintFilter(filter, HUD_PRINTCONSOLE, buffer, "", "", "", "");
}

void utils::PrintChat(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	UTIL_ClientPrintFilter(filter, HUD_PRINTTALK, buffer, "", "", "", "");
}

void utils::PrintCentre(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	UTIL_ClientPrintFilter(filter, HUD_PRINTCENTER, buffer, "", "", "", "");
}

void utils::PrintAlert(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	UTIL_ClientPrintFilter(filter, HUD_PRINTALERT, buffer, "", "", "", "");
}

void utils::PrintHTMLCentre(CBaseEntity *entity, const char *format, ...)
{
	CBasePlayerController *controller = utils::GetController(entity);
	if (!controller) return;

	FORMAT_STRING(buffer);

	IGameEvent *event = interfaces::pGameEventManager->CreateEvent("show_survival_respawn_status");
	if (!event) return;
	event->SetString("loc_token", buffer);
	event->SetInt("duration", 5);
	event->SetInt("userid", -1);

	CPlayerSlot slot = controller->entindex() - 1;
	IGameEventListener2 *listener = utils::GetLegacyGameEventListener(slot);
	listener->FireGameEvent(event);
	interfaces::pGameEventManager->FreeEvent(event);
}

void utils::PrintConsoleAll(const char *format, ...)
{
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	UTIL_ClientPrintFilter(filter, HUD_PRINTCONSOLE, buffer, "", "", "", "");
}

void utils::PrintChatAll(const char *format, ...)
{
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	UTIL_ClientPrintFilter(filter, HUD_PRINTTALK, buffer, "", "", "", "");
}

void utils::PrintCentreAll(const char *format, ...)
{
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	UTIL_ClientPrintFilter(filter, HUD_PRINTCENTER, buffer, "", "", "", "");
}

void utils::PrintAlertAll(const char *format, ...)
{
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	UTIL_ClientPrintFilter(filter, HUD_PRINTALERT, buffer, "", "", "", "");
}

void utils::PrintHTMLCentreAll(const char *format, ...)
{
	FORMAT_STRING(buffer);

	IGameEvent *event = interfaces::pGameEventManager->CreateEvent("show_survival_respawn_status");
	if (!event) return;
	event->SetString("loc_token", buffer);
	event->SetInt("duration", 5);
	event->SetInt("userid", -1);

	interfaces::pGameEventManager->FireEvent(event);
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

f32 utils::GetAngleDifference(const f32 x, const f32 y, const f32 c)
{
	return fmod(fabs(x - y) + c, 2 * c) - c;
}
