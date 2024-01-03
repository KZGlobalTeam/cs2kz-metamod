#pragma once
#include "../kz.h"

class KZCheckpointService : public KZBaseService
{
public:
	KZCheckpointService(KZPlayer *player) : KZBaseService(player)
	{
		this->checkpoints = CUtlVector<Checkpoint>(1, 0);
	}
	virtual void Reset() override;
	// Checkpoint stuff
	struct Checkpoint
	{
		Vector origin;
		QAngle angles;
		Vector ladderNormal;
		bool onLadder;
		CHandle< CBaseEntity2 > groundEnt;
		f32 slopeDropOffset;
		f32 slopeDropHeight;
	};

	i32 currentCpIndex{};
	i32 tpCount;
	bool holdingStill{};
	f32 teleportTime{};

	CUtlVector<Checkpoint> checkpoints;
	void SetCheckpoint();
	void DoTeleport(i32 index);
	void TpHoldPlayerStill();
	void TpToCheckpoint();
	void TpToPrevCp();
	void TpToNextCp();
};
