#pragma once
#include "vendor/tinyformat.h"

#include "../kz.h"

class KZLanguageService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static_global void Init();
	static_global void LoadLanguages();
	static_global void LoadTranslations();

	void SetLanguage(const char *lang);
	const char *GetLanguage();

	static_global const char *GetTranslatedFormat(const char *language, const char *phrase);

	inline void ReplaceStringInPlace(std::string &subject, const std::string &search, const std::string &replace)
	{
		size_t pos = 0;
		while ((pos = subject.find(search, pos)) != std::string::npos)
		{
			subject.replace(pos, search.length(), replace);
			pos += replace.length();
		}
	}

	template<typename... Args>
	std::string GetFormattedMessage(const char *input, const char *format, Args &&...args)
	{
		std::string inputStr = std::string(input);
		const char *tokenStart = format;
		const char *tokenEnd;
		const char *replaceStart;
		const char *replaceEnd;
		int argNumber = 0;
		while (true)
		{
			tokenEnd = strstr(tokenStart, ":");
			replaceStart = tokenEnd + 1;
			replaceEnd = strstr(replaceStart, ",");
			if (!replaceEnd)
			{
				replaceEnd = format + strlen(format);
				ReplaceStringInPlace(inputStr, '{' + std::string(tokenStart, tokenEnd - tokenStart) + '}',
									 '%' + std::to_string(++argNumber) + '$' + std::string(replaceStart, replaceEnd - replaceStart));
				break;
			}
			else
			{
				ReplaceStringInPlace(inputStr, '{' + std::string(tokenStart, tokenEnd - tokenStart) + '}',
									 '%' + std::to_string(++argNumber) + '$' + std::string(replaceStart, replaceEnd - replaceStart));
				tokenStart = replaceEnd + 1;
			}
		}
		return tfm::format(inputStr.c_str(), std::forward<Args>(args)...);
	}

	template<typename... Args>
	void PrintConsole(const char *message, Args &&...args)
	{
		const char *language = GetLanguage();
		const char *paramFormat = GetTranslatedFormat("#format", message);
		const char *msgFormat = GetTranslatedFormat(language, message);
		std::string buffer = GetFormattedMessage(msgFormat, paramFormat, args...);
		// TODO: colors/strip color off console
		META_CONPRINTF("%s\n", buffer.c_str());
	}

private:
	bool hasQueriedLanguage {};
	bool hasSavedLanguage {};
	char language[32] {};
};
