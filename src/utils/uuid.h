#pragma once

#include "common.h"
#include "random.h"
#include "vendor/tinyformat.h"
#include <chrono>
#include <random>

struct UUID_t
{
	u8 bytes[16] {};

	UUID_t(bool init = true)
	{
		if (init)
		{
			Init();
		}
	}

	void Init()
	{
		// Generate all random bytes first
		std::random_device rd;
		for (int i = 0; i < 16; i++)
		{
			bytes[i] = (u8)(rd() & 0xFF);
		}

		// Get timestamp in milliseconds
		auto now = std::chrono::system_clock::now();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		// Overwrite first 48 bits with timestamp
		bytes[0] = (millis >> 40) & 0xFF;
		bytes[1] = (millis >> 32) & 0xFF;
		bytes[2] = (millis >> 24) & 0xFF;
		bytes[3] = (millis >> 16) & 0xFF;
		bytes[4] = (millis >> 8) & 0xFF;
		bytes[5] = millis & 0xFF;

		// Set version 7 (4 bits at bytes[6] high nibble)
		bytes[6] = (bytes[6] & 0x0F) | 0x70;

		// Set variant 10 (2 bits at bytes[8] high 2 bits)
		bytes[8] = (bytes[8] & 0x3F) | 0x80;
	}

	bool operator==(const UUID_t &other) const
	{
		return memcmp(bytes, other.bytes, 16) == 0;
	}

	bool operator<(const UUID_t &other) const
	{
		return memcmp(bytes, other.bytes, 16) < 0;
	}

	// Convert UUID to standard string format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
	std::string ToString() const
	{
		// clang-format off
        return tfm::format("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                           bytes[0], bytes[1], bytes[2], bytes[3],
                           bytes[4], bytes[5],
                           bytes[6], bytes[7],
                           bytes[8], bytes[9],
                           bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
		// clang-format on
	}

	// Extract timestamp from UUID v7 (returns milliseconds since Unix epoch)
	u64 GetTimestamp() const
	{
		// clang-format off
        u64 timestamp = ((u64)bytes[0] << 40) |
                        ((u64)bytes[1] << 32) |
                        ((u64)bytes[2] << 24) |
                        ((u64)bytes[3] << 16) |
                        ((u64)bytes[4] << 8) |
                        bytes[5];
		// clang-format on
		return timestamp;
	}

	// Check if this is a valid v7 UUID
	bool IsV7() const
	{
		u8 version = (bytes[6] >> 4) & 0x0F;
		u8 variant = (bytes[8] >> 6) & 0x03;
		return version == 7 && variant == 2; // variant 10 binary = 2 decimal
	}

	// Validate and optionally parse a UUID string
	static bool FromString(const char *uuid_str, UUID_t *out_uuid = nullptr)
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
			u32 parts[16];
			int result = sscanf(uuid_str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", &parts[0], &parts[1], &parts[2],
								&parts[3], &parts[4], &parts[5], &parts[6], &parts[7], &parts[8], &parts[9], &parts[10], &parts[11], &parts[12],
								&parts[13], &parts[14], &parts[15]);

			if (result != 16)
			{
				return false;
			}

			for (int i = 0; i < 16; ++i)
			{
				out_uuid->bytes[i] = (u8)parts[i];
			}
		}

		return true;
	}

	static bool IsValid(const std::string &uuid_str, UUID_t *out_uuid = nullptr)
	{
		return FromString(uuid_str.c_str(), out_uuid);
	}
};

// Hash specialization for std::unordered_map support
namespace std
{
	template<>
	struct hash<UUID_t>
	{
		std::size_t operator()(const UUID_t &uuid) const noexcept
		{
			std::size_t h = 0;
			for (int i = 0; i < 16; ++i)
			{
				h ^= std::hash<u8> {}(uuid.bytes[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
			}
			return h;
		}
	};
} // namespace std
