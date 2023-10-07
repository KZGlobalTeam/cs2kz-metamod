#pragma once
#include "common.h"
#include "addresses.h"
#include "cgameresourceserviceserver.h"
#include "cschemasystem.h"
#include "module.h"
#include <ISmmPlugin.h>
#include "utils/datatypes.h"

typedef void ClientPrintFilter_t(IRecipientFilter &filter, MsgDest msgDest, char *msgName, char *param1, char *param2, char *param3, char *param4);
extern ClientPrintFilter_t *UTIL_ClientPrintFilter;

namespace interfaces
{
	bool Initialize(ISmmAPI *ismm, char *error, size_t maxlen);

	inline CGameResourceService *pGameResourceServiceServer = nullptr;
	inline IVEngineServer2 *pEngine = nullptr;
	inline ISource2Server *pServer = nullptr;
}

namespace utils
{
	bool Initialize(ISmmAPI *ismm, char *error, size_t maxlen);
	void Cleanup();

	CGlobalVars *GetServerGlobals();
	void UnlockConVars();
	void UnlockConCommands();

	bool IsEntityPawn(CBaseEntity *entity);
	bool IsEntityController(CBaseEntity *entity);
	
	CBasePlayerController *GetController(CBaseEntity *entity);
	CBasePlayerController *GetController(CPlayerSlot slot);

	CPlayerSlot GetEntityPlayerSlot(CBaseEntity *entity);
	// Print functions do not work inside movement hooks, for some reasons...
	void PrintConsole(CBaseEntity *entity, char *format, ...);
	void PrintChat(CBaseEntity *entity, char *format, ...);
	void PrintCentre(CBaseEntity *entity, char *format, ...);
	void PrintAlert(CBaseEntity *entity, char *format, ...);

	void PrintConsoleAll(char *format, ...);
	void PrintChatAll(char *format, ...);
	void PrintCentreAll(char *format, ...);
	void PrintAlertAll(char *format, ...);

}
