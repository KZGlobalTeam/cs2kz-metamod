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
public:
	bool hideOtherPlayers{};
	
	virtual void Reset() override;

	void ToggleHide() { this->hideOtherPlayers = !this->hideOtherPlayers; };

	bool ShouldHide();
	bool ShouldHideIndex(u32 targetIndex);
};