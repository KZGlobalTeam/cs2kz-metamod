#include "kz_style.h"

#include "filesystem.h"

#include "utils/utils.h"
#include "interfaces/interfaces.h"

#include "utils/simplecmds.h"
#include "../db/kz_db.h"
#include "../timer/kz_timer.h"
#include "../language/kz_language.h"
#include "utils/plat.h"

static_global KZStyleManager styleManager;
KZStyleManager *g_pKZStyleManager = &styleManager;
static_global CUtlVector<KZStyleManager::StylePluginInfo> styleInfos;

static_global class KZDatabaseServiceEventListener_Styles : public KZDatabaseServiceEventListener
{
public:
	virtual void OnDatabaseSetup() override;
} databaseEventListener;

void KZ::style::InitStyleManager()
{
	static_persist bool initialized = false;
	if (initialized)
	{
		return;
	}
	KZDatabaseService::RegisterEventListener(&databaseEventListener);
	initialized = true;
}

void KZ::style::LoadStylePlugins()
{
	char buffer[1024];
	g_SMAPI->PathFormat(buffer, sizeof(buffer), "addons/cs2kz/styles/*%s", MODULE_EXT);
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
			g_SMAPI->PathFormat(fullPath, sizeof(fullPath), "%s/addons/cs2kz/styles/%s", g_SMAPI->GetBaseDir(), output);
			bool already = false;
			pluginManager->Load(fullPath, g_PLID, already, error, sizeof(error));
			output = g_pFullFileSystem->FindNext(findHandle);
		} while (output);

		g_pFullFileSystem->FindClose(findHandle);
	}
}

void KZ::style::UpdateStyleDatabaseID(CUtlString name, i32 id)
{
	FOR_EACH_VEC(styleInfos, i)
	{
		if (!V_stricmp(styleInfos[i].longName, name))
		{
			styleInfos[i].databaseID = id;
			break;
		}
	}
}

KZStyleManager::StylePluginInfo KZ::style::GetStyleInfo(KZStyleService *style)
{
	KZStyleManager::StylePluginInfo emptyInfo;
	if (!style)
	{
		META_CONPRINTF("[KZ] Warning: Getting style info from a nullptr!\n");
		return emptyInfo;
	}
	FOR_EACH_VEC(styleInfos, i)
	{
		if (!V_stricmp(style->GetStyleName(), styleInfos[i].longName))
		{
			return styleInfos[i];
		}
	}
	return emptyInfo;
}

KZStyleManager::StylePluginInfo KZ::style::GetStyleInfo(CUtlString styleName)
{
	KZStyleManager::StylePluginInfo emptyInfo;
	if (styleName.IsEmpty())
	{
		META_CONPRINTF("[KZ] Warning: Getting style info from an empty string!\n");
		return emptyInfo;
	}
	FOR_EACH_VEC(styleInfos, i)
	{
		if (styleName.IsEqual_FastCaseInsensitive(styleInfos[i].shortName) || styleName.IsEqual_FastCaseInsensitive(styleInfos[i].longName))
		{
			return styleInfos[i];
		}
	}
	return emptyInfo;
}

bool KZStyleManager::RegisterStyle(PluginId id, const char *shortName, const char *longName, StyleServiceFactory factory,
								   const char **incompatibleStyles, u32 incompatibleStylesCount)
{
	if (!shortName || V_strlen(shortName) == 0 || !shortName || V_strlen(longName) == 0)
	{
		return false;
	}
	StylePluginInfo *info = nullptr;
	FOR_EACH_VEC(styleInfos, i)
	{
		if (!V_stricmp(styleInfos[i].shortName, shortName) || !V_stricmp(styleInfos[i].longName, longName))
		{
			if (styleInfos[i].id < 0)
			{
				info = &styleInfos[i];
				break;
			}
			return false;
		}
	}

	// Add to the list otherwise, and update the database for ID.
	if (!info)
	{
		info = styleInfos.AddToTailGetPtr();
		// If there is already information about this mode while the ID is -1, that means it has to come from the database, so no need to update it.
		KZDatabaseService::InsertAndUpdateStyleIDs(longName, shortName);
	}
	*info = {id, shortName, longName, factory};

	ISmmPluginManager *pluginManager = (ISmmPluginManager *)g_SMAPI->MetaFactory(MMIFACE_PLMANAGER, nullptr, nullptr);
	const char *path;
	pluginManager->Query(id, &path, nullptr, nullptr);
	g_pKZUtils->GetFileMD5(path, info->md5, sizeof(info->md5));

	for (u32 i = 0; i < incompatibleStylesCount; i++)
	{
		info->incompatibleStyles.AddToTail(incompatibleStyles[i]);
	}

	return true;
}

