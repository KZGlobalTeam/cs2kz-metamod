#pragma once
#include "../kz.h"

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
	static void Init();
	bool HasSavedPosition();
	void SavePosition();
	void LoadPosition();
	void ResetSavedPosition();

	bool IsSpectating(KZPlayer *target);
	bool SpectatePlayer(const char *playerName);
	bool SpectatePlayer(KZPlayer *target);
	bool CanSpectate();

	void GetSpectatorList(CUtlVector<CUtlString> &spectatorList);
	KZPlayer *GetSpectatedPlayer();
	KZPlayer *GetNextSpectator(KZPlayer *current);
};
