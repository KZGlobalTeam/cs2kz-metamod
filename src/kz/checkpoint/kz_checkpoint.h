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
		bool onLadder{};
		CHandle< CBaseEntity2 > groundEnt;
		f32 slopeDropOffset;
		f32 slopeDropHeight;
	};

	static_global void RegisterCommands();
private:
	i32 currentCpIndex{};
	u32 tpCount{};
	bool holdingStill{};
	f32 teleportTime{};
	CUtlVector<Checkpoint> checkpoints;

	bool hasCustomStartPosition{};
	Checkpoint customStartPosition;
	Checkpoint const *lastTeleportedCheckpoint{};
public:
	void SetCheckpoint();

	void DoTeleport(const Checkpoint &cp);
	void DoTeleport(i32 index);
	void TpHoldPlayerStill();
	void TpToCheckpoint();
	void TpToPrevCp();
	void TpToNextCp();

	u32 GetTeleportCount() { return this->tpCount; }

	void SetStartPosition();
	void ClearStartPosition();
	bool HasCustomStartPosition() { return this->hasCustomStartPosition; }
	void TpToStartPosition();
};
