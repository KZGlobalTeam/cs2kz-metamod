#pragma once
#include "../kz.h"
#include "kz/global/kz_global.h"

class KZProfileService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

	static void OnCheckTransmit();

	virtual void Reset() override
	{
		clanTag[0] = '\0';
		clanTagOverride[0] = '\0';
		chatPrefixOverride[0] = '\0';
		desiredMode = 0;
		timeToNextRatingRefresh = 0.0f;
		currentRating = -1.0f;
	}

	static inline bool isDirty = false;
	char clanTag[32] {};
	u8 desiredMode {};
	f32 timeToNextRatingRefresh = 0.0f;
	f64 currentRating = -1.0f;

	// Overrides set through the public plugin interface (see src/public/ics2kz.h).
	// Empty means "use the kz-computed value". They are independent of each other.
	char clanTagOverride[32] {};
	char chatPrefixOverride[128] {};

	void OnPlayerActive();

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

	// Replace the scoreboard clan tag. A null or empty tag clears the override.
	// Applies immediately instead of waiting for the next mode/style/rating event.
	void SetClanTagOverride(const char *tag);

	const char *GetClanTagOverride() const
	{
		return this->clanTagOverride;
	}

	// Replace the chat prefix. A null or empty prefix clears the override.
	void SetChatPrefixOverride(const char *prefix)
	{
		V_strncpy(this->chatPrefixOverride, prefix ? prefix : "", sizeof(this->chatPrefixOverride));
	}

	const char *GetChatPrefixOverride() const
	{
		return this->chatPrefixOverride;
	}
};
