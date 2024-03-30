#pragma once
#include "../kz.h"
#include "utils/httpmanager.h"
#include "steam/steam_api_common.h"
#include "steam/isteamhttp.h"
#include "utils/ctimer.h"

class KZGlobalService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static_global void Init();

private:
	static f64 Heartbeat();
	static void HeartbeatCallback(HTTPRequestHandle request, int statusCode, std::string response);
};
