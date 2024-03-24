#include "kz_option.h"

internal KeyValues *pServerCfgKeyValues;

void KZOptionService::LoadDefaultOptions()
{
	char serverCfgPath[1024];
	V_snprintf(serverCfgPath, sizeof(serverCfgPath), "%s%s", g_SMAPI->GetBaseDir(), "/addons/cs2kz/server-config/cs2kz-server-config.txt");

	pServerCfgKeyValues = new KeyValues("ServerConfig");
	pServerCfgKeyValues->LoadFromFile(g_pFullFileSystem, serverCfgPath, nullptr);
}			

const char *KZOptionService::GetDefaultOptionStr(const char *optionName, const char *defaultValue)
{
	return pServerCfgKeyValues->GetString(optionName, defaultValue);
}

f64 KZOptionService::GetDefaultOptionFloat(const char *optionName, f64 defaultValue)
{
	return pServerCfgKeyValues->GetFloat(optionName, defaultValue);
}

i64 KZOptionService::GetDefaultOptionInt(const char *optionName, i64 defaultValue)
{
	return pServerCfgKeyValues->GetInt(optionName, defaultValue);
}

void KZOptionService::InitOptions()
{
	LoadDefaultOptions();
}
