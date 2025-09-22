#include <string_view>

#include "common.h"
#include "kz_global.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/option/kz_option.h"
#include "utils/http.h"
#include "utils/simplecmds.h"

static_function std::string_view MakeStatusString(bool checkmark)
{
	using namespace std::literals::string_view_literals;
	return checkmark ? "{green}✓{default}"sv : "{darkred}✗{default}"sv;
}

SCMD(kz_globalcheck, SCFL_GLOBAL | SCFL_MAP | SCFL_PLAYER)
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
#ifdef _WIN32
					const std::string& checksum = globalMode.windowsChecksum;
#else
					const std::string& checksum = globalMode.linuxChecksum;
#endif

					if (mode == globalMode.mode && KZ_STREQ(modeInfo.md5, checksum.c_str()))
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
#ifdef _WIN32
					const std::string& checksum = globalStyle.windowsChecksum;
#else
					const std::string& checksum = globalStyle.linuxChecksum;
#endif

					if (style == globalStyle.style && KZ_STREQ(styleInfo.md5, checksum.c_str()))
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
		HTTP::Request request(HTTP::Method::GET, KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org"));
		auto callback = [player](HTTP::Response response)
		{
			std::string_view apiStatus = MakeStatusString(response.status == 200);
			std::string_view serverStatus = MakeStatusString(false);
			std::string_view mapStatus = MakeStatusString(false);
			std::string_view playerStatus = MakeStatusString(false);
			std::string_view modeStatus = MakeStatusString(false);
			std::string_view styleStatus = MakeStatusString(false);

			if (KZGlobalService::MayBecomeAvailable())
			{
				mapStatus = "{yellow}?";
				playerStatus = "{yellow}?";
				modeStatus = "{yellow}?";
				styleStatus = "{yellow}?";
			}

			// clang-format off
			player->languageService->PrintChat(true, false, "Global Check",
					apiStatus,
					serverStatus,
					mapStatus,
					playerStatus,
					modeStatus,
					styleStatus);
			// clang-format on
		};
		request.Send(callback);
	}

	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_gc, kz_globalcheck);
