#pragma once
#include "kz/kz.h"
#include "tier1/circularbuffer.h"

/*
 For the player beam, create a grenade that is X ticks behind the player's origin.

 This is done by storing a history of player's position.

 If the player lands on the ground/is on a ladder, tell the beam to stop updating after X ticks.

 If the player noclips, tell the beam to stop updating after X ticks and only start updating once the player is on the ground and/or is on a ladder.

 If the player teleports, force the grenade to recreate itself after X ticks so that no beam can be made.
*/
namespace KZ::beam
{
	constexpr int originHistorySize = 8;

	struct BeamOrigin : public Vector
	{
		bool forceRecreate = false;
	};

	class BeamBuffer : public CFixedSizeCircularBuffer<BeamOrigin, originHistorySize>
	{
	public:
		virtual void ElementAlloc(BeamOrigin &element) {};
		virtual void ElementRelease(BeamOrigin &element) {};

		void Clear()
		{
			Advance(originHistorySize);
		}
	};

	struct PlayerBeam
	{
		CEntityHandle handle;
		Vector lastOrigin;
		bool moving = true;
	};

	struct InstantBeam
	{
		constexpr static i32 maxLingerTicks = 4;
		CEntityHandle handle;
		Vector start;
		Vector end;
		i32 tickRemaining;
		i32 totalTicks;
		i32 tickLingered = 0;
	};

	inline bool operator==(const InstantBeam &lhs, const InstantBeam &rhs)
	{
		return lhs.handle == rhs.handle;
	}

} // namespace KZ::beam

class KZBeamService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

	virtual void Reset();
	static void UpdateBeams();
	void Update();
	KZ::beam::PlayerBeam playerBeam;

	static inline const Vector defaultOffset {0.0f, 0.0f, 1.75f};
	Vector playerBeamOffset {defaultOffset};

	KZPlayer *target {};
	KZ::beam::BeamBuffer buffer;

	enum BeamType : u8
	{
		BEAM_NONE = 0,
		BEAM_GROUND,
		BEAM_FEET,
		BEAM_COUNT
	} desiredBeamType;

	i32 beamStopTick = 0;
	bool canResumeBeam {};
	i32 noclipTick = 0;

	void SetBeamType(u8 type)
	{
		desiredBeamType = (BeamType)type;
	}

	void UpdatePlayerBeam();

	bool teleportedThisTick = false;

	void OnTeleport()
	{
		teleportedThisTick = true;
	}

	void OnPlayerPreferencesLoaded();

	CUtlVector<KZ::beam::InstantBeam> instantBeams;

	void AddInstantBeam(const Vector &start, const Vector &end, u32 lifetime);
	void UpdateInstantBeams();
};
