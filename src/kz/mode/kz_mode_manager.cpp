#include "kz_mode.h"
#include "kz_mode_vnl.h"

#include "filesystem.h"

#include "utils/utils.h"
#include "interfaces/interfaces.h"

#include "utils/simplecmds.h"

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
		modeCvarHandles[i] = cvarHandle;
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

void KZ::mode::LoadModePlugins()
{
	char buffer[1024];
	g_SMAPI->PathFormat(buffer, sizeof(buffer), "addons/cs2kz/modes/*.*");
	FileFindHandle_t findHandle = {};
	const char *output = g_pFullFileSystem->FindFirstEx(buffer, "GAME", &findHandle);
	if (output)
	{
		int ret;
		ISmmPluginManager *pluginManager = (ISmmPluginManager *)g_SMAPI->MetaFactory(MMIFACE_PLMANAGER, &ret, 0);
		if (ret == META_IFACE_FAILED) return;
		char error[256];
		char fullPath[1024];
		do
		{
			g_SMAPI->PathFormat(fullPath, sizeof(fullPath), "%s/addons/cs2kz/modes/%s", g_SMAPI->GetBaseDir(), output);
			bool already = false;
			pluginManager->Load(fullPath, g_PLID, already, error, sizeof(error));
			output = g_pFullFileSystem->FindNext(findHandle);
		} while (output);

		g_pFullFileSystem->FindClose(findHandle);
	}
}

void KZ::mode::InitModeService(KZPlayer *player)
{
	player->modeService = new KZVanillaModeService(player);
}

void KZ::mode::DisableReplicatedModeCvars()
{
	for (u32 i = 0; i < numCvar; i++)
	{
		assert(modeCvars[i]);
		modeCvars[i]->flags &= ~FCVAR_REPLICATED;
	}
}

void KZ::mode::EnableReplicatedModeCvars()
{
	for (u32 i = 0; i < numCvar; i++)
	{
		assert(modeCvars[i]);
		modeCvars[i]->flags |= FCVAR_REPLICATED;
	}
}

void KZ::mode::ApplyModeCvarValues(KZPlayer *player)
{
	for (u32 i = 0; i < numCvar; i++)
	{
		u8 byteArray[16] = {};
		if (modeCvars[i]->m_eVarType == EConVarType_Float32)
		{
			f32 newValue = atof(player->modeService->GetModeConVarValues()[i]);
			V_memcpy(byteArray, &newValue, sizeof(f32));
		}
		else 
		{
			u32 newValue;
			if (V_stricmp(player->modeService->GetModeConVarValues()[i], "true") == 0)
			{
				newValue = 1;
			}
			else if (V_stricmp(player->modeService->GetModeConVarValues()[i], "false") == 0)
			{
				newValue = 0;
			}
			else
			{
				newValue = atoi(player->modeService->GetModeConVarValues()[i]);
			} 
			V_memcpy(byteArray, &newValue, sizeof(u32));
		}
		V_memcpy(&(modeCvars[i]->values), byteArray, 16);
		//g_pCVar->SetConVarValue(modeCvarHandles[i], 0, (CVValue_t *)player->modeService->GetModeConVarValues()[i], (CVValue_t *)modeCvars[i]->values);
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
	
	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(i);
		if (strcmp(player->modeService->GetModeName(), modeName) == 0 || strcmp(player->modeService->GetModeShortName(), modeName) == 0)
		{
			this->SwitchToMode(player, "VNL");
		}
	}
}

bool KZModeManager::SwitchToMode(KZPlayer *player, const char *modeName, bool silent)
{
	// Don't change mode if it doesn't exist.
	if (!modeName || !this->nameIDMap.Defined(modeName)) return false;

	// If it's the same mode, do nothing.
	if (strcmp(player->modeService->GetModeName(), modeName) == 0 || strcmp(player->modeService->GetModeShortName(), modeName) == 0) return false;

	int modeID = this->nameIDMap[modeName];
	ModeServiceFactory factory = this->IDFactoryMap[modeID];
	delete player->modeService;
	player->modeService = factory(player);

	if (player->GetController() && !silent)
	{
		utils::CPrintChat(player->GetController(), "%s {grey}You have switched to the {purple}%s {grey}mode.", KZ_CHAT_PREFIX, player->modeService->GetModeName());
	}

	utils::SendMultipleConVarValues(player->GetPlayerSlot(), KZ::mode::modeCvars, player->modeService->GetModeConVarValues(), KZ::mode::numCvar);
	return true;
}

internal SCMD_CALLBACK(Command_KzMode)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	KZ::mode::GetKZModeManager()->SwitchToMode(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

void KZ::mode::RegisterCommands()
{
	scmd::RegisterCmd("kz_mode", Command_KzMode);
}