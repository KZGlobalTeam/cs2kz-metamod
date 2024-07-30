#include "utils.h"
#include "gameconfig.h"
#include "iserver.h"
#include "interfaces.h"
#include "cs2kz.h"
#include "ctimer.h"
#include "keyvalues3.h"
#include <filesystem.h>
#include "checksum_md5.h"

#include "memdbgon.h"
#include "sdk/gamerules.h"
static_global char currentMapMD5[33];
static_global bool md5NeedsUpdating {};

extern CGameConfig *g_pGameConfig;

CGameConfig *KZUtils::GetGameConfig()
{
	return g_pGameConfig;
}

const CGlobalVars *KZUtils::GetServerGlobals()
{
	return g_KZPlugin.simulatingPhysics ? &(g_KZPlugin.serverGlobals) : this->GetGlobals();
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

u64 KZUtils::GetCurrentMapWorkshopID()
{
	CUtlString directory = this->GetCurrentMapDirectory();
	if (directory.MatchesPattern("*workshop*"))
	{
		return atoll(directory.UnqualifiedFilenameAlloc());
	}
	return 0;
}

CUtlString KZUtils::GetCurrentMapVPK()
{
	CNetworkGameServerBase *networkGameServer = (CNetworkGameServerBase *)g_pNetworkServerService->GetIGameServer();
	if (!networkGameServer)
	{
		return "";
	}
	CUtlVector<CUtlString> paths;
	char mapName[1024];

	g_SMAPI->PathFormat(mapName, sizeof(mapName), "maps/%s.vpk", networkGameServer->GetMapName());

	g_pFullFileSystem->FindFileAbsoluteList(paths, mapName, "GAME");
	if (paths.Count() > 0)
	{
		CUtlString realPath = paths[0];
		if (realPath.MatchesPattern("vpk:*"))
		{
			realPath = realPath.Slice((sizeof("vpk:") - 1));
			return realPath.StripFilename().StripExtension() + ".vpk";
		}
		return realPath;
	}
	return "";
}

CUtlString KZUtils::GetCurrentMapDirectory()
{
	return this->GetCurrentMapVPK().DirName();
}

u64 KZUtils::GetCurrentMapSize()
{
	if (this->GetCurrentMapVPK())
	{
		return g_pFullFileSystem->Size(this->GetCurrentMapVPK(), "GAME");
	}
	return 0;
}

bool KZUtils::UpdateCurrentMapMD5()
{
	return this->GetFileMD5(this->GetCurrentMapVPK(), currentMapMD5, sizeof(currentMapMD5));
}

bool KZUtils::GetCurrentMapMD5(char *buffer, i32 size)
{
	if (md5NeedsUpdating)
	{
		md5NeedsUpdating = this->UpdateCurrentMapMD5();
	}
	if (!md5NeedsUpdating)
	{
		V_strncpy(buffer, currentMapMD5, size);
		return true;
	}
	return false;
}

bool KZUtils::GetFileMD5(const char *filePath, char *buffer, i32 size)
{
	u8 chunk[8192];
	FileHandle_t file = g_pFullFileSystem->OpenEx(filePath, "rb");
	i64 sizeRemaining = g_pFullFileSystem->Size(file);
	i32 bytesRead;
	MD5Context_t ctx;
	unsigned char digest[MD5_DIGEST_LENGTH];
	memset(&ctx, 0, sizeof(MD5Context_t));

	MD5Init(&ctx);
	while (sizeRemaining > 0)
	{
		bytesRead = g_pFullFileSystem->Read(chunk, sizeRemaining > sizeof(chunk) ? sizeof(chunk) : sizeRemaining, file);
		sizeRemaining -= bytesRead;
		if (bytesRead > 0)
		{
			MD5Update(&ctx, chunk, bytesRead);
		}
		if (g_pFullFileSystem->EndOfFile(file))
		{
			g_pFullFileSystem->Close(file);
			file = NULL;
			break;
		}
		else if (!g_pFullFileSystem->IsOk(file))
		{
			if (file)
			{
				g_pFullFileSystem->Close(file);
			}
			return false;
		}
	}

	MD5Final(digest, &ctx);
	char *data = MD5_Print(digest, sizeof(digest));
	V_strncpy(buffer, data, size);
	if (file)
	{
		g_pFullFileSystem->Close(file);
	}
	return true;
}

CCSGameRules *KZUtils::GetGameRules()
{
	CCSGameRulesProxy *proxy = NULL;
	proxy = (CCSGameRulesProxy *)utils::FindEntityByClassname(proxy, "cs_gamerules");
	if (proxy)
	{
		return proxy->m_pGameRules();
	}
	return nullptr;
}
