#include "kz_language.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "utils/ctimer.h"

internal KeyValues *translationKV;
internal KeyValues *languagesKV;

void KZLanguageService::Init()
{
	if (translationKV)
	{
		delete translationKV;
	}
	if (languagesKV)
	{
		delete languagesKV;
	}
	translationKV = new KeyValues("Phrases");
	translationKV->UsesEscapeSequences(true);
	languagesKV = new KeyValues("Languages");
	languagesKV->UsesEscapeSequences(true);

	KZLanguageService::LoadTranslations();
	KZLanguageService::LoadLanguages();
}

void KZLanguageService::LoadLanguages()
{
	char fullPath[1024];
	g_SMAPI->PathFormat(fullPath, sizeof(fullPath), "%s/addons/cs2kz/translations/config.txt", g_SMAPI->GetBaseDir());
	if (!languagesKV->LoadFromFile(g_pFullFileSystem, fullPath, nullptr))
	{
		META_CONPRINT("Failed to load translation config file.\n");
	}
}

void KZLanguageService::LoadTranslations()
{
	char buffer[1024];
	g_SMAPI->PathFormat(buffer, sizeof(buffer), "addons/cs2kz/translations/*.phrases.txt");
	FileFindHandle_t findHandle = {};
	const char *fileName = g_pFullFileSystem->FindFirst(buffer, &findHandle);
	if (fileName)
	{
		do
		{
			char fullPath[1024];
			g_SMAPI->PathFormat(fullPath, sizeof(fullPath), "%s/addons/cs2kz/translations/%s", g_SMAPI->GetBaseDir(), fileName);
			if (!translationKV->LoadFromFile(g_pFullFileSystem, fullPath, nullptr))
			{
				META_CONPRINTF("Failed to load %s\n", fileName);
			}
			fileName = g_pFullFileSystem->FindNext(findHandle);
		} while (fileName);
		g_pFullFileSystem->FindClose(findHandle);
	}
}

const char *KZLanguageService::GetLanguage()
{
	if (this->hasQueriedLanguage || this->hasSavedLanguage)
	{
		return this->language;
	}
	return KZ_DEFAULT_LANGUAGE;
}

const char *KZLanguageService::GetTranslatedFormat(const char *language, const char *phrase)
{
	if (!translationKV->FindKey(phrase))
	{
		return NULL;
	}
	const char *outFormat = translationKV->FindKey(phrase)->GetString(language);
	if (outFormat[0] == '\0')
	{
		META_CONPRINTF("Warning: Phrase '%s' not found for language %s!\n", phrase, language);
		return translationKV->FindKey(phrase)->GetString(KZ_DEFAULT_LANGUAGE);
	}
	return outFormat;
}
