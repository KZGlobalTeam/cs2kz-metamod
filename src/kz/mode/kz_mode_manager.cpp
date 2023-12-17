#include "kz_mode.h"
#include "kz_mode_vnl.h"
#include "interfaces/interfaces.h"

internal KZModeManager modeManager;

bool KZ::mode::InitModeCvars()
{
	bool success = true;
	for (u32 i = 0; i < numCvar; i++)
	{
		ConVarHandle cvarHandle = g_pCVar->FindConVar(KZ::mode::modeCvarNames[i]);
		if (!cvarHandle.IsValid())
		{
			META_CONPRINTF("Failed to find %s!\n", KZ::mode::modeCvarNames[i]);
			success = false;
		}
		modeCvars[i] = g_pCVar->GetConVar(cvarHandle);
	}
	return success;
}

void KZ::mode::InitModeManager()
{
	static bool initialized = false;
	if (initialized) return;
	ModeServiceFactory vnlFactory = [](KZPlayer *player) -> KZModeService *{ return new KZVanillaModeService(player); };
	modeManager.RegisterMode("VNL", "Vanilla", vnlFactory);
	initialized = true;
}

void KZ::mode::InitModeService(KZPlayer *player)
{
	player->modeService = new KZVanillaModeService(player);
}

void KZ::mode::DisableReplicatedModeCvars()
{
	for (u32 i = 0; i < numCvar; ++i)
	{
		assert(modeCvars[i]);
		modeCvars[i]->flags &= ~FCVAR_REPLICATED;
	}
}

void KZ::mode::EnableReplicatedModeCvars()
{
	for (u32 i = 0; i < numCvar; ++i)
	{
		assert(modeCvars[i]);
		modeCvars[i]->flags |= FCVAR_REPLICATED;
	}
}

KZModeManager *KZ::mode::GetKZModeManager()
{
	return &modeManager;
}

bool KZModeManager::RegisterMode(const char *shortModeName, const char *longModeName, ModeServiceFactory factory)
{
	if ((shortModeName && this->nameIDMap.Defined(shortModeName)) || (longModeName && this->nameIDMap.Defined(longModeName)))
	{
		return false;
	}

	this->IDFactoryMap.Insert(this->currentID, factory);
	if (shortModeName)
	{
		this->nameIDMap[shortModeName] = this->currentID;
	}
	if (longModeName)
	{
		this->nameIDMap[longModeName] = this->currentID;
	}
	this->currentID++;
	return true;
}

void KZModeManager::UnregisterMode(const char *modeName)
{
	if (!modeName || !this->nameIDMap.Defined(modeName)) return;

	int modeID = this->nameIDMap[modeName];
	if (modeID == 0)
	{
		return; // Special case for VNL (0)
	}
	for (int i = 0; i < this->nameIDMap.GetNumStrings(); i++)
	{
		if (this->nameIDMap[i] == modeID)
		{
			this->nameIDMap[i] = UTL_INVAL_SYMBOL;
		}
	}
	this->IDFactoryMap.Remove(modeID);
}

bool KZModeManager::SwitchToMode(KZPlayer *player, const char *modeName)
{
	if (!modeName || !this->nameIDMap.Defined(modeName)) return false;

	int modeID = this->nameIDMap[modeName];
	ModeServiceFactory factory = this->IDFactoryMap[modeID];
	delete player->modeService;
	player->modeService = factory(player);
	return true;
}