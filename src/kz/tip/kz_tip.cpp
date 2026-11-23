#include "kz_tip.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/timer/kz_timer.h"
#include "kz/language/kz_language.h"
#include "kz/global/kz_global.h"
#include "kz/tip/hud/welcome.h"
#include "utils/http.h"
#include "utils/json.h"

#include <vendor/MultiAddonManager/public/imultiaddonmanager.h>

static_global KeyValues *pTipKeyValues;
static_global std::vector<const char *> tipNames;
static_global f64 tipInterval;
static_global i32 nextTipIndex;
static_global CTimer<> *tipTimer;

extern IMultiAddonManager *g_pMultiAddonManager;

void KZTipService::Init()
{
	LoadTips();
	ShuffleTips();
	tipTimer = StartTimer(PrintTips, true);
}

void KZTipService::Reset()
{
	this->teamJoinedAtLeastOnce = false;
	this->welcomePending = false;
}

void KZTipService::ToggleTips()
{
	bool showTips = !this->player->optionService->GetPreferenceBool("showTips", true);
	this->player->optionService->SetPreferenceBool("showTips", showTips);
	player->languageService->PrintChat(true, false, showTips ? "Option - Tips - Enable" : "Option - Tips - Disable");
}

bool KZTipService::ShouldPrintTip()
{
	return this->player->optionService->GetPreferenceBool("showTips", true);
}

void KZTipService::PrintTip()
{
	this->player->languageService->PrintChat(true, false, tipNames[nextTipIndex]);
}

void KZTipService::LoadTips()
{
	pTipKeyValues = new KeyValues("Tips");
	pTipKeyValues->UsesEscapeSequences(true);

	char buffer[1024];
	g_SMAPI->PathFormat(buffer, sizeof(buffer), "addons/cs2kz/translations/*.*");
	FileFindHandle_t findHandle = {};
	const char *fileName = g_pFullFileSystem->FindFirst(buffer, &findHandle);
	if (fileName)
	{
		do
		{
			char fullPath[1024];
			g_SMAPI->PathFormat(fullPath, sizeof(fullPath), "%s/addons/cs2kz/translations/%s", g_SMAPI->GetBaseDir(), fileName);
			if (V_strstr(fileName, "cs2kz-tips-"))
			{
				if (!pTipKeyValues->LoadFromFile(g_pFullFileSystem, fullPath, nullptr))
				{
					KZ_LOG_WARN(LogChannel::Tip, "Failed to load %s\n", fileName);
				}
			}
			fileName = g_pFullFileSystem->FindNext(findHandle);
		} while (fileName);
		g_pFullFileSystem->FindClose(findHandle);
	}

	FOR_EACH_SUBKEY(pTipKeyValues, it)
	{
		tipNames.push_back(it->GetName());
	}

	tipInterval = KZOptionService::GetOptionFloat("tipInterval", KZ_DEFAULT_TIP_INTERVAL);
	delete pTipKeyValues;
}

void KZTipService::ShuffleTips()
{
	for (size_t i = tipNames.size(); i > 1; i--)
	{
		int j = RandomInt(0, i - 1);
		V_swap(tipNames[i - 1], tipNames[j]);
	}
}

SCMD(kz_tips, SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->tipService->ToggleTips();
	return true;
}

bool KZTipService::ShowWelcome()
{
	return this->player->uiService->GetWindow<KZ::tip::hud::WelcomeWindow>()->Show();
}

void KZTipService::OnLocalRunsChecked(bool hasRuns)
{
	if (hasRuns || this->player->optionService->GetPreferenceBool("welcomeSeen", false))
	{
		return;
	}
	if (!KZGlobalService::IsAvailable())
	{
		this->welcomePending = true;
		this->ShowPendingWelcome();
		return;
	}
	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	HTTP::Request request(HTTP::Method::GET, KZ::api::GetBaseUrl() + "/players/" + std::to_string(this->player->GetSteamId64()));
	request.Send(
		[userID](HTTP::Response response)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			if (!player)
			{
				return;
			}
			// The API doesn't know players who have never played on a global server.
			// Any other failure leaves the window for the next connection.
			bool isNew = response.status == 404;
			if (response.status == 200)
			{
				Json json(response.Body().value_or(""));
				f64 ckzRating = 0;
				f64 vnlRating = 0;
				json.Get("ckz_rating", ckzRating);
				json.Get("vnl_rating", vnlRating);
				isNew = ckzRating <= 0 && vnlRating <= 0;
			}
			if (isNew)
			{
				player->tipService->welcomePending = true;
				player->tipService->ShowPendingWelcome();
			}
		});
}

void KZTipService::ShowPendingWelcome()
{
	if (!this->welcomePending || !this->teamJoinedAtLeastOnce)
	{
		return;
	}
	this->welcomePending = false;
	// Check again because the global preferences can arrive after the database query.
	// They may show the player already saw the window on another server.
	if (this->player->optionService->GetPreferenceBool("welcomeSeen", false))
	{
		return;
	}
	if (this->ShowWelcome())
	{
		this->player->optionService->SetPreferenceBool("welcomeSeen", true);
	}
}

SCMD(kz_tutorial, SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player->tipService->ShowWelcome())
	{
		player->languageService->PrintChat(true, false, "Welcome - Unavailable");
	}
	return true;
}

f64 KZTipService::PrintTips()
{
	// Nothing to print without loaded tip translation files.
	if (tipNames.empty())
	{
		return tipInterval;
	}
	for (KZPlayer *player : g_pKZPlayerManager->GetInGamePlayers())
	{
		if (player->tipService->ShouldPrintTip())
		{
			player->tipService->PrintTip();
		}
	}
	nextTipIndex = (nextTipIndex + 1) % tipNames.size();
	return tipInterval;
}

void KZTipService::OnPlayerJoinTeam(i32 team)
{
	if (this->teamJoinedAtLeastOnce || (team != CS_TEAM_CT && team != CS_TEAM_T))
	{
		return;
	}

	this->teamJoinedAtLeastOnce = true;
	if (g_pMultiAddonManager)
	{
		this->player->languageService->PrintChat(true, false, "Menu Hint");
	}

	this->player->globalService->PrintAnnouncements();
	this->ShowPendingWelcome();

	StartTimer<CPlayerUserId>(KZAnticheatService::PrintWarning, this->player->GetClient()->GetUserID(), 1.0f, false, false);
}
