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

		bool operator==(const Cell &other) const
		{
			return x == other.x && y == other.y && z == other.z;
		}
	};

	struct CellHash
	{
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
		// Ordered by distance along the route.
		std::vector<Segment> segments;
		std::unordered_map<Cell, std::vector<size_t>, CellHash> cells;
		// Segments too long for the grid are projected on every lookup instead.
		std::vector<size_t> longSegments;
		f64 length {};

		// Reads the completed run of a v6 replay into segments and the spatial index.
		bool Build(const replaysystem::data::ReplayMovement &replay, i32 courseID, const std::atomic<bool> &cancel);
	};

} // namespace KZ::progress
