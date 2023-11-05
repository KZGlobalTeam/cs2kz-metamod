#pragma once

#include "common.h"
#include "movement/movement.h"
#include "utils/datatypes.h"

#include "../kz.h"

class KZPlayer;

enum JumpType
{
	JumpType_FullInvalid = -1,
	JumpType_LongJump,
	JumpType_Bhop,
	JumpType_MultiBhop,
	JumpType_WeirdJump,
	JumpType_LadderJump,
	JumpType_Ladderhop,
	JumpType_Jumpbug,
	JumpType_Fall,
	JumpType_Other,
	JumpType_Invalid,
	JUMPTYPE_COUNT
};

enum DistanceTier
{
	DistanceTier_None = 0,
	DistanceTier_Meh,
	DistanceTier_Impressive,
	DistanceTier_Perfect,
	DistanceTier_Godlike,
	DistanceTier_Ownage,
	DistanceTier_Wrecker,
	DISTANCETIER_COUNT
};

extern const char *jumpTypeShortStr[JUMPTYPE_COUNT];
class AACall
{
public:
	f32 externalSpeedDiff{};
	f32 prevYaw{};
	f32 currentYaw{};
	Vector wishdir;
	f32 maxspeed{};
	f32 wishspeed{};
	f32 accel{};

	f32 surfaceFriction{};
	f32 subtickFraction{};
	CInButtonState buttons;
	Vector velocityPre;
	Vector velocityPost;
	f32 curtime{};
	i32 tickcount{};
	bool ducking{};

public:
	f32 CalcIdealYaw(bool useRadians = false);
	f32 CalcMinYaw(bool useRadians = false);
	f32 CalcMaxYaw(bool useRadians = false);
	f32 CalcAccelSpeed(bool tryMaxSpeed = false);
	f32 CalcIdealGain();
};

class Strafe
{
public:
	CCopyableUtlVector<AACall> aaCalls;
	TurnState turnstate;

private:
	f32 duration;

	f32 badAngles;
	f32 overlap;
	f32 deadAir;

	f32 syncDuration;
	f32 width;

	f32 gain;
	f32 maxGain;
	f32 loss;
	f32 externalGain;
	f32 externalLoss;
public:
	void End();
	f32 GetStrafeDuration() { return this->duration; };

	f32 GetMaxGain() { return this->maxGain; };
	f32 GetGain(bool external = false) { return external ? this->externalGain : this->gain; };
	f32 GetLoss(bool external = false) { return external ? this->externalLoss : this->loss; };
	f32 GetWidth() { return this->width; };

	// BA/OL/DA
	f32 GetBadAngleDuration() { return this->badAngles; };
	f32 GetOverlapDuration() { return this->overlap; };
	f32 GetDeadAirDuration() { return this->deadAir; };

	f32 GetSync() { return this->syncDuration / this->duration; };
	f32 GetSyncDuration() { return this->syncDuration; };
	// Calculate the ratio for each strafe.
	// The ratio is 0 if the angle is perfect, closer to -100 if it's too slow
	// Closer to 100 if it passes the optimal value.
	// Note: if the player jumps in place, no velocity and no attempt to move at all, any angle will be "perfect".
	// Returns false if there is no available stats.
	internal int SortFloat(const f32 *a, const f32 *b) { return &a > &b; };
	struct AngleRatioStats
	{
		bool available;
		f32 max;
		f32 median;
		f32 average;
	};
	AngleRatioStats arStats;
	
	bool CalcAngleRatioStats();
};

class Jump
{
private:
	KZPlayer *player;

	Vector takeoffOrigin;
	Vector adjustedTakeoffOrigin;

	Vector landingOrigin;
	Vector adjustedLandingOrigin;

	JumpType jumpType;

	// Required for airpath stat.
	f32 totalDistance{};
	f32 currentMaxSpeed{};
	f32 currentMaxHeight = -16384.0f;
	f32 airtime{};

	f32 deadAir{};
	f32 overlap{};
	f32 badAngles{};
	f32 sync{};
	f32 duckDuration{};
	f32 duckEndDuration{};
	f32 width{};
	f32 gainEff{};
	bool validJump = true;
public:
	CCopyableUtlVector<Strafe> strafes;

public:
	Jump() {}
	Jump(KZPlayer *player) : player(player)
	{
		Init();
	}
	void Init();
	void UpdateAACallPost(Vector wishdir, f32 wishspeed, f32 accel);
	void Update();
	void End();
	void Invalidate();

	Strafe *GetCurrentStrafe();
	JumpType GetJumpType() { return this->jumpType; };

	bool IsValid() { return this->validJump; };
	f32 GetOffset() { return adjustedLandingOrigin.z - adjustedTakeoffOrigin.z; };
	f32 GetDistance();
	f32 GetMaxSpeed() { return this->currentMaxSpeed; };
	f32 GetSync() { return this->sync; };
	f32 GetBadAngles() { return this->badAngles; };
	f32 GetOverlap() { return this->overlap; };
	f32 GetDeadAir() { return this->deadAir; };
	f32 GetMaxHeight() { return this->currentMaxHeight; };
	f32 GetWidth() { return this->width; };
	f32 GetEdge(bool landing);
	f32 GetGainEfficiency() { return this->gainEff; };
	f32 GetAirPath();
	f32 GetDuckTime(bool endOnly = true) { return endOnly ? this->duckEndDuration : this->duckDuration; };
	f32 GetDeviation();
	
};

class KZJumpstatsService : public KZBaseService
{
public:
	KZJumpstatsService(KZPlayer *player) : KZBaseService(player)
	{
		this->jumps = CUtlVector<Jump>(1, 0);
	}
	// Jumpstats
	CUtlVector<Jump> jumps;
	bool jsAlways{};
	f32 lastJumpButtonTime{};
	f32 lastNoclipTime{};
	f32 lastDuckbugTime{};
	f32 lastGroundSpeedCappedTime{};
	f32 lastMovementProcessedTime{};

	void OnStartProcessMovement();
	void OnChangeMoveType(MoveType_t oldMoveType);
	JumpType DetermineJumpType();
	bool HitBhop();
	bool HitDuckbugRecently();
	bool ValidWeirdJumpDropDistance();
	bool GroundSpeedCappedRecently();

	void TrackJumpstatsVariables();

	void AddJump();
	void UpdateJump();
	void EndJump();
	void InvalidateJumpstats();
	void OnAirAcceleratePre();
	void OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel);
	void UpdateAACallPost();
	void ToggleJSAlways();
};