#include "kz_global.h"
#include "kz/language/kz_language.h"
#include "utils/simplecmds.h"

static_function SCMD_CALLBACK(Command_KzProfile)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	auto onSuccess = [player](std::optional<KZ::API::Player> info)
	{
		if (!info)
		{
			player->languageService->PrintChat(true, false, "Player not found");
			return;
		}

		const char *name = info->name.c_str();
		const char *steamID = info->steamID.c_str();
		const char *isBanned = info->isBanned ? info->isBanned.value() ? "" : "not " : "maybe ";

		player->languageService->PrintChat(true, false, "Display PlayerInfo", name, steamID, isBanned);
	};

	auto onError = [player](KZ::API::Error error)
	{
		player->globalService->ReportError(error);
	};

	const char *playerIdentifier = args->Arg(1);

	if (playerIdentifier[0] == '\0')
	{
		player->globalService->FetchPlayer(player->GetSteamId64(), onSuccess, onError);
	}
	else
	{
		player->globalService->FetchPlayer(playerIdentifier, onSuccess, onError);
	}

	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzMapInfo)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	const char *mapIdentifier = args->Arg(1);

	if (mapIdentifier[0] != '\0')
	{
		auto onSuccess = [player](std::optional<KZ::API::Map> map)
		{
			if (!map)
			{
				player->languageService->PrintChat(true, false, "MapNotGlobal");
				return;
			}

			player->globalService->DisplayMapInfo(map.value());
		};

		auto onError = [player](KZ::API::Error error)
		{
			player->globalService->ReportError(error);
		};

		player->globalService->FetchMap(mapIdentifier, onSuccess, onError);
	}
	else if (KZGlobalService::currentMap)
	{
		player->globalService->DisplayMapInfo(KZGlobalService::currentMap.value());
	}
	else
	{
		player->languageService->PrintChat(true, false, "MapNotGlobal");
	}

	return MRES_SUPERCEDE;
}

void KZGlobalService::RegisterCommands()
{
	scmd::RegisterCmd("kz_profile", Command_KzProfile);
	scmd::RegisterCmd("kz_mapinfo", Command_KzMapInfo);
	scmd::RegisterCmd("kz_minfo", Command_KzMapInfo);
}
