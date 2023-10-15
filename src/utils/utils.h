#pragma once
#include "common.h"
#include "addresses.h"
#include "module.h"
#include "utils/schema.h"
#include "utils/interfaces.h"
#include "utils/datatypes.h"

typedef void ClientPrintFilter_t(IRecipientFilter &filter, MsgDest msgDest, const char *msgName, const char *param1, const char *param2, const char *param3, const char *param4);
typedef void InitPlayerMovementTraceFilter_t(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactWith, int collisionGroup);
typedef void TracePlayerBBoxForGround_t (const Vector &start, const Vector &end, const Vector &minsSrc,
	const Vector &maxsSrc, CTraceFilterPlayerMovementCS *filter, trace_t_s2 &pm, float minGroundNormalZ, bool overwriteEndpos, int *pCounter);
typedef void InitGameTrace_t(trace_t_s2 *trace);
typedef IGameEventListener2 *GetLegacyGameEventListener_t(CEntityIndex index);
extern ClientPrintFilter_t *UTIL_ClientPrintFilter;

namespace utils
{
	bool Initialize(ISmmAPI *ismm, char *error, size_t maxlen);
	void Cleanup();

	CGlobalVars *GetServerGlobals();
	void UnlockConVars();
	void UnlockConCommands();

	void SetEntityMoveType(CBaseEntity *entity, MoveType_t movetype);
	void EntityCollisionRulesChanged(CBaseEntity *entity);

	bool IsEntityPawn(CBaseEntity *entity);
	bool IsEntityController(CBaseEntity *entity);
	
	CBasePlayerController *GetController(CBaseEntity *entity);
	CBasePlayerController *GetController(CPlayerSlot slot);

	extern InitPlayerMovementTraceFilter_t *InitPlayerMovementTraceFilter;
	extern TracePlayerBBoxForGround_t *TracePlayerBBoxForGround;
	extern InitGameTrace_t *InitGameTrace;
	extern GetLegacyGameEventListener_t *GetLegacyGameEventListener;

	CPlayerSlot GetEntityPlayerSlot(CBaseEntity *entity);
	
	// Print functions do not work inside movement hooks, for some reasons...
	void PrintConsole(CBaseEntity *entity, const char *format, ...);
	void PrintChat(CBaseEntity *entity, const char *format, ...);
	void PrintCentre(CBaseEntity *entity, const char *format, ...);
	void PrintAlert(CBaseEntity *entity, const char *format, ...);

	void PrintConsoleAll(const char *format, ...);
	void PrintChatAll(const char *format, ...);
	void PrintCentreAll(const char *format, ...);
	void PrintAlertAll(const char *format, ...);

}
