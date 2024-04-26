#include "kz_tip.h"
#include "../language/kz_language.h"

internal KeyValues *pTipKeyValues;
internal CUtlVector<const char *> tipNames;
internal f64 tipInterval;
internal i32 nextTipIndex;
internal CTimer<> *tipTimer;

void KZTipService::Reset()
{
	this->showTips = true;
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
	KeyValues *configKeyValues = new KeyValues("Config");
	pTipKeyValues->UsesEscapeSequences(true);
	configKeyValues->UsesEscapeSequences(true);

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
}

void KZTipService::ShuffleTips()
{
	for (int i = tipNames.Count() - 1; i > 0; --i)
	{
		int j = RandomInt(0, i);
		V_swap(tipNames.Element(i), tipNames.Element(j));
	}
}

internal SCMD_CALLBACK(Command_KzToggleTips)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->tipService->ToggleTips();
	return MRES_SUPERCEDE;
}

void KZTipService::InitTips()
{
	scmd::RegisterCmd("kz_tips", Command_KzToggleTips, "Toggle tips.");
	LoadTips();
	ShuffleTips();
	tipTimer = StartTimer(PrintTips, true);
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
