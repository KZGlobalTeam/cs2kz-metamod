#pragma once
#include "kz/kz.h"

class KZSpecService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool savedPosition;
	Vector savedOrigin;
	QAngle savedAngles;
	Vector savedVelocity;
	bool savedOnLadder;

public:
	virtual void Reset() override;
	static void Init();
	bool HasSavedPosition();
	void SavePosition();
	void LoadPosition();
	void ResetSavedPosition();

	// Whether the player views through their observer pawn, either without a living pawn or while their pawn persists.
	bool IsObserving();
	bool IsSpectating(KZPlayer *target);
	bool SpectatePlayer(const char *playerName);
	// force skips CanSpectate.
	bool SpectatePlayer(KZPlayer *target, bool force = false);
	bool CanSpectate();

	void GetSpectatorList(std::vector<std::string> &spectatorList);
	KZPlayer *GetSpectatedPlayer();
	KZPlayer *GetNextSpectator(KZPlayer *current);
	// The game's own check takes any living pawn, including one its player does not control.
	bool CanObserveTarget(CBaseEntity *target);
};
