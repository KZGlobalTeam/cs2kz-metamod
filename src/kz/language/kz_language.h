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

	template<typename... Args>
	void PrintConsole(const char *message, Args... args)
	{
		const char *language = GetLanguage();
		const char *format = GetTranslatedFormat(language, message);
		std::string buffer = tfm::format(format, std::forward<Args>(args)...);
		META_CONPRINTF("%s\n", buffer.c_str());
	}

private:
	bool hasQueriedLanguage {};
	bool hasSavedLanguage {};
	char language[32] {};
};
