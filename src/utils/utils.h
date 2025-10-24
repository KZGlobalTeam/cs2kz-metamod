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

	inline f64 StringToFloat(const char *str)
	{
		if (!str || *str == '\0')
		{
			return 0.0f;
		}

		// Handle special string values
		if (KZ_STREQI(str, "nan") || KZ_STREQI(str, "-nan"))
		{
			return FLOAT32_NAN;
		}

		if (KZ_STREQI(str, "inf"))
		{
			return FLT_MAX;
		}

		if (KZ_STREQI(str, "-inf"))
		{
			return -FLT_MAX;
		}

		// Use standard conversion
		return atof(str);
	}

	inline void FormatTime(f64 time, char *output, u32 length, bool precise = true)
	{
		i32 roundedTime = RoundFloatToInt(time * 1000); // Time rounded to number of ms

		i32 milliseconds = roundedTime % 1000;
		roundedTime = (roundedTime - milliseconds) / 1000;
		i32 seconds = roundedTime % 60;
		roundedTime = (roundedTime - seconds) / 60;
		i32 minutes = roundedTime % 60;
		roundedTime = (roundedTime - minutes) / 60;
		i32 hours = roundedTime;

		if (hours == 0)
		{
			if (precise)
			{
				snprintf(output, length, "%02i:%02i.%03i", minutes, seconds, milliseconds);
			}
			else
			{
				snprintf(output, length, "%i:%02i", minutes, seconds);
			}
		}
		else
		{
			if (precise)
			{
				snprintf(output, length, "%i:%02i:%02i.%03i", hours, minutes, seconds, milliseconds);
			}
			else
			{
				snprintf(output, length, "%i:%02i:%02i", hours, minutes, seconds);
			}
		}
	}

	inline CUtlString FormatTime(f64 time, bool precise = true)
	{
		char temp[32];
		FormatTime(time, temp, sizeof(temp), precise);
		return CUtlString(temp);
	}

	inline bool ParseTimeString(const char *timeStr, f64 *outSeconds)
	{
		if (!timeStr || !outSeconds)
		{
			return false;
		}

		// Skip leading whitespace
		while (*timeStr && isspace(*timeStr))
		{
			timeStr++;
		}

		if (*timeStr == '\0')
		{
			return false;
		}

		f64 parts[3] = {0.0, 0.0, 0.0};
		i32 partCount = 0;
		const char *ptr = timeStr;

		// Parse up to 3 colon-separated parts
		while (partCount < 3)
		{
			// Parse the integer part
			if (!isdigit(*ptr))
			{
				return false;
			}

			i32 intPart = 0;
			while (isdigit(*ptr))
			{
				intPart = intPart * 10 + (*ptr - '0');
				ptr++;
			}

			parts[partCount] = (float)intPart;

			// Check for decimal part
			if (*ptr == '.')
			{
				ptr++;
				if (!isdigit(*ptr))
				{
					return false;
				}

				f64 fracPart = 0.0;
				f64 divisor = 10.0;
				while (isdigit(*ptr))
				{
					fracPart += (*ptr - '0') / divisor;
					divisor *= 10.0;
					ptr++;
				}

				parts[partCount] += fracPart;
			}

			partCount++;

			// Check what comes next
			if (*ptr == ':')
			{
				ptr++;
				if (partCount >= 3)
				{
					return false; // Too many colons
				}
			}
			else
			{
				break; // End of parsing
			}
		}

		// Skip trailing whitespace
		while (*ptr && isspace(*ptr))
		{
			ptr++;
		}

		// Should be at end of string
		if (*ptr != '\0')
		{
			return false;
		}

		// Validate ranges and convert to seconds
		f64 totalSeconds = 0.0;

		if (partCount == 1)
		{
			// Just seconds: 12 or 12.5
			totalSeconds = parts[0];
		}
		else if (partCount == 2)
		{
			// Minutes:seconds: 12:34
			if (parts[1] >= 60.0f)
			{
				return false;
			}
			totalSeconds = parts[0] * 60.0f + parts[1];
		}
		else if (partCount == 3)
		{
			// Hours:minutes:seconds: 12:34:45
			if (parts[1] >= 60.0f || parts[2] >= 60.0f)
			{
				return false;
			}
			totalSeconds = parts[0] * 3600.0f + parts[1] * 60.0f + parts[2];
		}

		*outSeconds = totalSeconds;
		return true;
	}
} // namespace utils
