#pragma once
#include "common.h"
#include "mathlib/vector.h"
#include <vector>
#include <unordered_map>
#include <string>

namespace KZ::progress
{
	struct Point
	{
		Vector origin;
		bool breakBefore {};
	};

	struct Segment
	{
		Vector start, end;
		f32 length {};
		f64 distance {};
		u32 section {};
	};

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
		size_t operator()(const Cell &c) const
		{
			return (size_t)(u32)c.x * 73856093u ^ (size_t)(u32)c.y * 19349663u ^ (size_t)(u32)c.z * 83492791u;
		}
	};

	struct Route
	{
		std::vector<Point> points;
		std::vector<Segment> segments;
		std::unordered_map<Cell, std::vector<u32>, CellHash> cells;
		Vector mins = vec3_origin, maxs = vec3_origin;
		f64 totalLength {};
		u64 generation {};
		u32 courseGUID {};
		i32 courseID {};
		std::string courseName, mode, source;
		bool Build(std::vector<Point> input);
	};

	// Stored inside CS2KZ checkpoints, never in a second player database.
	struct Anchor
	{
		u64 generation {};
		u32 courseGUID {};
		i32 segment = -1;
		f64 distance {};
	};

	struct Match
	{
		i32 segment = -1;
		f64 distance {};
		f32 distanceToRoute {};
		u32 evaluated {};
	};

	bool ValidPosition(const Vector &v);
	Cell ToCell(const Vector &v);
	bool FindMatch(const Route &route, const Vector &position, const Vector &velocity, const Vector &lastPosition, const Match &previous,
				   bool reacquire, bool teleported, Match &result, f32 maxDistance = 384.0f);
} // namespace KZ::progress
