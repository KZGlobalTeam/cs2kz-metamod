#pragma once
#include "route.h"
#include <atomic>
#include <memory>

namespace KZ::progress
{
	struct ReplayCandidate
	{
		std::string path, uuid, mapName, mapMD5, courseName, mode;
		i32 courseID {};
		u32 courseGUID {};
		f32 time {};
	};

	std::shared_ptr<Route> ReadReferenceReplay(const ReplayCandidate &candidate, const std::atomic<bool> &cancel);
} // namespace KZ::progress
