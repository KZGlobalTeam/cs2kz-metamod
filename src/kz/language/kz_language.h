#pragma once
#include "vendor/tinyformat.h"

#include "../kz.h"
#include "../spec/kz_spec.h"

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
	std::string PrepareMessage(const char *message, Args &&...args)
	{
		const char *language = GetLanguage();
		const char *paramFormat = GetTranslatedFormat("#format", message);
		const char *msgFormat = GetTranslatedFormat(language, message);
		if (!paramFormat)
		{
			// Just return the raw unformatted message if format can't be found.
			return std::string(message);
		}
		return GetFormattedMessage(msgFormat, paramFormat, args...);
	}

	template<typename... Args>
	static_global void PrintConsoleSingle(KZPlayer *player, bool addPrefix, const char *message, Args &&...args)
	{
		std::string msg = player->languageService->PrepareMessage(message, args...);
		player->PrintConsole(addPrefix, false, msg.c_str());
	}

	template<typename... Args>
	void PrintConsole(bool addPrefix, bool includeSpectators, const char *message, Args &&...args)
	{
		PrintConsoleSingle(this->player, addPrefix, message, args...);
		if (includeSpectators)
		{
			for (KZPlayer *spec = this->player->specService->GetNextSpectator(NULL); spec != NULL;
				 spec = this->player->specService->GetNextSpectator(spec))
			{
				PrintConsoleSingle(spec, addPrefix, message, args...);
			}
		}
	}

	template<typename... Args>
	static_global void PrintConsoleAll(bool addPrefix, const char *message, Args &&...args)
	{
		for (u32 i = 0; i < MAXPLAYERS + 1; i++)
		{
			CBasePlayerController *controller = g_pKZPlayerManager->players[i]->GetController();
			if (controller)
			{
				PrintConsoleSingle(g_pKZPlayerManager->ToPlayer(i), addPrefix, message, args...);
			}
		}
	}

private:
	bool hasQueriedLanguage {};
	bool hasSavedLanguage {};
	char language[32] {};
};
