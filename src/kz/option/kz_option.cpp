#include "kz_option.h"

KZServerConfig *g_pKZServerConfig;

void KZOptionService::Reset()
{
}

void KZOptionService::LoadDefaultOptions()
{

	if (!g_pKZServerConfig)
	{
		g_pKZServerConfig = new KZServerConfig();
	}

	char serverCfgPath[1024];
	V_snprintf(serverCfgPath, sizeof(serverCfgPath), "%s%s", g_SMAPI->GetBaseDir(), "/addons/cs2kz/server-config/cs2kz-server-config.txt");

	KeyValues *serverCfgKeyValues = new KeyValues("ServerConfig");
	serverCfgKeyValues->LoadFromFile(g_pFullFileSystem, serverCfgPath, nullptr);

	g_pKZServerConfig->defaultMode = serverCfgKeyValues->GetString("defaultMode");
	g_pKZServerConfig->defaultStyle = serverCfgKeyValues->GetString("defaultStyle");
	g_pKZServerConfig->defaultLanguage = serverCfgKeyValues->GetString("defaultLanguage");
}

void KZOptionService::InitOptions()
{
	LoadDefaultOptions();
}
