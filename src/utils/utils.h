#pragma once
#include "common.h"
#include "utils/interfaces.h"
#include "sdk/datatypes.h"
#include "igameevents.h"

class KZUtils;
class CBasePlayerController;

namespace utils
{
	bool Initialize(ISmmAPI *ismm, char *error, size_t maxlen);
	void Cleanup();

	// ConVars/ConCommands
	void UnlockConVars();
	void UnlockConCommands();

	bool SetConVarValue(CPlayerSlot slot, const char *name, const char *value, bool replicate, bool triggerCallback = false);
	bool SetConVarValue(CPlayerSlot slot, ConVarRefAbstract conVarRef, const char *value, bool replicate, bool triggerCallback = false);
	bool SetConVarValue(CPlayerSlot slot, ConVarRefAbstract conVarRef, const CVValue_t *value, bool replicate, bool triggerCallback = false);

	void SendConVarValue(CPlayerSlot slot, const char *cvar, const char *value);
	// These two functions require a valid server convar reference, unlike the first one when you can send arbitrary convars,
	// which can be useful for client-only convars with replicate flag.
	void SendConVarValue(CPlayerSlot slot, const ConVarRefAbstract conVarRef, const char *value);
	void SendConVarValue(CPlayerSlot slot, const ConVarRefAbstract conVarRef, const CVValue_t *value);

	void SendMultipleConVarValues(CPlayerSlot slot, const char **cvars, const char **values, u32 size);
	// These two functions require all valid convar references as well.
	void SendMultipleConVarValues(CPlayerSlot slot, ConVarRefAbstract **conVarRefs, const char **values, u32 size);
	void SendMultipleConVarValues(CPlayerSlot slot, ConVarRefAbstract **conVarRefs, const CVValue_t *values, u32 size);

	CBaseEntity *FindEntityByClassname(CEntityInstance *start, const char *name);

	CBasePlayerController *GetController(CBaseEntity *entity);
	CBasePlayerController *GetController(CPlayerSlot slot);

	CPlayerSlot GetEntityPlayerSlot(CBaseEntity *entity);

	// Normalize the angle between -180 and 180.
	f32 NormalizeDeg(f32 a);
	// Gets the difference in angle between 2 angles.
	// c can be PI (for radians) or 180.0 (for degrees);
	f32 GetAngleDifference(const f32 x, const f32 y, const f32 c, bool relative = false);

	// Print functions
	bool CFormat(char *buffer, u64 buffer_size, const char *text);
	void SayChat(CBaseEntity *entity, const char *format, ...);
	void ClientPrintFilter(IRecipientFilter *filter, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3,
						   const char *param4);
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

	// Color print
	void CPrintChat(CBaseEntity *entity, const char *format, ...);
	void CPrintChatAll(const char *format, ...);

	// Sounds
	void PlaySoundToClient(CPlayerSlot player, const char *sound, f32 volume = 1.0f);

	// Return true if the spawn found is truly valid (not in the ground or out of bounds)
	bool IsSpawnValid(const Vector &origin);
	bool FindValidSpawn(Vector &origin, QAngle &angles);

	// Return true if there's a direct line of sight from the box with specified bounds.
	bool CanSeeBox(Vector origin, Vector mins, Vector maxs);
	bool FindValidPositionAroundCenter(Vector center, Vector distFromCenter, Vector extraOffset, Vector &originDest, QAngle &anglesDest);

	// Return true if there is a valid position for the trigger.
	bool FindValidPositionForTrigger(CBaseTrigger *trigger, Vector &originDest, QAngle &anglesDest);

	void ResetMap();
	void ResetMapIfEmpty();

	bool IsServerSecure();
	void UpdateServerVersion();
	u32 GetServerVersion();

	inline bool IsNumeric(const char *str)
	{
		if (!str || !*str)
		{
			return false;
		}

		return str[strspn(str, "-0123456789.")] == 0;
	}

	bool ParseSteamID2(std::string_view steamID, u64 &out);

	inline u32 GetPaddingForWideString(const char *string)
	{
		return MAX(0, strlen(string) - mbstowcs(NULL, string, 0));
	}

	inline Vector TransformPoint(const CTransform &tm, const Vector &p)
	{
		return Vector(tm.m_vPosition.x
						  + (1.0f - 2.0f * tm.m_orientation.y * tm.m_orientation.y - 2.0f * tm.m_orientation.z * tm.m_orientation.z) * p.x
						  + (2.0f * tm.m_orientation.x * tm.m_orientation.y - 2.0f * tm.m_orientation.w * tm.m_orientation.z) * p.y
						  + (2.0f * tm.m_orientation.x * tm.m_orientation.z + 2.0f * tm.m_orientation.w * tm.m_orientation.y) * p.z,
					  tm.m_vPosition.y + (2.0f * tm.m_orientation.x * tm.m_orientation.y + 2.0f * tm.m_orientation.w * tm.m_orientation.z) * p.x
						  + (1.0f - 2.0f * tm.m_orientation.x * tm.m_orientation.x - 2.0f * tm.m_orientation.z * tm.m_orientation.z) * p.y
						  + (2.0f * tm.m_orientation.y * tm.m_orientation.z - 2.0f * tm.m_orientation.w * tm.m_orientation.x) * p.z,
					  tm.m_vPosition.z + (2.0f * tm.m_orientation.x * tm.m_orientation.z - 2.0f * tm.m_orientation.w * tm.m_orientation.y) * p.x
						  + (2.0f * tm.m_orientation.y * tm.m_orientation.z + 2.0f * tm.m_orientation.w * tm.m_orientation.x) * p.y
						  + (1.0f - 2.0f * tm.m_orientation.x * tm.m_orientation.x - 2.0f * tm.m_orientation.y * tm.m_orientation.y) * p.z);
	}

	template<typename T = CBaseEntity>
	T *CreateEntityByName(const char *className)
	{
		return reinterpret_cast<T *>(g_pKZUtils->CreateEntityByName(className, -1));
	}

	void InitItemAttributes();
	std::string GetItemAttributeName(u16 id);
	bool DoesPaintKitUseLegacyModel(float paintKit);
} // namespace utils
