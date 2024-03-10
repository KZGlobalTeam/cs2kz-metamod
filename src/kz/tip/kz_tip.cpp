#include "kz_tip.h"

internal KeyValues *pTipKeyValues;
internal CUtlVector<const char *> tipNames;
internal float tipInterval;
internal i32 nextTipIndex;
internal CTimer<> *tipTimer;

void KZTipService::Reset()
{
	this->showTips = true; 
}

void KZTipService::ToggleTips()
{
	this->showTips = !this->showTips;
	player->PrintChat(true, false, "{grey}%s", this->showTips ? "You will now see random tips in chat periodically." : "You will no longer see random tips in chat periodically.");
}

bool KZTipService::ShouldPrintTip()
{
	return this->showTips;
}

void KZTipService::PrintTip()
{
	player->PrintChat(true, false, "%s", pTipKeyValues->FindKey(tipNames[nextTipIndex])->GetString(this->language));
}

void KZTipService::LoadTips()
{
	pTipKeyValues = new KeyValues("Tips");
	KeyValues *configKeyValues = new KeyValues("Config");

	char buffer[1024];
	g_SMAPI->PathFormat(buffer, sizeof(buffer), "addons/cs2kz/tips/*.*");
	FileFindHandle_t findHandle = {};
	const char *output = g_pFullFileSystem->FindFirst(buffer, &findHandle);
	if (output)
	{
		do
		{
			char fullPath[1024];
			g_SMAPI->PathFormat(fullPath, sizeof(fullPath), "%s/addons/cs2kz/tips/%s", g_SMAPI->GetBaseDir(), output);
			if (V_stricmp(output, "config.txt") == 0)
			{
				if (!configKeyValues->LoadFromFile(g_pFullFileSystem, fullPath, nullptr))
				{
					META_CONPRINTF("Failed to load tips config file\n", output);
				}
			}
			else 
			{
				if (!pTipKeyValues->LoadFromFile(g_pFullFileSystem, fullPath, nullptr))
				{
					META_CONPRINTF("Failed to load %s\n", output);
				}
			}
			output = g_pFullFileSystem->FindNext(findHandle);
		} while (output);
		g_pFullFileSystem->FindClose(findHandle);
	}

	KeyValues *removedKeyValues = configKeyValues->FindKey("Remove", true);
	CUtlVector<const char *> removedTipNames;
	FOR_EACH_SUBKEY(removedKeyValues, i)
	{
		removedTipNames.AddToTail(i->GetName());
	}

	FOR_EACH_SUBKEY(pTipKeyValues, i)
	{
		if (!removedTipNames.HasElement(i->GetName()))
		{
			tipNames.AddToTail(i->GetName());
		}
	}

	KeyValues *insertedKeyValues = configKeyValues->FindKey("Insert", true);
	FOR_EACH_SUBKEY(insertedKeyValues, i)
	{
		pTipKeyValues->FindAndDeleteSubKey(i->GetName());
		pTipKeyValues->AddSubKey(i); 
	}

	tipInterval = configKeyValues->FindKey("Settings", true)->GetFloat("interval");
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
	for (int i = tipNames.Count() - 1; i > 0; --i)
	{
		int j = RandomInt(0, i);
		V_swap(tipNames.Element(i), tipNames.Element(j));
	}
	tipTimer = StartTimer(true, PrintTips);
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
