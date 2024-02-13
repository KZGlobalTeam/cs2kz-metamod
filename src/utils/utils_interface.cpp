#include "utils.h"
#include "gameconfig.h"
#include "iserver.h"
#include "interfaces.h"
#include "cs2kz.h"

extern CGameConfig *g_pGameConfig;

CGameConfig *KZUtils::GetGameConfig()
{
	return g_pGameConfig;
}

CSchemaSystem *KZUtils::GetSchemaSystemPointer()
{
	return interfaces::pSchemaSystem;
}

const CGlobalVars *KZUtils::GetServerGlobals()
{
	return &(g_KZPlugin.serverGlobals);
}

CGlobalVars *KZUtils::GetGlobals()
{
	INetworkGameServer *server = g_pNetworkServerService->GetIGameServer();

	if(!server)
		return nullptr;

	return server->GetGlobals();
}

CBaseEntity2 *KZUtils::FindEntityByClassname(CEntityInstance *start, const char *name)
{
	return utils::FindEntityByClassname(start, name);
}

CBasePlayerController *KZUtils::GetController(CBaseEntity2 *entity)
{
	return utils::GetController(entity);
}

CBasePlayerController *KZUtils::GetController(CPlayerSlot slot)
{
	return utils::GetController(slot);
}

CPlayerSlot KZUtils::GetEntityPlayerSlot(CBaseEntity2 *entity)
{
	return utils::GetEntityPlayerSlot(entity);
}

f32 KZUtils::NormalizeDeg(f32 a)
{
	return utils::NormalizeDeg(a);
}

f32 KZUtils::GetAngleDifference(const f32 source, const f32 target, const f32 c, bool relative)
{
	return utils::GetAngleDifference(source, target, c, relative);
}

CGameEntitySystem *KZUtils::GetGameEntitySystem()
{
	return GameEntitySystem();
}
