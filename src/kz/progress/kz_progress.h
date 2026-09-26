#pragma once
#include "kz/kz.h"
#include "route.h"
#include <memory>

namespace KZ::progress
{
	void Init();
	void Shutdown();
	void Clear();
	void OnMapReady();
	void OnGameFrame();
	void Reload();
	std::shared_ptr<const Route> GetRoute(u32 courseGUID, const char *mode);
	std::string HUDText(KZPlayer *viewer, KZPlayer *source);
} // namespace KZ::progress

class KZProgressService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	void Reset() override;
	void EnterStart(u32 courseGUID);
	void Start(u32 courseGUID, bool fromStart = true);
	void Finish(u32 courseGUID);
	void Stop();
	void Update(bool force = false);
	void OnTeleport(const Vector *origin);
	KZ::progress::Anchor SaveAnchor();
	void RestoreAnchor(const KZ::progress::Anchor &anchor);
	bool GetDisplay(f64 &value) const;
	void PrintDebug();

	bool IsApproximate() const
	{
		return approximate;
	}

private:
	std::shared_ptr<const KZ::progress::Route> route;
	u32 courseGUID {};
	KZ::progress::Match match;
	Vector lastPosition = vec3_origin;
	f64 rawProgress {}, displayedProgress {}, nextUpdate {}, stoppedAt {}, lastMatchedAt {};
	bool initialized {}, running {}, finished {}, atStart {}, teleported {}, reacquire = true, visible {};
	u32 reacquireCount {};
	bool approximate {};
};
