#pragma once
#include "addresses.h"
#include "cgameresourceserviceserver.h"
#include "cschemasystem.h"
#include "module.h"
#include <ISmmPlugin.h>
#include "utils/datatypes.h"

typedef void ClientPrint_t(CBaseEntity *client, MsgDest msgDest, char *msgName, char *param1, char *param2, char *param3, char *param4);
ClientPrint_t *ClientPrint = NULL;

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
}
