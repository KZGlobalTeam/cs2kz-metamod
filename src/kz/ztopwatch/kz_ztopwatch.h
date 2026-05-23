#pragma once
#include "kz/kz.h"

#define KZ_ZONE_SW_LINE_PARTICLE "particles/ui/annotation/ui_annotation_line_segment.vpcf"

class KZZtopwatchService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	struct Zone
	{
		Vector point1 {};
		Vector point2 {};
		Vector mins {};
		Vector maxs {};
		bool point1Active {};
		bool point2Active {};

		static constexpr int NUM_EDGES = 12;
		CEntityHandle edges[NUM_EDGES] {};

		bool IsValid() const
		{
			return point1Active && point2Active;
		}

		void SetMinsMaxs()
		{
			for (int i = 0; i < 3; i++)
			{
				mins[i] = Min(point1[i], point2[i]);
				maxs[i] = Max(point1[i], point2[i]);
			}
		}

		void Reset()
		{
			point1Active = false;
			point2Active = false;
			point1 = vec3_origin;
			point2 = vec3_origin;
			mins = vec3_origin;
			maxs = vec3_origin;
		}
	};

	Zone startZone {};
	Zone endZone {};

	f32 startTime {};
	bool enabled {};
	bool startOnJump {};
	bool stopOnLand {};
	u8 nextPoint {}; // cycles 0-3: start1, start2, end1, end2

	static void Init();
	virtual void Reset() override;

	void Toggle();
	void PlaceNextPoint();
	void ToggleStartOnJump();
	void ToggleStopOnLand();
	void ResetZones();

	void OnProcessMovementPost();
	void OnJump();
	void OnTimerStart();
	void OnTimerStop();

private:
	bool GetLookAtPos(Vector &outPos);
	bool SetZonePoint(Zone &zone, bool isPoint1);
	void UpdateZoneVisual(Zone &zone, const Color &color);
	void RemoveZoneVisual(Zone &zone);
	bool IsPlayerInZone(const Zone &zone);
	void TryStartTimer();
	void TryStopTimer();
};
