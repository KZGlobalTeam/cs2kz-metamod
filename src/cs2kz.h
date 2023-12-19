#pragma once

#include "common.h"
#include "networksystem/inetworkserializer.h"
#include "inetchannel.h"

class KZPlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AllPluginsLoaded();
public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
};

extern KZPlugin g_KZPlugin;

internal void Hook_ClientCommand(CPlayerSlot slot, const CCommand& args);
internal void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
internal float Hook_ProcessUsercmds_Pre(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused);
internal float Hook_ProcessUsercmds_Post(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused);
internal void Hook_CEntitySystem_Spawn_Post(int nCount, const EntitySpawnInfo_t* pInfo);
internal void Hook_CheckTransmit(CCheckTransmitInfo **pInfo, int, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntities);
internal void Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid);
internal void Hook_StartupServer(const GameSessionConfiguration_t &config, ISource2WorldSession *, const char *);
internal bool Hook_FireEvent(IGameEvent *event, bool bDontBroadcast);
internal void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64* clients,
	INetworkSerializable* pEvent, const void* pData, unsigned long nSize, NetChannelBufType_t bufType);