#include <string_view>

#include "common.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/option/kz_option.h"
#include "kz/style/kz_style.h"
#include "kz_global.h"
#include "utils/http.h"
#include "utils/simplecmds.h"

namespace
{
	std::string_view MakeStatusString(bool checkmark)
	{
		using namespace std::literals::string_view_literals;
		return checkmark ? "{green}✓{default}"sv : "{darkred}✗{default}"sv;
	}

	struct GlobalStatus
	{
		bool apiOnline = false;
		bool serverConnected = false;
		bool mapGlobal = false;
		bool playerGlobal = false;
		bool modeGlobal = false;
		bool stylesGlobal = false;

		void Report(KZPlayer *player) const
		{
			// clang-format off
			player->languageService->PrintChat(true, false, "Global Check",
					MakeStatusString(this->apiOnline),
					MakeStatusString(this->serverConnected),
					MakeStatusString(this->mapGlobal),
					MakeStatusString(this->playerGlobal),
					MakeStatusString(this->modeGlobal),
					MakeStatusString(this->stylesGlobal));
			// clang-format on
		}
	};
}; // namespace

SCMD(kz_globalcheck, SCFL_GLOBAL | SCFL_MAP | SCFL_PLAYER)
{
	GlobalStatus status;
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	if (KZGlobalService::IsAvailable())
	{
		status.apiOnline = true;
		status.serverConnected = true;
		status.mapGlobal = KZGlobalService::WithCurrentMap([](const std::optional<KZ::api::Map> &mapInfo) { return mapInfo.has_value(); });
		status.playerGlobal = !player->globalService->IsBanned();

		KZGlobalService::WithGlobalModes(
			[&](const KZGlobalService::GlobalModeChecksums &checksums)
			{
				const char *modeName = player->modeService->GetModeShortName();
				KZModeManager::ModePluginInfo modeInfo = KZ::mode::GetModeInfo(modeName);

				if (KZ_STREQ(modeName, "VNL"))
				{
					status.modeGlobal = KZ_STREQ(modeInfo.md5, checksums.vanilla.c_str());
				}
				else if (KZ_STREQ(modeName, "CKZ"))
				{
					status.modeGlobal = KZ_STREQ(modeInfo.md5, checksums.classic.c_str());
				}
			});

		KZGlobalService::WithGlobalStyles(
			[&](const std::vector<KZGlobalService::GlobalStyleChecksum> &checksums)
			{
				bool allValid = true;

				FOR_EACH_VEC(player->styleServices, i)
				{
					bool isValid = false;

					for (const KZGlobalService::GlobalStyleChecksum &checksum : checksums)
					{
						if (KZ_STREQ(player->styleServices[i]->GetStyleShortName(), checksum.style.c_str()))
						{
							auto styleInfo = KZ::style::GetStyleInfo(player->styleServices[i]);
							isValid = KZ_STREQ(styleInfo.md5, checksum.checksum.c_str());
							break;
						}
					}

					if (!isValid)
					{
						allValid = false;
						break;
					}
				}

				status.stylesGlobal = allValid;
			});

		status.Report(player);
	}
	else
	{
		HTTP::Request request(HTTP::Method::GET, KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org"));
		auto onResponse = [=](HTTP::Response response) mutable
		{
			status.apiOnline = (response.status == 200);

			if (player)
			{
				status.Report(player);
			}
		};

		request.Send(std::move(onResponse));
	}

	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_gc, kz_globalcheck);
