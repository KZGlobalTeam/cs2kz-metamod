#include "route.h"
#include <algorithm>
#include <cmath>

namespace KZ::progress
{
	bool ValidPosition(const Vector &v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x) < 10000000 && std::abs(v.y) < 10000000
			   && std::abs(v.z) < 10000000;
	}

	Cell ToCell(const Vector &v)
	{
		return {(i32)std::floor(v.x / 128), (i32)std::floor(v.y / 128), (i32)std::floor(v.z / 128)};
	}

	static f32 Projection(const Vector &p, const Vector &a, const Vector &b)
	{
		Vector ab = b - a;
		return ab.LengthSqr() > 0.0001f ? std::clamp((p - a).Dot(ab) / ab.LengthSqr(), 0.0f, 1.0f) : 0.0f;
	}

	bool Route::Build(std::vector<Point> input)
	{
		points.clear();
		segments.clear();
		cells.clear();
		totalLength = 0;
		if (input.size() < 2 || input.size() > 500000)
		{
			return false;
		}
		for (const auto &p : input)
		{
			if (!ValidPosition(p.origin))
			{
				return false;
			}
		}

		// Iterative RDP, independently per continuous section. Preserve endpoints and keep
		// every deviation above one unit (including vertical jump/ladder curvature).
		std::vector<u8> keep(input.size(), 0);
		std::vector<std::pair<size_t, size_t>> pending;
		size_t first = 0;
		for (size_t i = 1; i <= input.size(); ++i)
		{
			if (i < input.size() && !input[i].breakBefore)
			{
				continue;
			}
			keep[first] = keep[i - 1] = 1;
			pending.emplace_back(first, i - 1);
			first = i;
		}
		while (!pending.empty())
		{
			auto range = pending.back();
			pending.pop_back();
			const auto a = range.first, b = range.second;
			f32 error = 1.0f;
			size_t split = a;
			// Limit RDP's worst case on noisy input: split long ranges before measuring.
			if (b - a > 512)
			{
				split = a + (b - a) / 2;
			}
			else
			{
				for (size_t i = a + 1; i < b; ++i)
				{
					Vector projected =
						input[a].origin + (input[b].origin - input[a].origin) * Projection(input[i].origin, input[a].origin, input[b].origin);
					f32 e = (input[i].origin - projected).LengthSqr();
					if (e > error)
					{
						error = e;
						split = i;
					}
				}
			}
			if (split != a)
			{
				keep[split] = 1;
				pending.emplace_back(a, split);
				pending.emplace_back(split, b);
			}
		}
		u32 section = 0;
		mins = maxs = input.front().origin;
		for (size_t i = 0; i < input.size(); ++i)
		{
			if (!keep[i])
			{
				continue;
			}
			Point p = input[i];
			for (i32 axis = 0; axis < 3; ++axis)
			{
				mins[axis] = (std::min)(mins[axis], p.origin[axis]);
				maxs[axis] = (std::max)(maxs[axis], p.origin[axis]);
			}
			if (points.empty() || p.breakBefore)
			{
				if (!points.empty())
				{
					++section;
				}
				points.push_back(p);
				continue;
			}
			Vector a = points.back().origin;
			f32 length = (p.origin - a).Length();
			if (length < 0.001f)
			{
				continue;
			}
			// Bounded segment lengths give stable local windows and bounded cell insertion.
			i32 count = (i32)std::ceil(length / 64.0f);
			if (segments.size() + count > 200000)
			{
				return false;
			}
			for (i32 part = 1; part <= count; ++part)
			{
				Vector end = a + (p.origin - a) * ((f32)part / count);
				Vector start = points.back().origin;
				f32 len = (end - start).Length();
				Segment s {start, end, len, totalLength, section};
				u32 index = (u32)segments.size();
				segments.push_back(s);
				points.push_back({end, false});
				totalLength += len;
				Cell ca = ToCell(start), cb = ToCell(end);
				for (i32 x = (std::min)(ca.x, cb.x); x <= (std::max)(ca.x, cb.x); ++x)
				{
					for (i32 y = (std::min)(ca.y, cb.y); y <= (std::max)(ca.y, cb.y); ++y)
					{
						for (i32 z = (std::min)(ca.z, cb.z); z <= (std::max)(ca.z, cb.z); ++z)
						{
							cells[{x, y, z}].push_back(index);
						}
					}
				}
			}
		}
		return !segments.empty() && std::isfinite(totalLength) && totalLength > 1.0;
	}

	bool FindMatch(const Route &r, const Vector &p, const Vector &velocity, const Vector &lastPosition, const Match &previous, bool reacquire,
				   bool teleported, Match &result, f32 maxDistance)
	{
		result = {};
		if (!ValidPosition(p) || !ValidPosition(velocity) || r.segments.empty())
		{
			return false;
		}
		maxDistance = std::isfinite(maxDistance) ? std::clamp(maxDistance, 64.0f, 768.0f) : 384.0f;
		const bool known = previous.segment >= 0 && (size_t)previous.segment < r.segments.size();
		const f32 travel = known && ValidPosition(lastPosition) ? (p - lastPosition).Length() : 0;
		const f64 window = (std::max)(128.0f, (std::min)(1024.0f, travel * 3 + 64));
		const bool falling = velocity.z < -80 || (known && lastPosition.z - p.z > 2);
		i32 expectedSection = known ? (i32)r.segments[previous.segment].section : -1;
		if (known && teleported)
		{
			for (i32 i = (std::max)(0, previous.segment - 48); i < (std::min)((i32)r.segments.size() - 1, previous.segment + 49); ++i)
			{
				const auto &a = r.segments[i];
				const auto &b = r.segments[i + 1];
				if (a.section != b.section && (a.end - lastPosition).Length() < 192 && (b.start - p).Length() < 192)
				{
					expectedSection = (i32)b.section;
					break;
				}
			}
		}

		struct Search
		{
			Match match;
			f64 score = 1e30, runner = 1e30;

			void Add(u32 index, f64 distance, f32 d2, f64 cost)
			{
				if (cost < score)
				{
					if (match.segment >= 0 && std::abs(distance - match.distance) > 256)
					{
						runner = (std::min)(runner, score);
					}
					score = cost;
					match.segment = (i32)index;
					match.distance = distance;
					match.distanceToRoute = std::sqrt(d2);
				}
				else if (std::abs(distance - match.distance) > 256)
				{
					runner = (std::min)(runner, cost);
				}
			}
		} local, global;

		u32 evaluated = 0;
		auto consider = [&](u32 index, bool continuity)
		{
			++evaluated;
			const Segment &s = r.segments[index];
			const f32 t = Projection(p, s.start, s.end);
			const f64 along = s.distance + s.length * t;
			const f64 delta = known ? std::abs(along - previous.distance) : 0;
			if (continuity && (s.section != (u32)expectedSection || delta > window))
			{
				return;
			}
			const f32 d2 = (p - (s.start + (s.end - s.start) * t)).LengthSqr();
			if (d2 > maxDistance * maxDistance)
			{
				return;
			}
			f64 cost = d2 + (continuity ? delta * delta * 0.015 : (std::min)(delta, 2048.0) * 0.02);
			if (teleported && known && (i32)s.section != expectedSection)
			{
				cost += 1024;
			}
			f32 speed = velocity.Length();
			if (known && !teleported && speed > 30 && delta > 1)
			{
				f32 alignment = velocity.Dot(s.end - s.start) / (speed * s.length);
				if (along < previous.distance)
				{
					alignment = -alignment;
				}
				cost += 16 * (1 - std::clamp(alignment, -1.0f, 1.0f));
			}
			(continuity ? local : global).Add(index, along, d2, cost);
		};
		auto choose = [&](const Match &m)
		{
			result = m;
			result.evaluated = evaluated;
			return result.segment >= 0;
		};
		if (known && !teleported)
		{
			for (i32 i = (std::max)(0, previous.segment - 48); i <= (std::min)((i32)r.segments.size() - 1, previous.segment + 48); ++i)
			{
				consider((u32)i, true);
			}
			// Near the established path, history resolves crossings and stacked floors.
			// Outside this narrow corridor, compare geometry without a hard arc window.
			if (!reacquire && local.match.segment >= 0 && local.match.distanceToRoute <= 32)
			{
				return choose(local.match);
			}
		}
		Cell cell = ToCell(p);
		const i32 radius = (i32)std::ceil(maxDistance / 128);
		for (i32 x = cell.x - radius; x <= cell.x + radius; ++x)
		{
			for (i32 y = cell.y - radius; y <= cell.y + radius; ++y)
			{
				for (i32 z = cell.z - radius; z <= cell.z + radius; ++z)
				{
					auto found = r.cells.find({x, y, z});
					if (found == r.cells.end())
					{
						continue;
					}
					for (u32 index : found->second)
					{
						if (evaluated >= 4096)
						{
							// An incomplete spatial query cannot justify switching branches.
							result.evaluated = evaluated;
							return local.match.segment >= 0 && local.match.distanceToRoute <= 96 && choose(local.match);
						}
						consider(index, false);
					}
				}
			}
		}
		const bool unambiguous = global.match.segment >= 0 && global.runner - global.score >= 64;
		if (local.match.segment >= 0)
		{
			// A fall or a shortcut rejoin can change route distance by thousands of units.
			// Require a clearly closer branch, retaining history at indistinguishable crossings.
			const f32 a = local.match.distanceToRoute, b = global.match.distanceToRoute;
			const f32 margin = falling ? 12.0f : 32.0f;
			if (unambiguous && b <= 96 && b + margin < a && b < a * 0.6f)
			{
				return choose(global.match);
			}
			return choose(local.match);
		}
		if (!unambiguous)
		{
			result.evaluated = evaluated;
			return false;
		}
		return choose(global.match);
	}
} // namespace KZ::progress
