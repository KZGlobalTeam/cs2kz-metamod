#include "kz_style_legacy_jump.h"

#include "utils/addresses.h"
#include "utils/interfaces.h"
#include "utils/gameconfig.h"

KZLegacyJumpStylePlugin g_KZLegacyJumpStylePlugin;

CGameConfig *g_pGameConfig = NULL;
KZUtils *g_pKZUtils = NULL;
KZStyleManager *g_pStyleManager = NULL;
StyleServiceFactory g_StyleFactory = [](KZPlayer *player) -> KZStyleService * { return new KZLegacyJumpStyleService(player); };
PLUGIN_EXPOSE(KZLegacyJumpStylePlugin, g_KZLegacyJumpStylePlugin);

CConVarRef<bool> sv_legacy_jump("sv_legacy_jump");

bool KZLegacyJumpStylePlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	// Load mode
	int success;
	g_pStyleManager = (KZStyleManager *)g_SMAPI->MetaFactory(KZ_STYLE_MANAGER_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_STYLE_MANAGER_INTERFACE);
		return false;
	}
	g_pKZUtils = (KZUtils *)g_SMAPI->MetaFactory(KZ_UTILS_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_UTILS_INTERFACE);
		return false;
	}
	modules::Initialize();
	if (!interfaces::Initialize(ismm, error, maxlen))
	{
		V_snprintf(error, maxlen, "Failed to initialize interfaces");
		return false;
	}

	if (nullptr == (g_pGameConfig = g_pKZUtils->GetGameConfig()))
	{
		V_snprintf(error, maxlen, "Failed to get game config");
		return false;
	}

	if (!g_pStyleManager->RegisterStyle(g_PLID, STYLE_NAME_SHORT, STYLE_NAME, g_StyleFactory))
	{
		V_snprintf(error, maxlen, "Failed to register style");
		return false;
	}
	ConVar_Register();

	return true;
}

bool KZLegacyJumpStylePlugin::Unload(char *error, size_t maxlen)
{
	g_pStyleManager->UnregisterStyle(g_PLID);
	return true;
}

bool KZLegacyJumpStylePlugin::Pause(char *error, size_t maxlen)
{
	g_pStyleManager->UnregisterStyle(g_PLID);
	return true;
}

bool KZLegacyJumpStylePlugin::Unpause(char *error, size_t maxlen)
{
	if (!g_pStyleManager->RegisterStyle(g_PLID, STYLE_NAME_SHORT, STYLE_NAME, g_StyleFactory))
	{
		return false;
	}
	return true;
}

CGameEntitySystem *GameEntitySystem()
{
	return g_pKZUtils->GetGameEntitySystem();
}

void KZLegacyJumpStyleService::Init()
{
	g_pKZUtils->SendConVarValue(this->player->GetPlayerSlot(), sv_legacy_jump, "true");
}

const CVValue_t *KZLegacyJumpStyleService::GetTweakedConvarValue(const char *name)
{
	static_persist const CVValue_t sv_legacy_jump_desiredValue = true;
	if (!V_stricmp(name, "sv_legacy_jump"))
	{
		return &sv_legacy_jump_desiredValue;
	}
	return nullptr;
}

void KZLegacyJumpStyleService::Cleanup()
{
	g_pKZUtils->SendConVarValue(this->player->GetPlayerSlot(), sv_legacy_jump, "false");
}

void KZLegacyJumpStyleService::OnProcessMovement()
{
	sv_legacy_jump.Set(true);
}
