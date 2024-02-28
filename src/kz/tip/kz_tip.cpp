#include "kz_tip.h"

#define KZ_DEFAULT_TIP_INTERVAL 1.0f

internal KeyValues *pKeyValues;
internal CUtlVector<const char *> tipNames;
internal const char *tipPaths[4];
internal float tipInterval;

void KZTipService::Reset()
{
	this->showTips = true;
	this->tipCounter = 0;
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
	utils::CPrintChat(player->GetController(), "%s %s", KZ_CHAT_PREFIX, pKeyValues->GetString(tipNames[this->tipCounter]));
	this->tipCounter = (this->tipCounter >= tipNames.Count() - 1) ? 0 : (this->tipCounter + 1);
}

void KZTipService::LoadTips()
{
	pKeyValues = new KeyValues("Tips");
	tipPaths[0] = "addons/cs2kz/gamedata/cs2kz-tips-general.txt";
	tipPaths[1] = "addons/cs2kz/gamedata/cs2kz-tips-jumpstats.txt";
	tipPaths[2] = "addons/cs2kz/gamedata/cs2kz-tips-visual.txt";
	tipPaths[3] = "addons/cs2kz/gamedata/cs2kz-tips-config.txt";
	for (int i = 0; i < 4; i++)
	{
		if (!pKeyValues->LoadFromFile(g_pFullFileSystem, tipPaths[i], nullptr))
		{
			META_CONPRINTF("Failed to load %s, no tips loaded.\n", tipPaths[i]);
			return;
		}
	}

	KeyValues* removedKeyValues = pKeyValues->FindKey("Remove");
	CUtlVector<const char*> removedTipNames;
	FOR_EACH_SUBKEY(removedKeyValues, it)
	{
		removedTipNames.AddToTail(it->GetName());
	}

	FOR_EACH_SUBKEY(pKeyValues, it)
	{
		if (!removedTipNames.HasElement(it->GetName()) && 
			V_strcmp(it->GetName(), "Remove") != 0 && 
			V_strcmp(it->GetName(), "Insert") != 0 &&
			V_strcmp(it->GetName(), "Settings") != 0)
		{
			tipNames.AddToTail(it->GetName());
		}
	}

	KeyValues* insertedKeyValues = pKeyValues->FindKey("Insert");
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

	KeyValues* settingsKeyValues = pKeyValues->FindKey("Settings");
	tipInterval = settingsKeyValues->GetFloat("interval");
}

void KZTipService::InitTips()
{
	LoadTips();

	META_CONPRINTF("Interval: %0.2f\n", tipInterval);
	for (int i = 0; i < tipNames.Count(); i++)
	{
		META_CONPRINTF("Tip: %s\n", pKeyValues->GetString(tipNames[i]));
	}
	int n = tipNames.Count();
	for (int i = n - 1; i > 0; --i)
	{
		int j = RandomInt(0, i);
		V_swap(tipNames.Element(i), tipNames.Element(j));
	}

	new CTimer(tipInterval, true, []() {
		KZTipService::PrintTips();
		return tipInterval;
		});
}
	
void KZTipService::PrintTips() 
{
	for (int i = 0; i <= MAXPLAYERS; i++)
	{
		KZPlayer* player = g_pKZPlayerManager->ToPlayer(i);
		if (player->tipService->ShouldPrintTip())
		{
			player->tipService->PrintTip();
		}
	}
}
