#pragma once
#include "vendor/tinyformat.h"

#include "../kz.h"
#include "../spec/kz_spec.h"

class KZLanguageService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static void Init();
	static void LoadLanguages();
	static void LoadTranslations();
	static void RegisterCommands();

	virtual void Reset() override
	{
		hasQueriedLanguage = {};
		hasSavedLanguage = {};
		language[0] = 0;
	}

	void SetLanguage(const char *lang)
	{
		hasSavedLanguage = true;
		V_strncpy(language, lang, sizeof(language));
	}

	const char *GetLanguage();

	static const char *GetTranslatedFormat(const char *language, const char *phrase);

private:
	static inline void ReplaceStringInPlace(std::string &subject, std::string_view search, std::string_view replace)
	{
		size_t pos = 0;
		while ((pos = subject.find(search, pos)) != std::string::npos)
		{
			subject.replace(pos, search.length(), replace);
			pos += replace.length();
		}
	}

	template<typename... Args>
	static std::string GetFormattedMessage(const char *input, const char *format, Args &&...args)
	{
		std::string inputStr = std::string(input);
		const char *tokenStart = format;
		int argNumber = 0;
		while (true)
		{
			const char *tokenEnd = strstr(tokenStart, ":");
			if (!tokenEnd)
			{
				break;
			}
			const char *replaceStart = tokenEnd + 1;
			const char *replaceEnd = strstr(replaceStart, ",");
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

public:
	template<typename... Args>
	static std::string PrepareMessageWithLang(const char *language, const char *message, Args &&...args)
	{
		const char *paramFormat = GetTranslatedFormat("#format", message);
		const char *msgFormat = GetTranslatedFormat(language, message);
		if (!paramFormat)
		{
			// Just return the raw unformatted message if format can't be found.
			return std::string(msgFormat);
		}
		return GetFormattedMessage(msgFormat, paramFormat, args...);
	}

	template<typename... Args>
	std::string PrepareMessage(const char *message, Args &&...args)
	{
		const char *language = GetLanguage();
		return KZLanguageService::PrepareMessageWithLang(language, message, args...);
	}

private:
	enum MessageType : u8
	{
		MESSAGE_CHAT,
		MESSAGE_CONSOLE,
		MESSAGE_CENTRE,
		MESSAGE_ALERT,
		MESSAGE_HTML
	};

	template<typename... Args>
	static void PrintType(KZPlayer *player, bool addPrefix, MessageType type, const char *message, Args &&...args)
	{
		const char *language = player->languageService->GetLanguage();
		std::string msg = PrepareMessageWithLang(language, message, args...);
		switch (type)
		{
			case MESSAGE_CHAT:
			{
				player->PrintChat(addPrefix, false, msg.c_str());
				return;
			}
			case MESSAGE_CONSOLE:
			{
				player->PrintConsole(addPrefix, false, msg.c_str());
				return;
			}
			case MESSAGE_CENTRE:
			{
				player->PrintCentre(addPrefix, false, msg.c_str());
				return;
			}
			case MESSAGE_ALERT:
			{
				player->PrintAlert(addPrefix, false, msg.c_str());
				return;
			}
			case MESSAGE_HTML:
			{
				player->PrintHTMLCentre(addPrefix, false, msg.c_str());
				return;
			}
		}
	}

	template<typename... Args>
	static void PrintSingle(KZPlayer *player, bool addPrefix, bool includeSpectators, MessageType type, const char *message, Args &&...args)
	{
		PrintType(player, addPrefix, type, message, args...);
		if (includeSpectators)
		{
			for (KZPlayer *spec = player->specService->GetNextSpectator(NULL); spec != NULL; spec = player->specService->GetNextSpectator(spec))
			{
				PrintType(spec, addPrefix, type, message, args...);
			}
		}
	}

public:
#define REGISTER_PRINT_SINGLE_FUNCTION(name, type) \
	template<typename... Args> \
	void name(bool addPrefix, bool includeSpectators, const char *message, Args &&...args) \
	{ \
		PrintSingle(this->player, addPrefix, includeSpectators, type, message, args...); \
	}

	// Note: Takes 8 '%' for every % printed for chat, 4 for console.
	REGISTER_PRINT_SINGLE_FUNCTION(PrintChat, MESSAGE_CHAT)
	REGISTER_PRINT_SINGLE_FUNCTION(PrintConsole, MESSAGE_CONSOLE)
	REGISTER_PRINT_SINGLE_FUNCTION(PrintCentre, MESSAGE_CENTRE)
	REGISTER_PRINT_SINGLE_FUNCTION(PrintAlert, MESSAGE_ALERT)
	REGISTER_PRINT_SINGLE_FUNCTION(PrintHTMLCentre, MESSAGE_HTML)
#undef REGISTER_PRINT_SINGLE_FUNCTION

#define REGISTER_PRINT_ALL_FUNCTION(name, type) \
	template<typename... Args> \
	static void name(bool addPrefix, const char *message, Args &&...args) \
	{ \
		for (u32 i = 0; i < MAXPLAYERS + 1; i++) \
		{ \
			CBasePlayerController *controller = g_pKZPlayerManager->players[i]->GetController(); \
			if (controller) \
			{ \
				PrintSingle(g_pKZPlayerManager->ToPlayer(i), addPrefix, false, type, message, args...); \
			} \
		} \
	}

	REGISTER_PRINT_ALL_FUNCTION(PrintChatAll, MESSAGE_CHAT)
	REGISTER_PRINT_ALL_FUNCTION(PrintConsoleAll, MESSAGE_CONSOLE)
	REGISTER_PRINT_ALL_FUNCTION(PrintCentreAll, MESSAGE_CENTRE)
	REGISTER_PRINT_ALL_FUNCTION(PrintAlertAll, MESSAGE_ALERT)
	REGISTER_PRINT_ALL_FUNCTION(PrintHTMLCentreAll, MESSAGE_HTML)
#undef REGISTER_PRINT_ALL_FUNCTION

private:
	bool hasQueriedLanguage {};
	bool hasSavedLanguage {};
	char language[32] {};
};
