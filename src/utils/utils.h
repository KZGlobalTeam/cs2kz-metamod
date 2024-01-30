#pragma once
#include "common.h"
#include "utils/interfaces.h"
#include "sdk/datatypes.h"
#include "igameevents.h"

class KZUtils;
class CBasePlayerController;

typedef void InitPlayerMovementTraceFilter_t(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactWith, int collisionGroup);
typedef void InitGameTrace_t(trace_t_s2 *trace);
typedef IGameEventListener2 *GetLegacyGameEventListener_t(CPlayerSlot slot);
typedef void SnapViewAngles_t(CBasePlayerPawn *pawn, const QAngle &angle);
typedef CBaseEntity2 *FindEntityByClassname_t(CEntitySystem *, CEntityInstance *, const char *);
typedef SndOpEventGuid_t EmitSoundFunc_t(IRecipientFilter &filter, CEntityIndex ent, const EmitSound_t &params);

typedef void TracePlayerBBox_t(const Vector &start, const Vector &end, const bbox_t &bounds, CTraceFilterS2 *filter, trace_t_s2 &pm);
typedef void SwitchTeam_t(CCSPlayerController *controller, int team);
typedef void SetPawn_t(CBasePlayerController *controller, CCSPlayerPawn *pawn, bool, bool);

namespace utils
{
	bool Initialize(ISmmAPI *ismm, char *error, size_t maxlen);
	void Cleanup();

	// ConVars
	void UnlockConVars();
	void UnlockConCommands();
	void SendConVarValue(CPlayerSlot slot, ConVar *cvar, const char *value);
	void SendMultipleConVarValues(CPlayerSlot slot, ConVar **cvars, const char **values, u32 size);

	CBaseEntity2 *FindEntityByClassname(CEntityInstance *start, const char *name);

	CBasePlayerController *GetController(CBaseEntity2 *entity);
	CBasePlayerController *GetController(CPlayerSlot slot);

	CPlayerSlot GetEntityPlayerSlot(CBaseEntity2 *entity);

	extern SnapViewAngles_t *SnapViewAngles;

	extern GetLegacyGameEventListener_t *GetLegacyGameEventListener;

	extern SwitchTeam_t *SwitchTeam;
	extern SetPawn_t *SetPawn;
	// Tracing stuff
	extern InitPlayerMovementTraceFilter_t *InitPlayerMovementTraceFilter;
	extern InitGameTrace_t *InitGameTrace;
	extern TracePlayerBBox_t *TracePlayerBBox;

	// Normalize the angle between -180 and 180.
	f32 NormalizeDeg(f32 a);
	// Gets the difference in angle between 2 angles. 
	// c can be PI (for radians) or 180.0 (for degrees);
	f32 GetAngleDifference(const f32 x, const f32 y, const f32 c, bool relative = false);

	// Print functions
	void PrintConsole(CBaseEntity2 *entity, const char *format, ...);
	void PrintChat(CBaseEntity2 *entity, const char *format, ...);
	void PrintCentre(CBaseEntity2 *entity, const char *format, ...);
	void PrintAlert(CBaseEntity2 *entity, const char *format, ...);
	void PrintHTMLCentre(CBaseEntity2 *entity, const char *format, ...); // This one uses HTML formatting.

	void PrintConsoleAll(const char *format, ...);
	void PrintChatAll(const char *format, ...);
	void PrintCentreAll(const char *format, ...);
	void PrintAlertAll(const char *format, ...);
	void PrintHTMLCentreAll(const char *format, ...); // This one uses HTML formatting.

	// Color print
	void CPrintChat(CBaseEntity2 *entity, const char *format, ...);
	void CPrintChatAll(const char *format, ...);

	i32 FormatTimerText(i32 ticks, char *buffer, i32 bufferSize);

	// Sounds
	extern EmitSoundFunc_t *EmitSound;
	void PlaySoundToClient(CPlayerSlot player, const char *sound, f32 volume = 1.0f);

	// Return true if the spawn found is truly valid (not in the ground or out of bounds)
	bool IsSpawnValid(const Vector &origin);
	bool FindValidSpawn(Vector &origin, QAngle &angles);
}
