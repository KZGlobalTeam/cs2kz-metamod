#include "kz_tip.h"

#define KZ_DEFAULT_TIP_INTERVAL 75.0f

internal KeyValues *pKeyValues;
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
	utils::CPrintChat(player->GetController(), "%s Tips %s.", KZ_CHAT_PREFIX, this->showTips ? "enabled" : "disabled");
}

bool KZTipService::ShouldPrintTip()
{
	return this->showTips;
}

void KZTipService::PrintTip()
{
	utils::CPrintChat(player->GetController(), "%s %s", KZ_CHAT_PREFIX, pKeyValues->GetString(tipNames[nextTipIndex]));
}

void KZTipService::LoadTips()
{
	pKeyValues = new KeyValues("Tips");
	tipPaths[0] = "addons/cs2kz/tips/general-tips.txt";
	tipPaths[1] = "addons/cs2kz/tips/jumpstat-tips.txt";
	tipPaths[2] = "addons/cs2kz/tips/visual-tips.txt";
	tipPaths[3] = "addons/cs2kz/tips/config.txt";

	KeyValues* configKeyValues = new KeyValues("Config");
	if (!pKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[0], nullptr)
		|| !pKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[1], nullptr)
		|| !pKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[2], nullptr)
		|| !configKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[3], nullptr))
	{
		META_CONPRINTF("Failed to load tips.\n");
		return;
	}

	KeyValues* removedKeyValues = configKeyValues->FindKey("Remove", true);
	CUtlVector<const char*> removedTipNames;
	FOR_EACH_SUBKEY(removedKeyValues, it)
	{
		removedTipNames.AddToTail(it->GetName());
	}

	FOR_EACH_SUBKEY(pKeyValues, it)
	{
		if (!removedTipNames.HasElement(it->GetName()))
		{
			tipNames.AddToTail(it->GetName());
		}
	}

	KeyValues* insertedKeyValues = configKeyValues->FindKey("Insert", true);
	CUtlVector<const char*> insertedTipNames;
	FOR_EACH_SUBKEY(insertedKeyValues, it)
	{
		insertedTipNames.AddToTail(it->GetName());
	}

	for (int i = 0; i < insertedTipNames.Count(); i++)
	{
		if (V_strcmp(pKeyValues->GetString(insertedTipNames[i], ""), "") != 0)
		{
			pKeyValues->SetString(insertedTipNames[i], insertedKeyValues->GetString(insertedTipNames[i]));
		}
		else
		{
			pKeyValues->AddString(insertedTipNames[i], insertedKeyValues->GetString(insertedTipNames[i]));
			tipNames.AddToTail(insertedTipNames[i]);
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
