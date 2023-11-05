#pragma once
#include "../kz.h"

namespace KZ::quiet
{
	void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);
}


class KZQuietService : public KZBaseService
{
	using KZBaseService::KZBaseService;
public:
	bool hideOtherPlayers{};
	
	virtual void Reset() override;

	void ToggleHide() { this->hideOtherPlayers = !this->hideOtherPlayers; };
};