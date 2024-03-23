#include "kz_option.h"

KZServerConfig *g_pKZServerConfig = new KZServerConfig();

void KZOptionService::Reset()
{
}

void KZOptionService::LoadDefaultOptions()
{

	char serverCfgPath[1024];
	V_snprintf(serverCfgPath, sizeof(serverCfgPath), "%s%s", g_SMAPI->GetBaseDir(), "/addons/cs2kz/server-config/cs2kz-server-config.txt");

	KeyValues *serverCfgKeyValues = new KeyValues("ServerConfig");
	serverCfgKeyValues->LoadFromFile(g_pFullFileSystem, serverCfgPath, nullptr);

	g_pKZServerConfig->defaultMode = serverCfgKeyValues->GetString("defaultMode", "CKZ");
	g_pKZServerConfig->defaultStyle = serverCfgKeyValues->GetString("defaultStyle", "NRM");
	g_pKZServerConfig->defaultLanguage = serverCfgKeyValues->GetString("defaultLanguage", "en");
}

void KZOptionService::InitOptions()
{
	LoadDefaultOptions();
}
