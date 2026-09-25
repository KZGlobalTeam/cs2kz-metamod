#pragma once
#include "kz/kz.h"
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
	CHandle<CBaseEntity> weaponCamera {};

	void UpdateWeaponCamera();
	void ReleaseWeaponCamera();

public:
	bool hideOtherPlayers {};
	static void Init();
	static void Cleanup();
	virtual void Reset() override;

	void OnPhysicsSimulatePost();
	void ApplyPreferences();
	void ToggleHide();
	void UpdateHideState();
	void SendFullUpdate();
	bool ShouldHide();
	bool ShouldHideIndex(u32 targetIndex);

	void ToggleHideWeapon();
};