void KZStyleManager::UnregisterStyle(const char *styleName)
{
	if (!styleName)
	{
		return;
	}

	FOR_EACH_VEC(styleInfos, i)
	{
		if (V_stricmp(styleInfos[i].shortName, styleName) == 0 || V_stricmp(styleInfos[i].longName, styleName) == 0)
		{
			styleInfos[i].id = -1;
			styleInfos[i].md5[0] = 0;
			styleInfos[i].factory = nullptr;
			styleInfos[i].incompatibleStyles.RemoveAll();
			break;
		}
	}

	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		FOR_EACH_VEC(player->styleServices, i)
		{
			if (!V_stricmp(player->styleServices[i]->GetStyleName(), styleName)
				|| !V_stricmp(player->styleServices[i]->GetStyleShortName(), styleName))
			{
				this->RemoveStyle(player, styleName);
				break;
			}
		}
	}
}

void KZStyleManager::Cleanup()
{
	int ret;
	ISmmPluginManager *pluginManager = (ISmmPluginManager *)g_SMAPI->MetaFactory(MMIFACE_PLMANAGER, &ret, 0);
	if (ret == META_IFACE_FAILED)
	{
		return;
	}
	char error[256];
	FOR_EACH_VEC(styleInfos, i)
	{
		if (styleInfos[i].id <= 0)
		{
			continue;
		}
		pluginManager->Unload(styleInfos[i].id, true, error, sizeof(error));
	}
}

void KZStyleManager::AddStyle(KZPlayer *player, const char *styleName, bool silent)
{
	// Don't add style if it doesn't exist. Instead, print a list of styles to the client.
	if (!styleName || !V_stricmp("", styleName))
	{
		player->languageService->PrintChat(true, false, "Add Style Command Usage");
		// clang-format off
		KZStyleManager::PrintAllStyles(player);
		return;
		// clang-format on
	}
	FOR_EACH_VEC(player->styleServices, i)
	{
		if (!V_stricmp(player->styleServices[i]->GetStyleName(), styleName) || !V_stricmp(player->styleServices[i]->GetStyleShortName(), styleName))
		{
			player->languageService->PrintChat(true, false, "Style Already Active", styleName);
			return;
		}
	}

	StylePluginInfo info;

	FOR_EACH_VEC(styleInfos, i)
	{
		if (V_stricmp(styleInfos[i].shortName, styleName) == 0 || V_stricmp(styleInfos[i].longName, styleName) == 0)
		{
			info = styleInfos[i];
			break;
		}
	}
	if (!info.factory)
	{
		if (!silent)
		{
			player->languageService->PrintChat(true, false, "Style Not Available", styleName);
		}
		return;
	}

	FOR_EACH_VEC(player->styleServices, i)
	{
		// clang-format off
		if (info.incompatibleStyles.HasElement(player->styleServices[i]->GetStyleName())
			|| info.incompatibleStyles.HasElement(player->styleServices[i]->GetStyleShortName())
			|| !player->styleServices[i]->IsCompatibleWithStyle(info.shortName)
			|| !player->styleServices[i]->IsCompatibleWithStyle(info.longName))
		// clang-format on
		{
			player->languageService->PrintChat(true, false, "Style Conflict", styleName, player->styleServices[i]->GetStyleName());
			return;
		}
	}
	player->styleServices.AddToTail(info.factory(player));
	player->timerService->TimerStop();
	player->styleServices.Tail()->Init();

	if (!silent)
	{
		player->languageService->PrintChat(true, false, "Style Added", info.longName);
	}
}

void KZStyleManager::RemoveStyle(KZPlayer *player, const char *styleName, bool silent)
{
	if (!styleName || !V_stricmp("", styleName))
	{
		player->languageService->PrintChat(true, false, "Remove Style Command Usage");
		return;
	}

	FOR_EACH_VEC(player->styleServices, i)
	{
		auto style = player->styleServices[i];
		if (!V_stricmp(style->GetStyleName(), styleName) || !V_stricmp(style->GetStyleShortName(), styleName))
		{
			style->Cleanup();
			if (!silent)
			{
				player->languageService->PrintChat(true, false, "Style Removed", style->GetStyleName());
			}
			player->styleServices.Remove(i);
			delete style;
			return;
		}
	}
	if (!silent)
	{
		player->languageService->PrintChat(true, false, "Style Not Active", styleName);
	}
}

