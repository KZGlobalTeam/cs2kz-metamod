#pragma once
#include "common.h"
#include "random.h"
#include "vendor/tinyformat.h"

DLL_IMPORT void Plat_CreateUUIDImpl(void *uuid, int (*randomFunc)(int min, int max));

struct UUID_t
{
	u64 high {};
	u64 low {};

	UUID_t()
	{
		Init();
	}

	void Init()
	{
		Plat_CreateUUIDImpl(this, RandomInt);
	}

	bool operator==(const UUID_t &other) const
	{
		return low == other.low && high == other.high;
	}

	// Convert UUID to standard string format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
	std::string ToString() const
	{
		return tfm::format("%08x-%04x-%04x-%04x-%012x",
						   (u32)(high >> 32),         // First 4 bytes
						   (u16)(high >> 16),         // Next 2 bytes
						   (u16)(high),               // Next 2 bytes
						   (u16)(low >> 48),          // Next 2 bytes
						   low & 0x0000FFFFFFFFFFFF); // Last 6 bytes
	}

	// Check if a UUID string is valid format and parse it
	// Expected format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
	static bool IsValid(const char *uuid_str, UUID_t *out_uuid = nullptr)
	{
		if (!uuid_str)
		{
			return false;
		}

		// Check length (36 characters)
		size_t len = strlen(uuid_str);
		if (len != 36)
		{
			return false;
		}

		// Check hyphens are in the right positions
		if (uuid_str[8] != '-' || uuid_str[13] != '-' || uuid_str[18] != '-' || uuid_str[23] != '-')
		{
			return false;
		}

		// Check all other characters are valid hex digits
		for (int i = 0; i < 36; i++)
		{
			if (i == 8 || i == 13 || i == 18 || i == 23)
			{
				continue; // Skip hyphens
			}

			char c = uuid_str[i];
			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
			{
				return false;
			}
		}

		// If caller wants to parse the UUID, do it
		if (out_uuid)
		{
			u32 d1;
			u32 d2, d3; // Using u32 for scanf but will cast to u16
			u32 d4;     // Using u32 for scanf but will cast to u16
			u64 d5;

			int result = sscanf(uuid_str, "%08x-%04x-%04x-%04x-%012llx", &d1, &d2, &d3, &d4, &d5);

			if (result != 5)
			{
				return false;
			}

			// Reconstruct the high and low u64 values
			out_uuid->high = ((u64)d1 << 32) | ((u64)(u16)d2 << 16) | (u16)d3;
			out_uuid->low = ((u64)(u16)d4 << 48) | d5;
		}

		return true;
	}

	// Convenience overload for std::string
	static bool IsValid(const std::string &uuid_str, UUID_t *out_uuid = nullptr)
	{
		return IsValid(uuid_str.c_str(), out_uuid);
	}
};
