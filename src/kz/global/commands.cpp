#include "common.h"
#include "kz_global.h"
#include "kz/language/kz_language.h"
#include "utils/http.h"
#include "utils/simplecmds.h"

static_function SCMD_CALLBACK(Command_KzApiStatus)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	HTTP::Request request(HTTP::Method::GET, KZGlobalService::ApiURL());
	auto callback = [player](HTTP::Response response)
	{
		if (response.status == 200)
		{
			player->languageService->PrintChat(true, false, "API Healthy");
		}
		else
		{
			player->languageService->PrintChat(true, false, "API Unhealthy", response.status);
		}
	};

	request.Send(callback);

	return MRES_SUPERCEDE;
}

void KZGlobalService::RegisterCommands()
{
	scmd::RegisterCmd("kz_apistatus", Command_KzApiStatus);
}
