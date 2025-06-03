#include "utils.h"
#include "gameconfig.h"
#include "iserver.h"
#include "interfaces.h"
#include "cs2kz.h"
#include "ctimer.h"
#include "keyvalues3.h"
#include <filesystem.h>
#include "checksum_md5.h"
#include "sdk/serversideclient.h"
#include "sdk/gamerules.h"

#include "memdbgon.h"

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

void KZUtils::SendConVarValue(CPlayerSlot slot, ConVarRefAbstract conVar, const char *value)
{
	utils::SendConVarValue(slot, conVar, value);
}

void KZUtils::SendMultipleConVarValues(CPlayerSlot slot, ConVarRefAbstract **conVars, const char **values, u32 size)
{
	utils::SendMultipleConVarValues(slot, conVars, values, size);
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

CUtlString KZUtils::GetCurrentMapName(bool *result)
{
	CNetworkGameServerBase *networkGameServer = (CNetworkGameServerBase *)g_pNetworkServerService->GetIGameServer();
	if (networkGameServer)
	{
		if (result)
		{
			*result = true;
		}
		return networkGameServer->GetMapName();
	}

	const CGlobalVars *globals = g_pKZUtils->GetGlobals();
	if (globals && strlen(globals->mapname.ToCStr()))
	{
		if (result)
		{
			*result = true;
		}
		return globals->mapname.ToCStr();
	}

	if (result)
	{
		*result = false;
	}
	return "";
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
	CUtlString map = this->GetCurrentMapName();
	if (map.IsEmpty())
	{
		return "";
	}
	CUtlVector<CUtlString> paths;
	char mapName[1024];

	g_SMAPI->PathFormat(mapName, sizeof(mapName), "maps/%s.vpk", map.Get());

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
	CUtlString currentMap = this->GetCurrentMapVPK();
	u32 size = 0;
	if (g_pFullFileSystem->FileExists(currentMap.Get()))
	{
		size = g_pFullFileSystem->Size(currentMap.Get());
	}
	else
	{
		CUtlString newFileName = currentMap.StripExtension() + "_dir.vpk";
		size = g_pFullFileSystem->Size(newFileName.Get());
		for (u32 i = 0; g_pFullFileSystem->FileExists(newFileName.Get()); i++)
		{
			newFileName.Format("%s_%03i.vpk", currentMap.StripExtension().Get(), i);
			size += g_pFullFileSystem->Size(newFileName.Get());
		}
	}
	return size;
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
	if (file)
	{
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

	// Try and get the MD5 for multifile vpks.
	if (V_strlen(filePath) >= 4 && KZ_STREQI(filePath + V_strlen(filePath) - 4, ".vpk"))
	{
		i32 index = 0;
		CUtlString originalPath = filePath;
		CUtlString newFile = originalPath.StripExtension() + "_dir.vpk";
		file = g_pFullFileSystem->OpenEx(newFile.Get(), "rb");
		if (file)
		{
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
					newFile.Format("%s_%03i.vpk", originalPath.StripExtension().Get(), index);
					file = g_pFullFileSystem->OpenEx(newFile.Get(), "rb");
					if (!file)
					{
						break;
					}
					sizeRemaining = g_pFullFileSystem->Size(file);
					index++;
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
		return false;
	}
	return false;
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

u32 KZUtils::GetPlayerCount()
{
	u32 count = 0;
	auto clients = this->GetClientList();
	if (clients)
	{
		FOR_EACH_VEC(*clients, i)
		{
			CServerSideClient *client = clients->Element(i);

			if (client && client->IsConnected() && !client->IsFakeClient() && !client->IsHLTV())
			{
				count++;
			}
		}
	}
	return count;
}

void KZUtils::AddTriangleOverlay(Vector const &p1, Vector const &p2, Vector const &p3, u8 r, u8 g, u8 b, u8 a, bool noDepthTest, f64 flDuration)
{
	void *debugoverlay = CALL_VIRTUAL(void *, g_pGameConfig->GetOffset("GetDebugOverlay"), interfaces::pServer);
	if (debugoverlay)
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("DebugTriangle"), debugoverlay, &p1, &p2, &p3, r, g, b, a, noDepthTest, flDuration);
	}
}

void KZUtils::ClearOverlays()
{
	void *debugoverlay = CALL_VIRTUAL(void *, g_pGameConfig->GetOffset("GetDebugOverlay"), interfaces::pServer);
	if (debugoverlay)
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("ClearOverlays"), debugoverlay);
	}
}

BotProfile *KZUtils::GetBotProfile(u8 difficulty, i32 teamNumber, i32 weaponClass)
{
	BotProfile *result = NULL;
	if (!interfaces::ppBotProfileManager || !*interfaces::ppBotProfileManager)
	{
		return result;
	}
	result = this->GetBotProfile_(*interfaces::ppBotProfileManager, difficulty, teamNumber, weaponClass, false);
	return result;
}
