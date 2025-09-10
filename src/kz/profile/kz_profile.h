#pragma once
#include "../kz.h"
#include "kz/global/kz_global.h"

class KZProfileService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

	static void OnGameFrame();
	static void OnCheckTransmit();

	virtual void Reset() override
	{
		clanTag[0] = '\0';
		desiredMode = 0;
		timeToNextRatingRefresh = 0.0f;
		currentRating = -1.0f;
	}

	char clanTag[32] {};
	u8 desiredMode {};
	f32 timeToNextRatingRefresh = 0.0f;
	f64 currentRating = -1.0f;

	void RequestRating();
	bool CanDisplayRank();

	void SetClantag(const char *clanTag)
	{
		V_strncpy(this->clanTag, clanTag, sizeof(this->clanTag));
		this->player->SetClan(clanTag);
	}

	void UpdateClantag();
	void OnPhysicsSimulatePost();
	void UpdateCompetitiveRank();
	std::string GetPrefix(bool colors = true);
};
