#pragma once
#include "../kz.h"

class KZMeasureService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	struct MeasurePos
	{
		bool IsValid() const
		{
			return origin != vec3_invalid && normal != vec3_invalid;
		}

		void Invalidate()
		{
			origin = vec3_invalid;
			normal = vec3_invalid;
		}

		Vector origin {};
		Vector normal {};
	};

	f32 startPosSetTime {};
	MeasurePos startPos {};
	f32 lastMeasureTime {};
	CEntityHandle measurerHandle {};

	virtual void Reset() override
	{
		startPosSetTime = {};
		startPos.Invalidate();
		lastMeasureTime = {};
		measurerHandle = {};
	}

	void TryMeasure();
	void StartMeasure();
	// Return false if MeasureBlock should be called instead.
	bool EndMeasure(f32 minDistThreshold = -1.0f);
	void MeasureBlock();
	MeasurePos GetLookAtPos(const Vector *overrideOrigin = nullptr, const QAngle *overrideAngles = nullptr);

	void OnPhysicsSimulatePost();

	// Calculates the minimum equivalent jumpstat distance to go between the two points
	static f32 GetEffectiveDistance(Vector pointA, Vector pointB);
};
