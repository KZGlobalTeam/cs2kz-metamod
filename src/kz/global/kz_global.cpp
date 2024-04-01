#include "kz_global.h"
#include "../kz.h"

extern ISteamHTTP *g_http;
CSteamGameServerAPIContext g_steamAPI;

f64 KZGlobalService::Heartbeat()
{
	if (!g_http)
	{
		g_steamAPI.Init();
		g_http = g_steamAPI.SteamHTTP();
	}
	g_HTTPManager.GET("https://staging.cs2.kz/", &HeartbeatCallback);

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
