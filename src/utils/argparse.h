#pragma once
#include "common.h"
#include "keyvalues3.h"
#include <string>
#include <regex>

namespace utils
{
	// Return false if a non whitelisted key is found.
	inline bool ParseArgsToKV3(std::string input, KeyValues3 &kv, const char **wlKeys = NULL, u32 wlKeysLength = 0)
	{
		bool allValid = true;
		// Regex for "key=value"
		const static std::regex pattern(R"((\w+)=(.*?)(?=\s+\w+=|$))");
		auto words_begin = std::sregex_iterator(input.begin(), input.end(), pattern);
		auto words_end = std::sregex_iterator();

		for (std::sregex_iterator i = words_begin; i != words_end; i++)
		{
			std::smatch match = *i;
			std::string key = match[1];
			// Check if the key is in the whitelist.
			bool hasValidKey = wlKeysLength == 0;
			for (u32 i = 0; i < wlKeysLength; i++)
			{
				if (!V_stricmp(wlKeys[i], key.c_str()))
				{
					hasValidKey = true;
					break;
				}
			}
			if (!hasValidKey)
			{
				allValid = false;
			}

			std::string value = match[2];
			if (value.empty())
			{
				continue;
			}
			kv.FindOrCreateMember(key.c_str())->SetString(value.c_str());
		}
		return allValid;
	}
} // namespace utils
