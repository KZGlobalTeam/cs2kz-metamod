#include "kz_tip.h"

internal KeyValues *pTipKeyValues;
internal CUtlVector<const char *> tipNames;
internal f64 tipInterval;
internal i32 nextTipIndex;
internal CTimer<> *tipTimer;

void KZTipService::Reset()
{
	this->showTips = true;
	this->language = KZOptionService::GetOptionStr("defaultLanguage", KZ_DEFAULT_LANGUAGE);
}

void KZTipService::ToggleTips()
{
	this->showTips = !this->showTips;

	// clang-format off

	player->PrintChat(true, false,
		"{grey}%s",
		this->showTips
			? "You will now see random tips in chat periodically."
			: "You will no longer see random tips in chat periodically."
	);

	// clang-format on
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
	pTipKeyValues->UsesEscapeSequences(true);
	configKeyValues->UsesEscapeSequences(true);

	char buffer[1024];
	g_SMAPI->PathFormat(buffer, sizeof(buffer), "addons/cs2kz/tips/*.*");
	FileFindHandle_t findHandle = {};
	const char *fileName = g_pFullFileSystem->FindFirst(buffer, &findHandle);
	if (fileName)
	{
		do
		{
			char fullPath[1024];
			g_SMAPI->PathFormat(fullPath, sizeof(fullPath), "%s/addons/cs2kz/tips/%s", g_SMAPI->GetBaseDir(), fileName);
			if (V_stricmp(fileName, "config.txt") == 0)
			{
				if (!configKeyValues->LoadFromFile(g_pFullFileSystem, fullPath, nullptr))
				{
					META_CONPRINTF("Failed to load tips config file\n", fileName);
				}
			}
			else
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

	CUtlVector<const char *> removedTipNames;
	KeyValues *removed = configKeyValues->FindKey("Remove", true);
	FOR_EACH_SUBKEY(removed, it)
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

	KeyValues *inserted = configKeyValues->FindKey("Insert", true);
	FOR_EACH_SUBKEY(inserted, it)
	{
		if (pTipKeyValues->FindKey(it->GetName()))
		{
			pTipKeyValues->SwapSubKey(pTipKeyValues->FindKey(it->GetName()), it->MakeCopy());
		}
		else
		{
			pTipKeyValues->AddSubKey(it->MakeCopy());
			tipNames.AddToTail(it->GetName());
		}
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
