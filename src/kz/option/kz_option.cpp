#include "kz_option.h"
#include "kz/db/kz_db.h"
static_global KeyValues *pServerCfgKeyValues;

CUtlVector<KZOptionServiceEventListener *> KZOptionService::eventListeners;

bool KZOptionService::RegisterEventListener(KZOptionServiceEventListener *eventListener)
{
	if (eventListeners.Find(eventListener) >= 0)
	{
		return false;
	}
	eventListeners.AddToTail(eventListener);
	return true;
}

bool KZOptionService::UnregisterEventListener(KZOptionServiceEventListener *eventListener)
{
	return eventListeners.FindAndRemove(eventListener);
}

void KZOptionService::LoadDefaultOptions()
{
	char serverCfgPath[1024];
	V_snprintf(serverCfgPath, sizeof(serverCfgPath), "%s%s", g_SMAPI->GetBaseDir(), "/cfg/cs2kz-server-config.txt");

	pServerCfgKeyValues = new KeyValues("ServerConfig");
	pServerCfgKeyValues->LoadFromFile(g_pFullFileSystem, serverCfgPath, nullptr);
}

const char *KZOptionService::GetOptionStr(const char *optionName, const char *defaultValue)
{
	return pServerCfgKeyValues->GetString(optionName, defaultValue);
}

f64 KZOptionService::GetOptionFloat(const char *optionName, f64 defaultValue)
{
	return pServerCfgKeyValues->GetFloat(optionName, defaultValue);
}

i64 KZOptionService::GetOptionInt(const char *optionName, i64 defaultValue)
{
	return pServerCfgKeyValues->GetInt(optionName, defaultValue);
}

KeyValues *KZOptionService::GetOptionKV(const char *optionName)
{
	return pServerCfgKeyValues->FindKey(optionName);
}

void KZOptionService::InitOptions()
{
	LoadDefaultOptions();
}

void KZOptionService::InitializeLocalPrefs(CUtlString text)
{
	if (this->initState > LOCAL)
	{
		return;
	}
	if (text.IsEmpty())
	{
		text = "{\n}";
	}
	CUtlString error;
	LoadKV3FromJSON(&this->prefKV, &error, text.Get(), "");
	if (!error.IsEmpty())
	{
		META_CONPRINTF("[KZ::DB] Error fetching local preference: %s\n", error.Get());
		return;
	}
	this->initState = LOCAL;

	CALL_FORWARD(eventListeners, OnPlayerPreferencesLoaded, this->player);
}

void KZOptionService::SaveLocalPrefs()
{
	CUtlString error, output;
	SaveKV3AsJSON(&this->prefKV, &error, &output);
	if (!error.IsEmpty())
	{
		META_CONPRINTF("[KZ::DB] Error saving local preference: %s\n", error.Get());
		return;
	}
	this->player->databaseService->SavePrefs(output);
}
