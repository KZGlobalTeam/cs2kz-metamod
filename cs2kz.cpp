#include "cs2kz.h"

#include "interface.h"
#include "icvar.h"

#include "utils/utils.h"
#include "entity2/entitysystem.h"
#include "utils/recipientfilters.h"
#include "utils/detours.h"
#include "utils/addresses.h"
#include "movement/movement.h"

KZPlugin g_KZPlugin;

CEntitySystem* g_pEntitySystem = NULL;
SH_DECL_HOOK2_void(ISource2GameClients, ClientCommand, SH_NOATTRIB, false, CPlayerSlot, const CCommand&);

PLUGIN_EXPOSE(KZPlugin, g_KZPlugin);

CPlayerManager g_PlayerManager;

bool KZPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	
	if (!utils::Initialize(ismm, error, maxlen))
	{
		return false;
	}
	movement::InitDetours();
	SH_ADD_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);
}

bool KZPlugin::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(ISource2GameClients, ClientCommand, g_pSource2GameClients, SH_STATIC(Hook_ClientCommand), false);

	utils::Cleanup();
	return true;
}

void KZPlugin::AllPluginsLoaded()
{
}

bool KZPlugin::Pause(char *error, size_t maxlen)
{
	return true;
}

bool KZPlugin::Unpause(char *error, size_t maxlen)
{
	return true;
}

const char *KZPlugin::GetLicense()
{
	return "GPLv3";
}

const char *KZPlugin::GetVersion()
{
	return "0.0.0.1";
}

const char *KZPlugin::GetDate()
{
	return __DATE__;
}

const char *KZPlugin::GetLogTag()
{
	return "KZ";
}

const char *KZPlugin::GetAuthor()
{
	return "zer0.k";
}

const char *KZPlugin::GetDescription()
{
	return "CS2 KreedZ";
}

const char *KZPlugin::GetName()
{
	return "CS2KZ";
}

const char *KZPlugin::GetURL()
{
	return "https://cs2.kz/";
}

MovementPlayer *CPlayerManager::ToPlayer(CCSPlayer_MovementServices *ms)
{
	return players[ms->pawn->m_hController.GetEntryIndex()];
}

MovementPlayer *CPlayerManager::ToPlayer(CCSPlayerController *controller)
{
	return nullptr;
}
MovementPlayer *CPlayerManager::ToPlayer(CCSPlayerPawn *pawn)
{
	return nullptr;
}
MovementPlayer *CPlayerManager::ToPlayer(CPlayerSlot slot)
{
	return nullptr;
}
MovementPlayer *CPlayerManager::ToPlayer(CEntityIndex entIndex)
{
	return nullptr;
}
void Hook_ClientCommand(CPlayerSlot slot, const CCommand& args)
{
	RETURN_META(MRES_IGNORED);
}
