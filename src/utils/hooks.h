#include "common.h"
#include "interfaces.h"
#include "igameevents.h"
#include "iserver.h"

namespace hooks
{
	inline CUtlVector<int> entityTouchHooks;

	void Initialize();
	void Cleanup();
	void HookEntities();
	void UnhookEntities();

}

internal void Hook_ClientCommand(CPlayerSlot slot, const CCommand &args);
internal void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
internal int Hook_ProcessUsercmds_Pre(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused);
internal int Hook_ProcessUsercmds_Post(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused);
internal void Hook_CEntitySystem_Spawn_Post(int nCount, const EntitySpawnInfo_t *pInfo);
internal void Hook_CheckTransmit(CCheckTransmitInfo **pInfo, int, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntities);
internal void Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName, int type, uint64 xuid);
internal void Hook_StartupServer(const GameSessionConfiguration_t &config, ISource2WorldSession *, const char *);
internal bool Hook_FireEvent(IGameEvent *event, bool bDontBroadcast);
internal void Hook_DispatchConCommand(ConCommandHandle cmd, const CCommandContext &ctx, const CCommand &args);
internal void Hook_PostEvent(CSplitScreenSlot nSlot, bool bLocalOnly, int nClientCount, const uint64 *clients,
	INetworkSerializable *pEvent, const void *pData, unsigned long nSize, NetChannelBufType_t bufType);
internal void OnStartTouch(CBaseEntity2 *pOther);
internal void OnTouch(CBaseEntity2 *pOther);
internal void OnEndTouch(CBaseEntity2 *pOther);
