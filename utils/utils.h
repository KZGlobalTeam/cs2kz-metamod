#pragma once
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
	
	CBasePlayerController *GetController(CPlayerSlot slot);

	// Print functions do not work inside movement hooks, for some reasons...
	void PrintConsole(CBasePlayerController *player, char *format, ...);
	void PrintChat(CBasePlayerController *player, char *format, ...);
	void PrintCentre(CBasePlayerController *player, char *format, ...);
	void PrintAlert(CBasePlayerController *player, char *format, ...);
}
