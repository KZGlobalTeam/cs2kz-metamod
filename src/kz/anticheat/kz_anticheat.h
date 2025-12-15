#pragma once
#include "../kz.h"
class KZBaseService;

class KZAnticheatService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

private:
	bool hasValidCvars = true;

public:
	bool ShouldCheckClientCvars()
	{
		return hasValidCvars;
	}

	void MarkHasInvalidCvars()
	{
		hasValidCvars = false;
	}

	void OnPlayerFullyConnect();

	static f64 KickPlayerInvalidSettings(CPlayerUserId userID);
};

namespace KZ::anticheat
{
	void InitCvarChecker();
}