void KZStyleManager::ToggleStyle(KZPlayer *player, const char *styleName, bool silent)
{
	// Don't change style if it doesn't exist. Instead, print a list of styles to the client.
	if (!styleName || !V_stricmp("", styleName))
	{
		player->languageService->PrintChat(true, false, "Toggle Style Command Usage");
		KZStyleManager::PrintAllStyles(player);
		return;
	}

	// Try to remove styles first
	FOR_EACH_VEC(player->styleServices, i)
	{
		auto style = player->styleServices[i];
		if (!V_stricmp(style->GetStyleName(), styleName) || !V_stricmp(style->GetStyleShortName(), styleName))
		{
			style->Cleanup();
			if (!silent)
			{
				player->languageService->PrintChat(true, false, "Style Removed", style->GetStyleName());
			}
			player->styleServices.Remove(i);
			delete style;
			return;
		}
	}
	StylePluginInfo info;

	FOR_EACH_VEC(styleInfos, i)
	{
		if (V_stricmp(styleInfos[i].shortName, styleName) == 0 || V_stricmp(styleInfos[i].longName, styleName) == 0)
		{
			info = styleInfos[i];
			break;
		}
	}
	if (!info.factory)
	{
		if (!silent)
		{
			player->languageService->PrintChat(true, false, "Style Not Available", styleName);
		}
		return;
	}

	FOR_EACH_VEC(player->styleServices, i)
	{
		// clang-format off
		if (info.incompatibleStyles.HasElement(player->styleServices[i]->GetStyleName())
			|| info.incompatibleStyles.HasElement(player->styleServices[i]->GetStyleShortName())
			|| !player->styleServices[i]->IsCompatibleWithStyle(info.shortName)
			|| !player->styleServices[i]->IsCompatibleWithStyle(info.longName))
		// clang-format on
		{
			player->languageService->PrintChat(true, false, "Style Conflict", styleName, player->styleServices[i]->GetStyleName());
			return;
		}
	}
	player->styleServices.AddToTail(info.factory(player));
	player->timerService->TimerStop();
	player->styleServices.Tail()->Init();

	if (!silent)
	{
		player->languageService->PrintChat(true, false, "Style Added", info.longName);
	}
}

void KZStyleManager::ClearStyles(KZPlayer *player, bool silent)
{
	FOR_EACH_VEC(player->styleServices, i)
	{
		player->styleServices[i]->Cleanup();
	}
	player->styleServices.PurgeAndDeleteElements();
	if (!silent)
	{
		player->languageService->PrintChat(true, false, "Styles Cleared");
	}
}

void KZStyleManager::RefreshStyles(KZPlayer *player)
{
	CUtlVector<StylePluginInfo> styles;
	FOR_EACH_VEC(player->styleServices, i)
	{
		styles.AddToTail(KZ::style::GetStyleInfo(player->styleServices[i]->GetStyleName()));
	}
	KZStyleManager::ClearStyles(player, true);
	FOR_EACH_VEC(styles, i)
	{
		KZStyleManager::AddStyle(player, styles[i].longName, true);
	}
}

void KZStyleManager::PrintActiveStyles(KZPlayer *player)
{
	player->languageService->PrintConsole(false, false, "Current Styles");
	FOR_EACH_VEC(player->styleServices, i)
	{
		// clang-format off
		player->PrintConsole(false, false,
			"%s (%s)",
			player->styleServices[i]->GetStyleName(),
			player->styleServices[i]->GetStyleShortName()
		);
		// clang-format on
	}
}

void KZStyleManager::PrintAllStyles(KZPlayer *player)
{
	player->languageService->PrintConsole(false, false, "Possible Styles");
	FOR_EACH_VEC(styleInfos, i)
	{
		if (styleInfos[i].id < 0)
		{
			continue;
		}
		// clang-format off
		player->PrintConsole(false, false,
			"%s (%s)",
			styleInfos[i].longName,
			styleInfos[i].shortName
		);
		// clang-format on
	}
}

void KZ::style::InitStyleService(KZPlayer *player) {}

void KZDatabaseServiceEventListener_Styles::OnDatabaseSetup()
{
	FOR_EACH_VEC(styleInfos, i)
	{
		if (styleInfos[i].databaseID == -1)
		{
			KZDatabaseService::InsertAndUpdateStyleIDs(styleInfos[i].longName, styleInfos[i].shortName);
		}
	}
	KZDatabaseService::UpdateStyleIDs();
}

static_function SCMD_CALLBACK(Command_KzToggleStyle)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	styleManager.ToggleStyle(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzStyle)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!args->Arg(1))
	{
	}
	if (args->Arg(1)[0] == '+')
	{
		const char *styleName = args->Arg(1) + 1;
		styleManager.AddStyle(player, styleName);
	}
	else if (args->Arg(1)[0] == '-')
	{
		const char *styleName = args->Arg(1) + 1;
		styleManager.RemoveStyle(player, styleName);
	}
	else
	{
		styleManager.ToggleStyle(player, args->Arg(1));
	}
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzAddStyle)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	styleManager.AddStyle(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzRemoveStyle)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	styleManager.RemoveStyle(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzClearStyles)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	styleManager.ClearStyles(player);
	return MRES_SUPERCEDE;
}

void KZ::style::RegisterCommands()
{
	scmd::RegisterCmd("kz_style", Command_KzStyle);
	scmd::RegisterCmd("kz_togglestyle", Command_KzToggleStyle);
	scmd::RegisterCmd("kz_addstyle", Command_KzAddStyle);
	scmd::RegisterCmd("kz_removestyle", Command_KzRemoveStyle);
	scmd::RegisterCmd("kz_clearstyles", Command_KzClearStyles);
}
