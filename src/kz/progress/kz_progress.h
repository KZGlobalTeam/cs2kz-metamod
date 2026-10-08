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
	// Clear matching history and completed-course state for this player.
	void Reset() override;
	// Refresh the position estimate at the player's staggered interval.
	void OnPhysicsSimulatePost();
	// Discard continuity when the player's position is teleported.
	void OnTeleport();
	// Hide the finished course until a new run starts or another course is selected.
	void OnTimerEnd(u32 courseGUID);
	// Read the cached estimate without querying records or files.
	bool GetProgress(f32 &value, bool &estimate) const;
	// Subscribe to record-cache and replay-file changes.
	static void Init();
	// Unsubscribe and join the private reader before unloading.
	static void Cleanup();
	// Publish a completed reference load on the main thread.
	static void OnGameFrame();
	// Cancel the previous map's reader and clear the route cache.
	static void OnMapChange();
};
