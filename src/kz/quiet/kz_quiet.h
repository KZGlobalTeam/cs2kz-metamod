#pragma once
#include "../kz.h"
#include "iserver.h"
#include "networksystem/inetworkserializer.h"

namespace KZ::quiet
{
	void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);

	void OnPostEvent(INetworkMessageInternal *pEvent, const CNetMessage *pData, const uint64 *clients);
} // namespace KZ::quiet

class KZQuietService : public KZBaseService
{
	using KZBaseService::KZBaseService;
	u8 lastObserverMode;
	CHandle<CBaseEntity> lastObserverTarget;
	bool hideWeapon {};
	u32 weaponShownCount[3] {};

public:
	bool hideOtherPlayers {};
	static void Init();
	virtual void Reset() override;

	void OnPlayerPreferencesLoaded();
	void ToggleHide();
	void UpdateHideState();
	void SendFullUpdate();
	bool ShouldHide();
	bool ShouldHideIndex(u32 targetIndex);

	bool ShouldHideWeapon()
	{
		if (!this->player->IsAlive())
		{
			return false;
		}
		return this->hideWeapon;
	}

	bool ShouldHideWeapon(u32 slot)
	{
		if (!this->player->IsAlive())
		{
			return false;
		}
		// Arbitrary number to not crash the client
		if (this->weaponShownCount[slot] < 16)
		{
			this->weaponShownCount[slot]++;
			return false;
		}
		return this->hideWeapon;
	}

	void ToggleHideWeapon();
};
