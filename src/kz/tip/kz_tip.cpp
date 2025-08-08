#include "kz_tip.h"
#include "kz/timer/kz_timer.h"
#include "kz/language/kz_language.h"

#include <vendor/MultiAddonManager/public/imultiaddonmanager.h>
#include <vendor/ClientCvarValue/public/iclientcvarvalue.h>

extern IClientCvarValue *g_pClientCvarValue;
static_global KeyValues *pTipKeyValues;
static_global CUtlVector<const char *> tipNames;
static_global f64 tipInterval;
static_global i32 nextTipIndex;
static_global CTimer<> *tipTimer;

static_global class KZTimerServiceEventListener_Tip : public KZTimerServiceEventListener
{
	virtual void OnTimerStartPost(KZPlayer *player, u32 courseGUID) override;
} timerEventListener;

extern IMultiAddonManager *g_pMultiAddonManager;

void KZTipService::Init()
{
	LoadTips();
	ShuffleTips();
	tipTimer = StartTimer(PrintTips, true);
	KZTimerService::RegisterEventListener(&timerEventListener);
}

void KZTipService::Reset()
{
	this->showTips = true;
	this->teamJoinedAtLeastOnce = false;
	this->timerStartedAtLeastOnce = false;
}

void KZTipService::ToggleTips()
{
	this->showTips = !this->showTips;
	player->languageService->PrintChat(true, false, this->showTips ? "Option - Tips - Enable" : "Option - Tips - Disable");
}

bool KZTipService::ShouldPrintTip()
{
	return this->showTips;
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
					META_CONPRINTF("Failed to load %s\n", fileName);
				}
			}
			fileName = g_pFullFileSystem->FindNext(findHandle);
		} while (fileName);
		g_pFullFileSystem->FindClose(findHandle);
	}

	FOR_EACH_SUBKEY(pTipKeyValues, it)
	{
		tipNames.AddToTail(it->GetName());
	}

	tipInterval = KZOptionService::GetOptionFloat("tipInterval", KZ_DEFAULT_TIP_INTERVAL);
	delete pTipKeyValues;
}

void KZTipService::ShuffleTips()
{
	for (int i = tipNames.Count() - 1; i > 0; --i)
	{
		int j = RandomInt(0, i);
		V_swap(tipNames.Element(i), tipNames.Element(j));
	}
}

SCMD(kz_tips, SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->tipService->ToggleTips();
	return MRES_SUPERCEDE;
}

f64 KZTipService::PrintTips()
{
	for (int i = 0; i <= MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (player->tipService->ShouldPrintTip())
		{
			player->tipService->PrintTip();
		}
	}
	nextTipIndex = (nextTipIndex + 1) % tipNames.Count();
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
	this->QueryBeamCvar();
}

void KZTipService::QueryBeamCvar()
{
	CPlayerUserId userID = this->player->GetClient()->GetUserID();
	if (g_pClientCvarValue)
	{
		// clang-format off
		g_pClientCvarValue->QueryCvarValue(this->player->GetPlayerSlot(), "spec_show_xray",
			[userID](CPlayerSlot nSlot, ECvarValueStatus eStatus, const char *pszCvarName, const char *pszCvarValue)
			{
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
				if (!player)
				{
					return;
				}
		});
		// clang-format on
	}
}

void KZTipService::OnTimerStartPost()
{
	if (this->timerStartedAtLeastOnce)
	{
		return;
	}

	this->teamJoinedAtLeastOnce = true;
	// TODO: Print no cheating stuff
}

void KZTimerServiceEventListener_Tip::OnTimerStartPost(KZPlayer *player, u32 courseGUID)
{
	player->tipService->OnTimerStartPost();
}
