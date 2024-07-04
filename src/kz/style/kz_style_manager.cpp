#include "kz_style.h"
#include "kz_style_normal.h"

#include "filesystem.h"

#include "utils/utils.h"
#include "interfaces/interfaces.h"

#include "utils/simplecmds.h"

#include "../timer/kz_timer.h"
#include "../language/kz_language.h"
#include "utils/plat.h"

static_function SCMD_CALLBACK(Command_KzStyle);

static_global KZStyleManager styleManager;
KZStyleManager *g_pKZStyleManager = &styleManager;

void KZ::style::InitStyleManager()
{
	static_persist bool initialized = false;
	if (initialized)
	{
		return;
	}
	StyleServiceFactory vnlFactory = [](KZPlayer *player) -> KZStyleService * { return new KZNormalStyleService(player); };
	styleManager.RegisterStyle(0, "NRM", "Normal", vnlFactory);
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

bool KZStyleManager::RegisterStyle(PluginId id, const char *shortName, const char *longName, StyleServiceFactory factory)
{
	if (!shortName || V_strlen(shortName) == 0 || !shortName || V_strlen(longName) == 0)
	{
		return false;
	}
	FOR_EACH_VEC(this->styleInfos, i)
	{
		if (V_stricmp(this->styleInfos[i].shortName, shortName) == 0)
		{
			return false;
		}
		if (V_stricmp(this->styleInfos[i].longName, longName) == 0)
		{
			return false;
		}
	}

	this->styleInfos.AddToTail({id, shortName, longName, factory});
	return true;
}

void KZStyleManager::UnregisterStyle(const char *styleName)
{
	if (!styleName)
	{
		return;
	}

	// Cannot unregister NRM.
	if (V_stricmp("NRM", styleName) == 0 || V_stricmp("Normal", styleName) == 0)
	{
		return;
	}

	FOR_EACH_VEC(this->styleInfos, i)
	{
		if (V_stricmp(this->styleInfos[i].shortName, styleName) == 0 || V_stricmp(this->styleInfos[i].longName, styleName) == 0)
		{
			this->styleInfos.Remove(i);
			break;
		}
	}

	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (strcmp(player->styleService->GetStyleName(), styleName) == 0 || strcmp(player->styleService->GetStyleShortName(), styleName) == 0)
		{
			this->SwitchToStyle(player, "NRM");
		}
	}
}

bool KZStyleManager::SwitchToStyle(KZPlayer *player, const char *styleName, bool silent, bool force)
{
	// Don't change style if it doesn't exist. Instead, print a list of styles to the client.
	if (!styleName || !V_stricmp("", styleName))
	{
		player->languageService->PrintChat(true, false, "Style Command Usage");
		player->languageService->PrintConsole(false, false, "Possible & Current Styles", player->styleService->GetStyleName());
		FOR_EACH_VEC(this->styleInfos, i)
		{
			// clang-format off
			player->PrintConsole(false, false,
				"%s (kz_style %s / kz_style %s)",
				this->styleInfos[i].longName,
				this->styleInfos[i].longName,
				this->styleInfos[i].shortName
			);
			// clang-format on
		}
		return false;
	}

	// If it's the same style, do nothing, unless it's forced.
	if (!force
		&& (V_stricmp(player->styleService->GetStyleName(), styleName) == 0 || V_stricmp(player->styleService->GetStyleShortName(), styleName) == 0))
	{
		return false;
	}

	StyleServiceFactory factory = nullptr;

	FOR_EACH_VEC(this->styleInfos, i)
	{
		if (V_stricmp(this->styleInfos[i].shortName, styleName) == 0 || V_stricmp(this->styleInfos[i].longName, styleName) == 0)
		{
			factory = this->styleInfos[i].factory;
			break;
		}
	}
	if (!factory)
	{
		if (!silent)
		{
			player->languageService->PrintChat(true, false, "Style Not Available", styleName);
		}
		return false;
	}
	player->styleService->Cleanup();
	delete player->styleService;
	player->styleService = factory(player);
	player->timerService->TimerStop();
	player->styleService->Init();

	if (!silent)
	{
		player->languageService->PrintChat(true, false, "Switched Style", player->styleService->GetStyleName());
	}

	return true;
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
	FOR_EACH_VEC(this->styleInfos, i)
	{
		if (this->styleInfos[i].id == 0)
		{
			continue;
		}
		pluginManager->Unload(this->styleInfos[i].id, true, error, sizeof(error));
	}
}

void KZ::style::InitStyleService(KZPlayer *player)
{
	delete player->styleService;
	player->styleService = new KZNormalStyleService(player);
}

static_function SCMD_CALLBACK(Command_KzStyle)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	styleManager.SwitchToStyle(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

void KZ::style::RegisterCommands()
{
	scmd::RegisterCmd("kz_style", Command_KzStyle);
}
