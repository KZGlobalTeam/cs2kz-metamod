#pragma once
#include "common.h"
#include "interfaces.h"
#include "igameevents.h"
#include "engine2/inetworkgameserver.h"

class CCSPlayer_MovementServices;
class CUserCmd;

namespace hooks
{
	bool Initialize(char *error, size_t maxlen);
	void Cleanup();
	void HookEntities();

	void AddEntityHooks(CBaseEntity *entity);
	void RemoveEntityHooks(CBaseEntity *entity);

	void CallOriginalStartTouch(CBaseEntity *pThis, CBaseEntity *pOther);
	void CallOriginalTouch(CBaseEntity *pThis, CBaseEntity *pOther);
	void CallOriginalEndTouch(CBaseEntity *pThis, CBaseEntity *pOther);
	void CallOriginalPlayerRunCommand(CCSPlayer_MovementServices *pThis, CUserCmd *pCmd);
} // namespace hooks
