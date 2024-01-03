#pragma once
#include "../kz.h"
#include "networksystem/inetworkserializer.h"
#include "inetchannel.h"

namespace KZ::quiet
{
	void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);

	void OnPostEvent(INetworkSerializable *pEvent, const void *pData, const uint64 *clients);
}


class KZQuietService : public KZBaseService
{
	using KZBaseService::KZBaseService;
	u8 lastObserverMode;
	CHandle< CBaseEntity2 > lastObserverTarget;
public:
	bool hideOtherPlayers{};
	virtual void Reset() override;

	void ToggleHide();
	void UpdateHideState();
	void SendFullUpdate();
	bool ShouldHide();
	bool ShouldHideIndex(u32 targetIndex);
};
