#pragma once
#include "kz/kz.h"

class KZCheckpointService : public KZBaseService
{
public:
	KZCheckpointService(KZPlayer *player) : KZBaseService(player) {}

	static void Init();
	static void RegisterMenu();
	virtual void Reset() override;

	// Checkpoint stuff
	struct Checkpoint
	{
		Vector origin;
		QAngle angles;
		Vector ladderNormal;
		bool onLadder {};
		CHandle<CBaseEntity> groundEnt;
	};

	// UndoTeleport stuff
	struct UndoTeleportData : public Checkpoint
	{
		bool teleportOnGround {};
		bool teleportInBhopTrigger {};
		bool teleportInAntiCpTrigger {};
	};

	struct CustomStartPosition
	{
		Vector origin;
		QAngle angles;
		Vector ladderNormal;
		bool onLadder {};
		bool onGround {};
	};

private:
	i32 currentCpIndex {};
	u32 tpCount {};
	bool holdingStill {};
	f32 teleportTime {};
	std::vector<Checkpoint> checkpoints;
	UndoTeleportData undoTeleportData;

	bool hasCustomStartPosition {};
	CustomStartPosition customStartPosition;
	Checkpoint lastTeleportedCheckpoint {};
	bool lastTeleportForcedOnGround {};
	void DoTeleport(const Checkpoint cp, bool stayOnGround, i32 checkpointIndex, bool undo);

public:
	void OnPlayerPreferencesLoaded();
	void ResetCheckpoints(bool playSound = false, bool resetTeleports = true);
	void SetCheckpoint();

	void UndoTeleport();
	void DoTeleport(const Checkpoint cp);
	void DoTeleport(const Checkpoint cp, bool stayOnGround);
	void DoTeleport(i32 index);
	void TpHoldPlayerStill();
	void TpToCheckpoint();
	void TpToPrevCp();
	void TpToNextCp();

	u32 GetTeleportCount()
	{
		return this->tpCount;
	}

	i32 GetCurrentCpIndex()
	{
		if (!this->checkpoints.empty())
		{
			return this->currentCpIndex + 1;
		}
		else
		{
			return this->currentCpIndex;
		}
	}

	i32 GetCheckpointCount()
	{
		return (i32)this->checkpoints.size();
	}

	void SetStartPosition();
	void ClearStartPosition();

	bool HasCustomStartPosition()
	{
		return this->hasCustomStartPosition;
	}

	void TpToStartPosition();

	void PlayCheckpointErrorSound();
	void PlayTeleportErrorSound();
	void PlayCheckpointSound();
	void PlayTeleportSound();
	void PlayCheckpointResetSound();
};
