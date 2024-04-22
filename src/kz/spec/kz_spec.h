#pragma once
#include "../kz.h"

class KZTimerServiceEventListener;

#include "../timer/kz_timer.h"

class KZSpecServiceTimerEventListener : public KZTimerServiceEventListener
{
	virtual void OnTimerStartPost(KZPlayer *player, const char *courseName) override;
};

class KZSpecService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool savedPosition;
	Vector savedOrigin;
	QAngle savedAngles;
	bool savedOnLadder;

public:
	virtual void Reset() override;
	static_global void Init();
	static_global void RegisterCommands();
	bool HasSavedPosition();
	void SavePosition();
	void LoadPosition();
	void ResetSavedPosition();

	bool IsSpectating(KZPlayer *target);
	bool SpectatePlayer(const char *playerName);
	bool CanSpectate();

	KZPlayer *GetSpectatedPlayer();
	KZPlayer *GetNextSpectator(KZPlayer *current);
};
