#include "common.h"
#include "interfaces.h"
#include "igameevents.h"
#include "iserver.h"

namespace hooks
{
	void Initialize();
	void Cleanup();
	void HookEntities();

	void AddEntityHooks(CBaseEntity *entity);
	void RemoveEntityHooks(CBaseEntity *entity);

	void CallOriginalStartTouch(CBaseEntity *pThis, CBaseEntity *pOther);
	void CallOriginalTouch(CBaseEntity *pThis, CBaseEntity *pOther);
	void CallOriginalEndTouch(CBaseEntity *pThis, CBaseEntity *pOther);
} // namespace hooks
