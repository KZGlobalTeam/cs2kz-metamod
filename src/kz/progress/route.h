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
		// Finds the closest route position between behind and ahead of a known one.
		bool Follow(const Vector &position, f64 from, f64 behind, f64 ahead, f64 &distance, f32 &error) const;
		// Finds a route position anywhere. Overlapping parts of the route resolve to the one closest to hint, or fail without a hint.
		bool Locate(const Vector &position, f64 hint, f64 &distance, f32 &error) const;
	};

	// Follows one player along a route and decides what to display.
	struct Tracker
	{
		void Reset();
		// The run starts at the start of the route.
		void Start();
		// The next position found is displayed as is.
		void OnTeleport();
		// Returns false when there is nothing to display.
		bool Update(const Route &route, const Vector &position, f64 now, f64 &distance, bool &approximate);

	private:
		// Last route position found for the player.
		f64 following {-1};
		// The displayed value moves from shown towards target.
		f64 target {-1};
		f64 shown {-1};
		f64 divergedSince {-1};
		f64 lastMatch {-1};
		f32 travelSinceTarget {};
		Vector lastPosition;
		bool hasLastPosition {};
		bool acceptNext {};
		bool approximate {};
	};
} // namespace KZ::progress
