#pragma once

#include "common.h"
#include "cgameresourceserviceserver.h"
#include "cschemasystem.h"
#include "igameevents.h"
#include "igameeventsystem.h"
#include "utils/datatypes.h"
#include <iserver.h>

namespace interfaces
{
	bool Initialize(ISmmAPI *ismm, char *error, size_t maxlen);

	inline CGameResourceService *pGameResourceServiceServer = nullptr;
	inline CSchemaSystem *pSchemaSystem = nullptr;
	inline IVEngineServer2 *pEngine = nullptr;
	inline ISource2Server *pServer = nullptr;
	inline IGameEventManager2 *pGameEventManager = nullptr;
	inline IGameEventSystem *pGameEventSystem = nullptr;
}

#define KZ_UTILS_INTERFACE "KZUtilsInterface"
// Expose some of the utility functions to other plugins.
class KZUtils
{
public:
	virtual void *GetSchemaSystemPointer();
	virtual void *GetSchemaStateChangedPointer();
	virtual void *GetSchemaNetworkStateChangedPointer();
	virtual CGlobalVars *GetServerGlobals();

	virtual void SetEntityMoveType(CBaseEntity2 *entity, MoveType_t movetype);
	virtual void EntityCollisionRulesChanged(CBaseEntity2 *entity);
	virtual CBaseEntity2 *FindEntityByClassname(CEntityInstance *start, const char *name);

	virtual CBasePlayerController *GetController(CBaseEntity2 *entity);
	virtual CBasePlayerController *GetController(CPlayerSlot slot);

	virtual CPlayerSlot GetEntityPlayerSlot(CBaseEntity2 *entity);

	virtual void InitPlayerMovementTraceFilter(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactWith, int collisionGroup);
	virtual void InitGameTrace(trace_t_s2 *trace);
	virtual void TracePlayerBBox(const Vector &start, const Vector &end, const bbox_t &bounds, CTraceFilterPlayerMovementCS *filter, trace_t_s2 &pm);

	virtual bool IsButtonDown(CInButtonState *buttons, u64 button, bool onlyDown = false);

	// Normalize the angle between -180 and 180.
	virtual f32 NormalizeDeg(f32 a);
	// Gets the difference in angle between 2 angles. 
	// c can be PI (for radians) or 180.0 (for degrees);
	virtual f32 GetAngleDifference(const f32 source, const f32 target, const f32 c, bool relative = false);
};

extern KZUtils *g_pKZUtils;
