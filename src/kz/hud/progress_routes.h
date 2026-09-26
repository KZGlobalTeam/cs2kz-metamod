#pragma once
#include "progress/route.h"
#include <memory>

namespace KZ::progress
{
	void Clear();
	void OnMapReady();
	void OnGameFrame();
	void Reload();
	void InvalidateRoutes();
	std::shared_ptr<const Route> GetRoute(u32 courseGUID, const char *mode);
} // namespace KZ::progress
