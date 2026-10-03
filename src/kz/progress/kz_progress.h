#pragma once
#include "kz/kz.h"

class KZProgressService : public KZBaseService
{
	using KZBaseService::KZBaseService;
	u64 routeGeneration {};
	u32 completedCourse {};
	f64 nextUpdate {};
	f64 distance = -1;
	f32 percentage {};
	Vector lastPosition = vec3_origin;
	bool visible {};
	bool approximate {};

public:
	void Reset() override;
	void OnPhysicsSimulatePost();
	void OnTeleport();
	void OnTimerEnd(u32 courseGUID);
	bool GetProgress(f32 &value, bool &estimate) const;
	static void Init();
	static void Cleanup();
	static void OnGameFrame();
	static void ClearRoutes();
};
