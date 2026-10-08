#pragma once
#include "common.h"
#include "mathlib/vector.h"
#include "kz/replays/data.h"
#include <unordered_map>

namespace KZ::progress
{
	struct Cell
	{
		i32 x, y, z;

		// Identifies the same spatial-index bucket.
		bool operator==(const Cell &other) const
		{
			return x == other.x && y == other.y && z == other.z;
		}
	};

	struct CellHash
	{
		// Hashes signed grid coordinates without changing their equality semantics.
		size_t operator()(const Cell &cell) const;
	};

	struct Segment
	{
		Vector start, end;
		f64 distance;
		f32 length;
	};

	struct Route
	{
		std::vector<Segment> segments;
		std::unordered_map<Cell, std::vector<size_t>, CellHash> cells;
		// Long movement segments are kept once and projected separately, so one
		// long tick cannot expand every grid query across the map.
		std::vector<size_t> longSegments;
		f64 length {};
		// Reads a completed v6 run into a retained path and spatial index. Checkpoint
		// restores and known failure returns remove discarded branches; uncertain
		// map teleports and intentional repeated geometry remain. No replay is changed.
		bool Build(const replaysystem::data::ReplayMovement &replay, i32 courseID, const std::atomic<bool> &cancel);
		// Projects a position to route distance. History resolves only bounded
		// ambiguity; falls may move backward. Returns false for missing/ambiguous
		// geometry or exhausted work, so the HUD can hide an unreliable estimate.
		bool Match(const Vector &position, f64 previousDistance, f32 travel, f64 &distance, bool &approximate) const;
	};
} // namespace KZ::progress
