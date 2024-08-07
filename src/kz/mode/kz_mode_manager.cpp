#include "kz_mode.h"
#include "kz_mode_vnl.h"

#include "filesystem.h"

#include "utils/utils.h"
#include "interfaces/interfaces.h"

#include "../timer/kz_timer.h"
#include "../language/kz_language.h"
#include "../db/kz_db.h"
#include "utils/simplecmds.h"
#include "utils/plat.h"

static_function SCMD_CALLBACK(Command_KzModeShort);
static_function SCMD_CALLBACK(Command_KzMode);

static_global KZModeManager modeManager;
KZModeManager *g_pKZModeManager = &modeManager;

CUtlVector<KZModeManager::ModePluginInfo> modeInfos;

static_global class KZDatabaseServiceEventListener_Modes : public KZDatabaseServiceEventListener
{
public:
	virtual void OnDatabaseSetup() override;
} databaseEventListener;

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
	KZDatabaseService::RegisterEventListener(&databaseEventListener);
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

void KZ::mode::UpdateModeDatabaseID(CUtlString name, i32 id, CUtlString shortName)
{
	// Check if the mode already exists in the list, if yes, we update it.
	FOR_EACH_VEC(modeInfos, i)
	{
		if (!V_stricmp(modeInfos[i].longModeName, name))
		{
			modeInfos[i].databaseID = id;
			if (!shortName.IsEmpty())
			{
				modeInfos[i].shortModeName = shortName;
			}
			return;
		}
	}
	// If the code reaches here, that means the mode is not in the list yet.
	modeInfos.AddToTail({-1, name.Get(), shortName.Get()});
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
	// Update the info list if already exists
	ModePluginInfo *info = nullptr;
	FOR_EACH_VEC(modeInfos, i)
	{
		if (!V_stricmp(modeInfos[i].shortModeName, shortModeName) || !V_stricmp(modeInfos[i].longModeName, longModeName))
		{
			if (modeInfos[i].id < 0)
			{
				info = &modeInfos[i];
				break;
			}
			return false;
		}
	}

	char shortModeCmd[64];

	V_snprintf(shortModeCmd, 64, "kz_%s", shortModeName);
	bool shortCmdRegistered = scmd::RegisterCmd(V_strlower(shortModeCmd), Command_KzModeShort);

	// Add to the list otherwise, and update the database for ID.
	if (!info)
	{
		info = modeInfos.AddToTailGetPtr();
		// If there is already information about this mode while the ID is -1, that means it has to come from the database, so no need to update it.
		KZDatabaseService::InsertAndUpdateModeIDs(longModeName, shortModeName);
	}
	*info = {id, shortModeName, longModeName, factory, shortCmdRegistered};
	if (id)
	{
		ISmmPluginManager *pluginManager = (ISmmPluginManager *)g_SMAPI->MetaFactory(MMIFACE_PLMANAGER, nullptr, nullptr);
		const char *path;
		pluginManager->Query(id, &path, nullptr, nullptr);
		g_pKZUtils->GetFileMD5(path, info->md5, sizeof(info->md5));
	}
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

	FOR_EACH_VEC(modeInfos, i)
	{
		if (V_stricmp(modeInfos[i].shortModeName, modeName) == 0 || V_stricmp(modeInfos[i].longModeName, modeName) == 0)
		{
			char shortModeCmd[64];
			V_snprintf(shortModeCmd, 64, "kz_%s", modeInfos[i].shortModeName.Get());
			scmd::UnregisterCmd(shortModeCmd);

			modeInfos[i].id = -1;
			modeInfos[i].md5[0] = 0;
			modeInfos[i].factory = nullptr;
			modeInfos[i].shortCmdRegistered = false;
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
		FOR_EACH_VEC(modeInfos, i)
		{
			if (modeInfos[i].id < 0)
			{
				continue;
			}
			// clang-format off
			player->PrintConsole(false, false,
				"%s (kz_mode %s / kz_mode %s)",
				modeInfos[i].longModeName.Get(),
				modeInfos[i].longModeName.Get(),
				modeInfos[i].shortModeName.Get()
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

	FOR_EACH_VEC(modeInfos, i)
	{
		if (V_stricmp(modeInfos[i].shortModeName, modeName) == 0 || V_stricmp(modeInfos[i].longModeName, modeName) == 0)
		{
			factory = modeInfos[i].factory;
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
	FOR_EACH_VEC(modeInfos, i)
	{
		if (modeInfos[i].id == 0)
		{
			continue;
		}
		pluginManager->Unload(modeInfos[i].id, true, error, sizeof(error));
	}
	// Restore cvars to normal values.
	for (u32 i = 0; i < KZ::mode::numCvar; i++)
	{
		auto value = reinterpret_cast<CVValue_t *>(&(KZ::mode::modeCvars[i]->values));
		auto defaultValue = KZ::mode::modeCvars[i]->m_cvvDefaultValue;
		if (KZ::mode::modeCvars[i]->m_eVarType == EConVarType_Float32)
		{
			value->m_flValue = defaultValue->m_flValue;
		}
		else
		{
			value->m_i64Value = defaultValue->m_i64Value;
		}
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

KZModeManager::ModePluginInfo KZ::mode::GetModeInfo(KZModeService *mode)
{
	KZModeManager::ModePluginInfo emptyInfo;
	if (!mode)
	{
		META_CONPRINTF("[KZ] Warning: Getting mode info from a nullptr!\n");
		return emptyInfo;
	}
	FOR_EACH_VEC(modeInfos, i)
	{
		if (!V_stricmp(mode->GetModeName(), modeInfos[i].longModeName))
		{
			return modeInfos[i];
		}
	}
	return emptyInfo;
}

KZModeManager::ModePluginInfo KZ::mode::GetModeInfo(CUtlString modeName)
{
	KZModeManager::ModePluginInfo emptyInfo;
	if (modeName.IsEmpty())
	{
		META_CONPRINTF("[KZ] Warning: Getting mode info from an empty string!\n");
		return emptyInfo;
	}
	FOR_EACH_VEC(modeInfos, i)
	{
		if (modeName.IsEqual_FastCaseInsensitive(modeInfos[i].shortModeName) || modeName.IsEqual_FastCaseInsensitive(modeInfos[i].longModeName))
		{
			return modeInfos[i];
		}
	}
	return emptyInfo;
}

void KZ::mode::RegisterCommands()
{
	scmd::RegisterCmd("kz_mode", Command_KzMode);
}

void KZDatabaseServiceEventListener_Modes::OnDatabaseSetup()
{
	FOR_EACH_VEC(modeInfos, i)
	{
		if (modeInfos[i].databaseID == -1)
		{
			KZDatabaseService::InsertAndUpdateModeIDs(modeInfos[i].longModeName, modeInfos[i].shortModeName);
		}
	}
	KZDatabaseService::UpdateModeIDs();
}
