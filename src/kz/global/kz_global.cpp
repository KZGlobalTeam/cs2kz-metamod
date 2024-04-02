#include "kz_global.h"

extern ISteamHTTP *g_http;
CSteamGameServerAPIContext g_steamAPI;

#define KZ_API_BASE_URL "https://staging.cs2.kz/"

f64 KZGlobalService::Heartbeat()
{
	if (!g_http)
	{
		g_steamAPI.Init();
		g_http = g_steamAPI.SteamHTTP();
	}
	g_HTTPManager.GET(KZ_API_BASE_URL, &HeartbeatCallback);

	return 5.0f;
}

void KZGlobalService::HeartbeatCallback(HTTPRequestHandle request, int statusCode, std::string response)
{
	if (statusCode < 200 || statusCode > 299)
	{
		// API unavailable
	}
}

void KZGlobalService::Init()
{
	StartTimer(Heartbeat, true, true);
}
