#include "kz_tip.h"

#define KZ_DEFAULT_TIP_INTERVAL 75.0f

internal KeyValues *pTipKeyValues;
internal CUtlVector<const char *> tipNames;
internal const char *tipPaths[4];
internal float tipInterval;
internal i32 nextTipIndex;

void KZTipService::Reset()
{
	this->showTips = true; 
}

void KZTipService::ToggleTips()
{
	this->showTips = !this->showTips;
	player->PrintChat(true, "%s", this->showTips ? "Tips enabled." : "Tips disabled.");
}

bool KZTipService::ShouldPrintTip()
{
	return this->showTips;
}

void KZTipService::PrintTip()
{
	player->PrintChat(true, "%s", pTipKeyValues->GetString(tipNames[nextTipIndex]));
}

void KZTipService::LoadTips()
{
	tipPaths[0] = "addons/cs2kz/tips/config.txt";
	tipPaths[1] = "addons/cs2kz/tips/general-tips.txt";
	tipPaths[2] = "addons/cs2kz/tips/jumpstat-tips.txt";
	tipPaths[3] = "addons/cs2kz/tips/visual-tips.txt";

	pTipKeyValues = new KeyValues("Tips");
	KeyValues *configKeyValues = new KeyValues("Config");
	configKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[0], nullptr);
	if (!pTipKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[1], nullptr)
		|| !pTipKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[2], nullptr)
		|| !pTipKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[3], nullptr))
	{
		META_CONPRINTF("Failed to load tips.\n");
		return;
	}

	KeyValues *removedKeyValues = configKeyValues->FindKey("Remove", true);
	CUtlVector<const char *> removedTipNames;
	FOR_EACH_SUBKEY(removedKeyValues, it)
	{
		removedTipNames.AddToTail(it->GetName());
	}

	FOR_EACH_SUBKEY(pTipKeyValues, it)
	{
		if (!removedTipNames.HasElement(it->GetName()))
		{
			tipNames.AddToTail(it->GetName());
		}
	}

	KeyValues *insertedKeyValues = configKeyValues->FindKey("Insert", true);
	FOR_EACH_SUBKEY(insertedKeyValues, it)
	{
		const char *insertedValue = insertedKeyValues->GetString(it->GetName(), "");
		const char *currentValue = pTipKeyValues->GetString(it->GetName(), "");

		if (V_strcmp(currentValue, "") != 0)
		{
			pTipKeyValues->SetString(it->GetName(), insertedValue);
		}
		else
		{
			pTipKeyValues->AddString(it->GetName(), insertedValue);
			tipNames.AddToTail(it->GetName());
		}
	}

	tipInterval = configKeyValues->FindKey("Settings", true)->GetFloat("interval");
}


void KZTipService::InitTips()
{
	LoadTips();
	for (int i = tipNames.Count() - 1; i > 0; --i)
	{
		int j = RandomInt(0, i);
		V_swap(tipNames.Element(i), tipNames.Element(j));
	}
	StartTimer(true, PrintTips);
}
	
float KZTipService::PrintTips() 
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
