#pragma once
#include "common.h"
#include "addresses.h"
#include "module.h"
#include "utils/schema.h"
#include "utils/interfaces.h"
#include "utils/datatypes.h"

typedef void ClientPrintFilter_t(IRecipientFilter &filter, MsgDest msgDest, char *msgName, char *param1, char *param2, char *param3, char *param4);
typedef void InitPlayerMovementTraceFilter_t(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactAs /*should be with?*/, int collisionGroup);
typedef void TracePlayerBBoxForGround_t (const Vector &start, const Vector &end, const Vector &minsSrc,
	const Vector &maxsSrc, CTraceFilterPlayerMovementCS *filter, trace_t_s2 &pm, float minGroundNormalZ, bool overwriteEndpos, int *pCounter);
typedef void InitGameTrace_t(trace_t_s2 &trace);

extern ClientPrintFilter_t *UTIL_ClientPrintFilter;
extern InitPlayerMovementTraceFilter_t *InitPlayerMovementTraceFilter;
extern TracePlayerBBoxForGround_t *TracePlayerBBoxForGround;
extern InitGameTrace_t *InitGameTrace;
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
