#pragma once
#include "common.h"
#include "addresses.h"
#include "module.h"
#include "utils/schema.h"
#include "utils/interfaces.h"
#include "utils/datatypes.h"

typedef void InitPlayerMovementTraceFilter_t(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactWith, int collisionGroup);
typedef void TracePlayerBBoxForGround_t (const Vector &start, const Vector &end, const Vector &minsSrc,
	const Vector &maxsSrc, CTraceFilterPlayerMovementCS *filter, trace_t_s2 &pm, float minGroundNormalZ, bool overwriteEndpos, int *pCounter);
typedef void InitGameTrace_t(trace_t_s2 *trace);
typedef IGameEventListener2 *GetLegacyGameEventListener_t(CPlayerSlot slot);
typedef void SnapViewAngles_t(CBasePlayerPawn *pawn, const QAngle &angle);
// TODO: why?
#ifdef _WIN32
typedef void EmitSoundFunc_t(u64 &unknown, IRecipientFilter &filter, CEntityIndex ent, const EmitSound_t &params);
#else
typedef void EmitSoundFunc_t(IRecipientFilter &filter, CEntityIndex ent, const EmitSound_t &params);
#endif

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
	extern SnapViewAngles_t *SnapViewAngles;
	extern EmitSoundFunc_t *EmitSound;

	bool IsButtonDown(CInButtonState *buttons, u64 button, bool onlyDown = false);
	CPlayerSlot GetEntityPlayerSlot(CBaseEntity *entity);

	// Normalize the angle between -180 and 180.
	f32 NormalizeDeg(f32 a);
	// Gets the difference in angle between 2 angles. 
    // c can be PI (for radians) or 180.0 (for degrees);
	f32 GetAngleDifference(const f32 x, const f32 y, const f32 c);
	
	// Print functions
	void PrintConsole(CBaseEntity *entity, const char *format, ...);
	void PrintChat(CBaseEntity *entity, const char *format, ...);
	void PrintCentre(CBaseEntity *entity, const char *format, ...);
	void PrintAlert(CBaseEntity *entity, const char *format, ...);
	void PrintHTMLCentre(CBaseEntity *entity, const char *format, ...); // This one uses HTML formatting.

	void PrintConsoleAll(const char *format, ...);
	void PrintChatAll(const char *format, ...);
	void PrintCentreAll(const char *format, ...);
	void PrintAlertAll(const char *format, ...);
	void PrintHTMLCentreAll(const char *format, ...); // This one uses HTML formatting.
	
	i32 FormatTimerText(i32 ticks, char *buffer, i32 bufferSize);
	
	void PlaySoundToClient(CPlayerSlot player, const char *sound, f32 volume = 1.0f);

	// Color print
	void CPrintChat(CBaseEntity *entity, const char *format, ...);
	void CPrintChatAll(const char *format, ...);

	void SendConVarValue(CPlayerSlot slot, ConVar *cvar, const char *value);
	void SendMultipleConVarValues(CPlayerSlot slot, ConVar **cvars, const char **values, int size);
}
