#include "kz_language.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "utils/ctimer.h"

#include <vendor/ClientCvarValue/public/iclientcvarvalue.h>

extern IClientCvarValue *g_pClientCvarValue;

static_global KeyValues *translationKV;
static_global KeyValues *languagesKV;

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
	const char *lang = "";
	if (this->hasQueriedLanguage || this->hasSavedLanguage)
	{
		lang = this->language;
	}
	else if (g_pClientCvarValue)
	{
		lang = g_pClientCvarValue->GetClientLanguage(this->player->GetPlayerSlot());
	}

	if (lang && lang[0] != '\0')
	{
		return (languagesKV->GetString(lang), lang);
	}
	return KZ_DEFAULT_LANGUAGE;
}

const char *KZLanguageService::GetTranslatedFormat(const char *language, const char *phrase)
{
	if (!translationKV->FindKey(phrase))
	{
		// META_CONPRINTF("Warning: Phrase '%s' not found, returning orignal message!\n", phrase);
		return phrase;
	}
	const char *outFormat = translationKV->FindKey(phrase)->GetString(language);
	if (outFormat[0] == '\0')
	{
		if (!V_stricmp(language, "#format"))
		{
			// It is fine to have no format.
			return NULL;
		}
		// META_CONPRINTF("Warning: Phrase '%s' not found for language %s!\n", phrase, language);
		return translationKV->FindKey(phrase)->GetString(KZ_DEFAULT_LANGUAGE);
	}
	return outFormat;
}

static_function SCMD_CALLBACK(Command_KzSetLanguage)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	char language[32] {};
	V_snprintf(language, sizeof(language), "%s", args->Arg(1));
	V_strlower(language);
	player->languageService->SetLanguage(language);
	player->languageService->PrintChat(true, false, "Switch Language", language);

	return MRES_SUPERCEDE;
}

void KZLanguageService::RegisterCommands()
{
	scmd::RegisterCmd("kz_language", Command_KzSetLanguage);
}
