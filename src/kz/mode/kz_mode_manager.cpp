#include "kz_mode.h"
#include "kz_mode_vnl.h"

#include "filesystem.h"

#include "utils/utils.h"
#include "interfaces/interfaces.h"

#include "utils/simplecmds.h"
internal SCMD_CALLBACK(Command_KzModeShort);
internal SCMD_CALLBACK(Command_KzMode);

internal KZModeManager modeManager;
KZModeManager *g_pKZModeManager = &modeManager;

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
	if (initialized)
	{
		return;
	}
	ModeServiceFactory vnlFactory = [](KZPlayer *player) -> KZModeService *{ return new KZVanillaModeService(player); };
	modeManager.RegisterMode(0, "VNL", "Vanilla", vnlFactory);
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
		if (ret == META_IFACE_FAILED)
		{
			return;
		}
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
	delete player->modeService;
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

void KZ::mode::ApplyModeSettings(KZPlayer *player)
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
	player->enableWaterFix = player->modeService->EnableWaterFix();
}


bool KZModeManager::RegisterMode(PluginId id, const char *shortModeName, const char *longModeName, ModeServiceFactory factory)
{
	if (!shortModeName || V_strlen(shortModeName) == 0 || !longModeName || V_strlen(longModeName) == 0)
	{
		return false;
	}
	FOR_EACH_VEC(this->modeInfos, i)
	{
		if (V_stricmp(this->modeInfos[i].shortModeName, shortModeName) == 0)
		{
			return false;
		}
		if (V_stricmp(this->modeInfos[i].longModeName, longModeName) == 0)
		{
			return false;
		}
	}

	char shortModeCmd[64];
	char shortModeCmdDesc[256];
	
	V_snprintf(shortModeCmd, 64, "kz_%s", shortModeName);
	V_snprintf(shortModeCmdDesc, 64, "Switch to %s mode.", longModeName);
	bool shortCmdRegistered = scmd::RegisterCmd(V_strlower(shortModeCmd), Command_KzModeShort, shortModeCmdDesc);
	this->modeInfos.AddToTail({ id, shortModeName, longModeName, factory, shortCmdRegistered });
	return true;
}

void KZModeManager::UnregisterMode(const char *modeName)
{
	if (!modeName)
	{
		return;
	}

	// Cannot unregister VNL.
	if (V_stricmp("VNL", modeName) == 0 || V_stricmp("Vanilla", modeName) == 0)
	{
		return;
	}

	FOR_EACH_VEC(this->modeInfos, i)
	{
		if (V_stricmp(this->modeInfos[i].shortModeName, modeName) == 0 || V_stricmp(this->modeInfos[i].longModeName, modeName) == 0)
		{
			char shortModeCmd[64];
			V_snprintf(shortModeCmd, 64, "kz_%s", this->modeInfos[i].shortModeName);
			scmd::UnregisterCmd(shortModeCmd);
			this->modeInfos.Remove(i);
			break;
		}
	}

	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (strcmp(player->modeService->GetModeName(), modeName) == 0 || strcmp(player->modeService->GetModeShortName(), modeName) == 0)
		{
			this->SwitchToMode(player, "VNL");
		}
	}
}

bool KZModeManager::SwitchToMode(KZPlayer *player, const char *modeName, bool silent)
{
	// Don't change mode if it doesn't exist. Instead, print a list of modes to the client.
	if (!modeName || !V_stricmp("", modeName))
	{
		utils::CPrintChat(player->GetController(), "%s {grey}Usage: {default}kz_mode <mode>.{grey} Check console for possible modes!", KZ_CHAT_PREFIX);
		utils::PrintConsole(player->GetController(), "Possible modes:");
		FOR_EACH_VEC(this->modeInfos, i)
		{
			utils::PrintConsole(player->GetController(), "%s (kz_mode %s / kz_mode %s)", this->modeInfos[i].longModeName, this->modeInfos[i].longModeName, this->modeInfos[i].shortModeName);
		}
		return false;
	}

	// If it's the same mode, do nothing.
	if (V_stricmp(player->modeService->GetModeName(), modeName) == 0 || V_stricmp(player->modeService->GetModeShortName(), modeName) == 0)
	{
		return false;
	}

	ModeServiceFactory factory = nullptr;

	FOR_EACH_VEC(this->modeInfos, i)
	{
		if (V_stricmp(this->modeInfos[i].shortModeName, modeName) == 0 || V_stricmp(this->modeInfos[i].longModeName, modeName) == 0)
		{
			factory = this->modeInfos[i].factory;
			break;
		}
	}
	if (!factory)
	{

		if (player->GetController() && !silent)
		{
			utils::CPrintChat(player->GetController(), "%s {grey}The {purple}%s {grey}mode is not available.", KZ_CHAT_PREFIX, modeName);
		}
		return false;
	}
	delete player->modeService;
	player->modeService = factory(player);
	player->InvalidateTimer();
	
	if (player->GetController() && !silent)
	{
		utils::CPrintChat(player->GetController(), "%s {grey}You have switched to the {purple}%s {grey}mode.", KZ_CHAT_PREFIX, player->modeService->GetModeName());
	}

	utils::SendMultipleConVarValues(player->GetPlayerSlot(), KZ::mode::modeCvars, player->modeService->GetModeConVarValues(), KZ::mode::numCvar);
	return true;
}

void KZModeManager::Cleanup()
{
	int ret;
	ISmmPluginManager *pluginManager = (ISmmPluginManager *)g_SMAPI->MetaFactory(MMIFACE_PLMANAGER, &ret, 0);
	if (ret == META_IFACE_FAILED)
	{
		return;
	}
	char error[256];
	FOR_EACH_VEC(this->modeInfos, i)
	{
		if (this->modeInfos[i].id == 0) continue;
		pluginManager->Unload(this->modeInfos[i].id, true, error, sizeof(error));
	}
}

internal SCMD_CALLBACK(Command_KzMode)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	modeManager.SwitchToMode(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzModeShort)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	// Strip kz_ prefix if exist.
	size_t len = 0;
	bool stripped = false;
	if (!stripped)
	{
		len = strlen("kz_");
		stripped = strncmp(args->Arg(0), "kz_", len) == 0;
	}
	if (!stripped)
	{
		len = 1;
		stripped = args->Arg(0)[0] == SCMD_CHAT_TRIGGER || args->Arg(0)[0] == SCMD_CHAT_SILENT_TRIGGER;
	}
	
	if (stripped)
	{
		const char *mode = args->Arg(0) + len;
		modeManager.SwitchToMode(player, mode);
	}
	return MRES_SUPERCEDE;
}

void KZ::mode::RegisterCommands()
{
	scmd::RegisterCmd("kz_mode", Command_KzMode, "List or change mode.");
}
