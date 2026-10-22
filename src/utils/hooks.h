#pragma once
#include "common.h"
#include "interfaces.h"
#include "igameevents.h"
#include "engine2/inetworkgameserver.h"

class CCSPlayer_MovementServices;
class CUserCmd;

namespace hooks
{
	// Resolves every signature hook without installing anything, so a failure leaves nothing to undo.
	bool ResolveSignatures(char *error, size_t maxlen);
	void Initialize();
	void Cleanup();
	void HookEntities();

	void AddEntityHooks(CBaseEntity *entity);
	void RemoveEntityHooks(CBaseEntity *entity);

	void CallOriginalStartTouch(CBaseEntity *pThis, CBaseEntity *pOther);
	void CallOriginalTouch(CBaseEntity *pThis, CBaseEntity *pOther);
	void CallOriginalEndTouch(CBaseEntity *pThis, CBaseEntity *pOther);
	void CallOriginalPlayerRunCommand(CCSPlayer_MovementServices *pThis, CUserCmd *pCmd);
} // namespace hooks
