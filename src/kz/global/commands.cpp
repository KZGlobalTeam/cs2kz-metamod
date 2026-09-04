#include <string_view>

#include "common.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/option/kz_option.h"
#include "kz/style/kz_style.h"
#include "kz_global.h"
#include "utils/http.h"
#include "utils/interfaces.h"
#include "utils/simplecmds.h"
#include "utils/utils.h"

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

	void ReportModeToConsole(const char *modeName, const std::string &globalChecksum)
	{
		KZModeManager::ModePluginInfo modeInfo = KZ::mode::GetModeInfo(CUtlString(modeName));

		if (modeInfo.id < 0)
		{
			KZ_LOG_INFO(LogChannel::Global, "  Mode %s: not loaded\n", modeName);
		}
		else if (globalChecksum.empty())
		{
			KZ_LOG_INFO(LogChannel::Global, "  Mode %s: loaded, but the API did not report a checksum\n", modeName);
		}
		else if (KZ_STREQ(modeInfo.md5, globalChecksum.c_str()))
		{
			KZ_LOG_INFO(LogChannel::Global, "  Mode %s: global (%s)\n", modeName, modeInfo.md5);
		}
		else
		{
			// clang-format off
			KZ_LOG_INFO(LogChannel::Global, "  Mode %s: checksum mismatch (local: %s, global: %s)\n",
					modeName, modeInfo.md5, globalChecksum.c_str());
			// clang-format on
		}
	}

	void ReportStylesToConsole(const std::vector<KZGlobalService::GlobalStyleChecksum> &checksums)
	{
		if (checksums.empty())
		{
			KZ_LOG_INFO(LogChannel::Global, "  Styles: the API did not report any checksums\n");
			return;
		}

		for (const KZGlobalService::GlobalStyleChecksum &checksum : checksums)
		{
			KZStyleManager::StylePluginInfo styleInfo = KZ::style::GetStyleInfo(CUtlString(checksum.style.c_str()));

			if (styleInfo.id <= 0)
			{
				KZ_LOG_INFO(LogChannel::Global, "  Style %s: not loaded\n", checksum.style.c_str());
			}
			else if (KZ_STREQ(styleInfo.md5, checksum.checksum.c_str()))
			{
				KZ_LOG_INFO(LogChannel::Global, "  Style %s: global (%s)\n", checksum.style.c_str(), styleInfo.md5);
			}
			else
			{
				// clang-format off
				KZ_LOG_INFO(LogChannel::Global, "  Style %s: checksum mismatch (local: %s, global: %s)\n",
						checksum.style.c_str(), styleInfo.md5, checksum.checksum.c_str());
				// clang-format on
			}
		}
	}

	void ReportGlobalCheckToConsole(bool apiOnline, bool serverConnected)
	{
		KZ_LOG_INFO(LogChannel::Global, "Global check:\n");
		KZ_LOG_INFO(LogChannel::Global, "  API (%s): %s\n", KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org"),
					apiOnline ? "online" : "offline");
		KZ_LOG_INFO(LogChannel::Global, "  Server: %s\n", serverConnected ? "connected" : "not connected");

		CUtlString mapName = g_pKZUtils->GetCurrentMapName();

		KZGlobalService::WithCurrentMapState(
			[&](const std::optional<KZ::api::Map> &mapInfo, bool confirmed)
			{
				if (mapInfo.has_value())
				{
					// clang-format off
					KZ_LOG_INFO(LogChannel::Global, "  Map '%s': global (id: %i, courses: %i)\n",
							mapInfo->name.c_str(), mapInfo->id, (i32)mapInfo->courses.size());
					// clang-format on
				}
				else if (!confirmed && serverConnected)
				{
					KZ_LOG_INFO(LogChannel::Global, "  Map '%s': waiting for the API to respond\n", mapName.Get());
				}
				else
				{
					KZ_LOG_INFO(LogChannel::Global, "  Map '%s': not global\n", mapName.Get());
				}
			});

		KZGlobalService::WithGlobalModes(
			[](const KZGlobalService::GlobalModeChecksums &checksums)
			{
				ReportModeToConsole("VNL", checksums.vanilla);
				ReportModeToConsole("CKZ", checksums.classic);
			});

		KZGlobalService::WithGlobalStyles([](const std::vector<KZGlobalService::GlobalStyleChecksum> &checksums)
										  { ReportStylesToConsole(checksums); });
	}

	void RunGlobalCheckOnConsole()
	{
		if (KZGlobalService::IsAvailable())
		{
			ReportGlobalCheckToConsole(true, true);
			return;
		}

		HTTP::Request request(HTTP::Method::GET, KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org"));
		request.Send([](HTTP::Response response) { ReportGlobalCheckToConsole(response.status == 200, false); });
	}
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

CON_COMMAND_F(kz_globalcheck, "Print the global status of the server, the current map, and the loaded modes and styles.", FCVAR_GAMEDLL)
{
	RunGlobalCheckOnConsole();
}
