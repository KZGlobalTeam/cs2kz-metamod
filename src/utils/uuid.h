#pragma once
#include "common.h"
#include "random.h"
#include "vendor/tinyformat.h"

DLL_IMPORT void Plat_CreateUUIDImpl(void *uuid, int (*randomFunc)(int min, int max));

struct UUID_t
{
	u32 Data1 {};
	u16 Data2 {};
	u16 Data3 {};
	u8 Data4[8] {};

	UUID_t(bool init = true)
	{
		if (init)
		{
			Init();
		}
	}

	void Init()
	{
		Plat_CreateUUIDImpl(this, RandomInt);
	}

	bool operator==(const UUID_t &other) const
	{
		return Data1 == other.Data1 && Data2 == other.Data2 && Data3 == other.Data3 && memcmp(Data4, other.Data4, sizeof(Data4)) == 0;
	}

	// Convert UUID to standard string format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
	std::string ToString() const
	{
		return tfm::format("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", Data1, Data2, Data3, Data4[0], Data4[1], Data4[2], Data4[3], Data4[4],
						   Data4[5], Data4[6], Data4[7]);
	}

	// Validate and optionally parse a UUID string
	static bool IsValid(const char *uuid_str, UUID_t *out_uuid = nullptr)
	{
		if (!uuid_str)
		{
			return false;
		}

		if (strlen(uuid_str) != 36)
		{
			return false;
		}

		// Check hyphen positions
		if (uuid_str[8] != '-' || uuid_str[13] != '-' || uuid_str[18] != '-' || uuid_str[23] != '-')
		{
			return false;
		}

		// Validate hex digits
		for (int i = 0; i < 36; ++i)
		{
			if (i == 8 || i == 13 || i == 18 || i == 23)
			{
				continue;
			}
			char c = uuid_str[i];
			if (!isxdigit((unsigned char)c))
			{
				return false;
			}
		}

		if (out_uuid)
		{
			u32 d1, d2, d3;
			u32 d4[8];

			int result = sscanf(uuid_str, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", &d1, &d2, &d3, &d4[0], &d4[1], &d4[2], &d4[3], &d4[4],
								&d4[5], &d4[6], &d4[7]);

			if (result != 11)
			{
				return false;
			}

			out_uuid->Data1 = d1;
			out_uuid->Data2 = static_cast<u16>(d2);
			out_uuid->Data3 = static_cast<u16>(d3);
			for (int i = 0; i < 8; ++i)
			{
				out_uuid->Data4[i] = static_cast<u8>(d4[i]);
			}
		}

		return true;
	}

	static bool IsValid(const std::string &uuid_str, UUID_t *out_uuid = nullptr)
	{
		return IsValid(uuid_str.c_str(), out_uuid);
	}
};
