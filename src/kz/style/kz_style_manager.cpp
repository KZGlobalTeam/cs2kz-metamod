#include "kz_style.h"
#include "kz_style_normal.h"

#include "filesystem.h"

#include "utils/utils.h"
#include "interfaces/interfaces.h"

#include "utils/simplecmds.h"

#include "../timer/kz_timer.h"

internal SCMD_CALLBACK(Command_KzStyle);

internal KZStyleManager styleManager;
KZStyleManager *g_pKZStyleManager = &styleManager;

void KZ::style::InitStyleManager()
{
	static bool initialized = false;
	if (initialized)
	{
		return;
	}
	StyleServiceFactory vnlFactory = [](KZPlayer *player) -> KZStyleService *{ return new KZNormalStyleService(player); };
	styleManager.RegisterStyle(0, "NRM", "Normal", vnlFactory);
	initialized = true;
}

void KZ::style::LoadStylePlugins()
{
	char buffer[1024];
	g_SMAPI->PathFormat(buffer, sizeof(buffer), "addons/cs2kz/styles/*.*");
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

	this->styleInfos.AddToTail({ id, shortName, longName, factory });
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

void KZStyleManager::LoadDefaultStyle()
{
	char styleCfgPath[1024];
	V_snprintf(styleCfgPath, sizeof(styleCfgPath), "%s%s", g_SMAPI->GetBaseDir(), "/addons/cs2kz/styles/style-config.txt");

	KeyValues *styleCfgKeyValues = new KeyValues("StyleConfig");
	styleCfgKeyValues->LoadFromFile(g_pFullFileSystem, styleCfgPath, nullptr);

	const char *styleName = styleCfgKeyValues->GetString("defaultStyle");

	FOR_EACH_VEC(this->styleInfos, i)
	{
		if (V_stricmp(this->styleInfos[i].shortName, styleName) == 0 || V_stricmp(this->styleInfos[i].longName, styleName) == 0)
		{
			defaultStyle = styleName;
			break;
		}
	}
}

bool KZStyleManager::SwitchToStyle(KZPlayer *player, const char *styleName, bool silent)
{
	// Don't change style if it doesn't exist. Instead, print a list of styles to the client.
	if (!styleName || !V_stricmp("", styleName))
	{
		player->PrintChat(true, false, "{grey}Usage: {default}kz_style <style>{grey}. Check console for possible styles!", KZ_CHAT_PREFIX);
		player->PrintConsole(false, false, "Possible styles: (Current style is %s)", player->styleService->GetStyleName());
		FOR_EACH_VEC(this->styleInfos, i)
		{
			player->PrintConsole(false, false, "%s (kz_style %s / kz_style %s)", this->styleInfos[i].longName, this->styleInfos[i].longName, this->styleInfos[i].shortName);
		}
		return false;
	}

	// If it's the same style, do nothing.
	if (V_stricmp(player->styleService->GetStyleName(), styleName) == 0 || V_stricmp(player->styleService->GetStyleShortName(), styleName) == 0)
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
			player->PrintChat(true, false, "{grey}The {purple}%s {grey}style is not available.", styleName);
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
		player->PrintChat(true, false, "{grey}You have switched to the {purple}%s {grey}style.", player->styleService->GetStyleName());
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
		if (this->styleInfos[i].id == 0) continue;
		pluginManager->Unload(this->styleInfos[i].id, true, error, sizeof(error));
	}
}

void KZ::style::InitStyleService(KZPlayer *player)
{
	delete player->styleService;
	player->styleService = new KZNormalStyleService(player);
}


internal SCMD_CALLBACK(Command_KzStyle)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	styleManager.SwitchToStyle(player, args->Arg(1));
	return MRES_SUPERCEDE;
}

void KZ::style::RegisterCommands()
{
	scmd::RegisterCmd("kz_style", Command_KzStyle, "List or change style.");
}
