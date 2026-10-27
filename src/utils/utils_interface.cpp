#include "utils.h"
#include "gameconfig.h"
#include "engine2/inetworkserverservice.h"
#include "engine2/networkgameserverbase.h"
#include "interfaces.h"
#include "cs2kz.h"
#include "ctimer.h"
#include "keyvalues3.h"
#include <filesystem/ifilesystem.h>
#include "checksum_md5.h"
#include "engine2/serversideclient.h"
#include "sdk/gamerules.h"

#include "memdbgon.h"

static_global char currentMapMD5[33];
static_global bool md5NeedsUpdating {};

extern CGameConfig *g_pGameConfig;

CGameConfig *KZUtils::GetGameConfig()
{
	return g_pGameConfig;
}

const CGlobalVars *KZUtils::GetServerGlobals(bool unmodified)
{
	if (!g_KZPlugin.simulatingPhysics)
	{
		return this->GetGlobals();
	}
	return unmodified ? &g_KZPlugin.unmodifiedServerGlobals : &g_KZPlugin.serverGlobals;
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
		g_PersistentTimers.push_back(timer);
	}
	else
	{
		g_NonPersistentTimers.push_back(timer);
	}
}

void KZUtils::RemoveTimer(CTimerBase *timer)
{
	for (size_t i = 0; i < g_PersistentTimers.size(); i++)
	{
		if (g_PersistentTimers[i] == timer)
		{
			g_PersistentTimers.erase(g_PersistentTimers.begin() + i);
			delete timer;
			return;
		}
	}
	for (size_t i = 0; i < g_NonPersistentTimers.size(); i++)
	{
		if (g_NonPersistentTimers[i] == timer)
		{
			g_NonPersistentTimers.erase(g_NonPersistentTimers.begin() + i);
			delete timer;
			return;
		}
	}
}

CUtlVector<CServerSideClient *> *KZUtils::GetClientList()
{
	// There is no game server before the first map loads.
	if (!g_pNetworkServerService || !g_pNetworkServerService->GetIGameServer())
	{
		return nullptr;
	}
	// CServerSideClient's only base is CServerSideClientBase, so the element pointers are the same.
	auto server = static_cast<CNetworkGameServerBase *>(g_pNetworkServerService->GetIGameServer());
	return reinterpret_cast<CUtlVector<CServerSideClient *> *>(&server->m_Clients);
}

// The engine keeps the client object in its slot after a disconnect, so only hand it out while someone occupies the slot.
CServerSideClient *KZUtils::GetClientBySlot(CPlayerSlot slot)
{
	CUtlVector<CServerSideClient *> *clients = this->GetClientList();
	if (!clients || slot.Get() < 0 || slot.Get() >= clients->Count())
	{
		return nullptr;
	}
	CServerSideClient *client = clients->Element(slot.Get());
	return client && client->IsConnected() ? client : nullptr;
}

std::string KZUtils::GetCurrentMapName(bool *result)
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

	auto currentMap = g_KZPlugin.GetCurrentMap();
	if (currentMap.length() > 0)
	{
		if (result)
		{
			*result = true;
		}

		return currentMap.data();
	}

	if (result)
	{
		*result = false;
	}
	return "";
}

u64 KZUtils::GetCurrentMapWorkshopID()
{
	std::string directory = this->GetCurrentMapDirectory();
	if (directory.find("workshop") != std::string::npos)
	{
		return atoll(V_UnqualifiedFileName(directory.c_str()));
	}
	return 0;
}

std::string KZUtils::GetCurrentMapVPK()
{
	std::string map = this->GetCurrentMapName();
	if (map.empty())
	{
		return "";
	}
	CUtlVector<CUtlString> paths;
	char mapName[1024];

	g_SMAPI->PathFormat(mapName, sizeof(mapName), "maps/%s.vpk", map.c_str());

	g_pFullFileSystem->FindFileAbsoluteList(paths, mapName, "GAME");
	if (paths.Count() > 0)
	{
		std::string realPath = paths[0].Get();
		if (realPath.starts_with("vpk:"))
		{
			realPath = realPath.substr(sizeof("vpk:") - 1);
			return utils::StripExtension(utils::StripFilename(realPath)) + ".vpk";
		}
		return realPath;
	}
	return "";
}

std::string KZUtils::GetCurrentMapDirectory()
{
	return utils::DirName(this->GetCurrentMapVPK());
}

u64 KZUtils::GetCurrentMapSize()
{
	std::string currentMap = this->GetCurrentMapVPK();
	u32 size = 0;
	if (g_pFullFileSystem->FileExists(currentMap.c_str()))
	{
		size = g_pFullFileSystem->Size(currentMap.c_str());
	}
	else
	{
		std::string newFileName = utils::StripExtension(currentMap) + "_dir.vpk";
		size = g_pFullFileSystem->Size(newFileName.c_str());
		for (u32 i = 0; g_pFullFileSystem->FileExists(newFileName.c_str()); i++)
		{
			newFileName = tfm::format("%s_%03i.vpk", utils::StripExtension(currentMap), i);
			size += g_pFullFileSystem->Size(newFileName.c_str());
		}
	}
	return size;
}

bool KZUtils::UpdateCurrentMapMD5()
{
	return this->GetFileMD5(this->GetCurrentMapVPK().c_str(), currentMapMD5, sizeof(currentMapMD5));
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
		std::string originalPath = filePath;
		std::string newFile = utils::StripExtension(originalPath) + "_dir.vpk";
		file = g_pFullFileSystem->OpenEx(newFile.c_str(), "rb");
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
					newFile = tfm::format("%s_%03i.vpk", utils::StripExtension(originalPath), index);
					file = g_pFullFileSystem->OpenEx(newFile.c_str(), "rb");
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
	static_persist const int overlayOffset = g_pGameConfig->GetOffset("GetDebugOverlay");
	void *debugoverlay = CALL_VIRTUAL(void *, overlayOffset, interfaces::pServer);
	if (debugoverlay)
	{
		static_persist const int offset = g_pGameConfig->GetOffset("DebugTriangle");
		CALL_VIRTUAL(void, offset, debugoverlay, &p1, &p2, &p3, r, g, b, a, noDepthTest, flDuration);
	}
}

void KZUtils::ClearOverlays()
{
	static_persist const int overlayOffset = g_pGameConfig->GetOffset("GetDebugOverlay");
	void *debugoverlay = CALL_VIRTUAL(void *, overlayOffset, interfaces::pServer);
	if (debugoverlay)
	{
		static_persist const int offset = g_pGameConfig->GetOffset("ClearOverlays");
		CALL_VIRTUAL(void, offset, debugoverlay);
	}
}
