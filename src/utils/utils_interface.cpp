#include "utils.h"
#include "gameconfig.h"
#include "iserver.h"
#include "interfaces.h"
#include "cs2kz.h"
#include "ctimer.h"

extern CGameConfig *g_pGameConfig;

CGameConfig *KZUtils::GetGameConfig()
{
	return g_pGameConfig;
}

const CGlobalVars *KZUtils::GetServerGlobals()
{
	return &(g_KZPlugin.serverGlobals);
}

CGlobalVars *KZUtils::GetGlobals()
{
	INetworkGameServer *server = g_pNetworkServerService->GetIGameServer();

	if (!server)
	{
		return nullptr;
	}

	return server->GetGlobals();
}

CBaseEntity *KZUtils::FindEntityByClassname(CEntityInstance *start, const char *name)
{
	return utils::FindEntityByClassname(start, name);
}

CBasePlayerController *KZUtils::GetController(CBaseEntity *entity)
{
	return utils::GetController(entity);
}

CBasePlayerController *KZUtils::GetController(CPlayerSlot slot)
{
	return utils::GetController(slot);
}

CPlayerSlot KZUtils::GetEntityPlayerSlot(CBaseEntity *entity)
{
	return utils::GetEntityPlayerSlot(entity);
}

void KZUtils::SendConVarValue(CPlayerSlot slot, ConVar *conVar, const char *value)
{
	utils::SendConVarValue(slot, conVar, value);
}

void KZUtils::SendMultipleConVarValues(CPlayerSlot slot, ConVar **cvars, const char **values, u32 size)
{
	utils::SendMultipleConVarValues(slot, cvars, values, size);
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

void KZUtils::AddTimer(CTimerBase *timer, bool preserveMapChange)
{
	if (preserveMapChange)
	{
		g_PersistentTimers.AddToTail(timer);
	}
	else
	{
		g_NonPersistentTimers.AddToTail(timer);
	}
}

void KZUtils::RemoveTimer(CTimerBase *timer)
{
	FOR_EACH_VEC(g_PersistentTimers, i)
	{
		if (g_PersistentTimers.Element(i) == timer)
		{
			g_PersistentTimers.Remove(i);
			return;
		}
	}
	FOR_EACH_VEC(g_NonPersistentTimers, i)
	{
		if (g_NonPersistentTimers.Element(i) == timer)
		{
			g_NonPersistentTimers.Remove(i);
			return;
		}
	}
}

CUtlVector<CServerSideClient *> *KZUtils::GetClientList()
{
	if (!g_pNetworkServerService)
	{
		return nullptr;
	}
	static_persist const int offset = g_pGameConfig->GetOffset("ClientOffset");
	return (CUtlVector<CServerSideClient *> *)((char *)g_pNetworkServerService->GetIGameServer() + offset);
}
