#pragma once
#include "addresses.h"
#include "cgameresourceserviceserver.h"
#include "cschemasystem.h"
#include "module.h"
#include <ISmmPlugin.h>


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
