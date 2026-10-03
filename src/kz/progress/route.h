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
		size_t operator()(const Cell &cell) const
		{
			return (size_t)(u32)cell.x * 73856093u ^ (size_t)(u32)cell.y * 19349663u ^ (size_t)(u32)cell.z * 83492791u;
		}
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
		f64 length {};
		bool Build(const replaysystem::data::ReplayMovement &replay, i32 courseID, const std::atomic<bool> &cancel);
		bool Match(const Vector &position, f64 previousDistance, f32 travel, f64 &distance, bool &approximate) const;
	};
} // namespace KZ::progress
