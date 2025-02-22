#include <string_view>

#include "common.h"
#include "kz_global.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "utils/http.h"
#include "utils/simplecmds.h"

static_function std::string_view MakeStatusString(bool checkmark)
{
	using namespace std::literals::string_view_literals;
	return checkmark ? "{green}✓{default}"sv : "{darkred}✗{default}"sv;
}

static_function SCMD_CALLBACK(Command_KzGlobalCheck)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	if (KZGlobalService::IsAvailable())
	{
		bool apiStatus = true;
		bool serverStatus = true;
		bool mapStatus = KZGlobalService::WithCurrentMap([](const KZ::API::Map *currentMap) { return currentMap != nullptr; });
		bool playerStatus = !player->globalService->playerInfo.isBanned;

		// clang-format off
		bool modeStatus = KZGlobalService::WithGlobalModes([&](const auto& globalModes)
		{
			KZ::API::Mode mode;
			if (KZ::API::DecodeModeString(player->modeService->GetModeShortName(), mode))
			{
				KZModeManager::ModePluginInfo modeInfo = KZ::mode::GetModeInfo(player->modeService->GetModeShortName());

				for (const auto &globalMode : globalModes)
				{
					if (mode == globalMode.mode
								  && (KZ_STREQ(modeInfo.md5, globalMode.linuxChecksum.c_str()) || KZ_STREQ(modeInfo.md5, globalMode.windowsChecksum.c_str())))
					{
						return true;
					}
				}
			}

			return false;
		});
		// clang-format on

		// clang-format off
		bool styleStatus = KZGlobalService::WithGlobalStyles([&](const auto& globalStyles)
		{
			FOR_EACH_VEC(player->styleServices, i)
			{
				if (!styleStatus)
				{
					return false;
				}

				KZ::API::Style style;

				if (!KZ::API::DecodeStyleString(player->styleServices[i]->GetStyleShortName(), style))
				{
					return false;
				}

				auto styleInfo = KZ::style::GetStyleInfo(player->styleServices[i]);
				bool found = false;

				for (const auto &globalStyle : globalStyles)
				{
					if (style == globalStyle.style
						&& (KZ_STREQ(styleInfo.md5, globalStyle.linuxChecksum.c_str()) || KZ_STREQ(styleInfo.md5, globalStyle.windowsChecksum.c_str())))
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					return false;
				}
			}

			return true;
		});
		// clang-format on

		// clang-format off
		player->languageService->PrintChat(true, false, "Global Check",
				MakeStatusString(apiStatus),
				MakeStatusString(serverStatus),
				MakeStatusString(mapStatus),
				MakeStatusString(playerStatus),
				MakeStatusString(modeStatus),
				MakeStatusString(styleStatus));
		// clang-format on
	}
	else
	{
		HTTP::Request request(HTTP::Method::GET, "https://api.cs2kz.org");
		auto callback = [player](HTTP::Response response)
		{
			bool apiStatus = (response.status == 200);
			bool serverStatus = false;
			bool mapStatus = false;
			bool playerStatus = false;
			bool modeStatus = false;
			bool styleStatus = false;

			// clang-format off
			player->languageService->PrintChat(true, false, "Global Check",
					MakeStatusString(apiStatus),
					MakeStatusString(serverStatus),
					MakeStatusString(mapStatus),
					MakeStatusString(playerStatus),
					MakeStatusString(modeStatus),
					MakeStatusString(styleStatus));
			// clang-format on
		};
		request.Send(callback);
	}

	return MRES_SUPERCEDE;
}

void KZGlobalService::RegisterCommands()
{
	scmd::RegisterCmd("kz_globalcheck", Command_KzGlobalCheck);
	scmd::RegisterCmd("kz_gc", Command_KzGlobalCheck);
}
