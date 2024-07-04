#include "kz_mode.h"
#include "kz_mode_vnl.h"

#include "filesystem.h"

#include "utils/utils.h"
#include "interfaces/interfaces.h"

#include "../timer/kz_timer.h"
#include "../language/kz_language.h"
#include "utils/simplecmds.h"
#include "utils/plat.h"

static_function SCMD_CALLBACK(Command_KzModeShort);
static_function SCMD_CALLBACK(Command_KzMode);

static_global KZModeManager modeManager;
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
	static_persist bool initialized = false;
	if (initialized)
	{
		return;
	}
	ModeServiceFactory vnlFactory = [](KZPlayer *player) -> KZModeService * { return new KZVanillaModeService(player); };
	modeManager.RegisterMode(0, "VNL", "Vanilla", vnlFactory);
	initialized = true;
}

void KZ::mode::LoadModePlugins()
{
	char buffer[1024];
	g_SMAPI->PathFormat(buffer, sizeof(buffer), "addons/cs2kz/modes/*%s", MODULE_EXT);
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
		auto value = reinterpret_cast<CVValue_t *>(&(modeCvars[i]->values));
		if (modeCvars[i]->m_eVarType == EConVarType_Float32)
		{
			f32 newValue = atof(player->modeService->GetModeConVarValues()[i]);
			value->m_flValue = newValue;
		}
		else
		{
			i32 newValue;
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
			value->m_i32Value = newValue;
		}
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

	V_snprintf(shortModeCmd, 64, "kz_%s", shortModeName);
	bool shortCmdRegistered = scmd::RegisterCmd(V_strlower(shortModeCmd), Command_KzModeShort);
	this->modeInfos.AddToTail({id, shortModeName, longModeName, factory, shortCmdRegistered});
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

bool KZModeManager::SwitchToMode(KZPlayer *player, const char *modeName, bool silent, bool force)
{
	// Don't change mode if it doesn't exist. Instead, print a list of modes to the client.
	if (!modeName || !V_stricmp("", modeName))
	{
		player->languageService->PrintChat(true, false, "Mode Command Usage");
		player->languageService->PrintConsole(false, false, "Possible & Current Modes", player->modeService->GetModeName());
		FOR_EACH_VEC(this->modeInfos, i)
		{
			// clang-format off
			player->PrintConsole(false, false,
				"%s (kz_mode %s / kz_mode %s)",
				this->modeInfos[i].longModeName,
				this->modeInfos[i].longModeName,
				this->modeInfos[i].shortModeName
			);
			// clang-format on
		}
		return false;
	}

	// If it's the same style, do nothing, unless it's forced.
	if (!force && (V_stricmp(player->modeService->GetModeName(), modeName) == 0 || V_stricmp(player->modeService->GetModeShortName(), modeName) == 0))
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
		if (!silent)
		{
			player->languageService->PrintChat(true, false, "Mode Not Available", modeName);
		}
		return false;
	}
	player->modeService->Cleanup();
	delete player->modeService;
	player->modeService = factory(player);
	player->timerService->TimerStop();
	player->modeService->Init();

	if (!silent)
	{
		player->languageService->PrintChat(true, false, "Switched Mode", player->modeService->GetModeName());
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
		if (this->modeInfos[i].id == 0)
		{
			continue;
		}
		pluginManager->Unload(this->modeInfos[i].id, true, error, sizeof(error));
	}
}

static_function SCMD_CALLBACK(Command_KzMode)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	modeManager.SwitchToMode(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzModeShort)
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
	scmd::RegisterCmd("kz_mode", Command_KzMode);
}
