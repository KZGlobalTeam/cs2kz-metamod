#include "kz/progress/route.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <chrono>

using namespace KZ::progress;
static i32 checks;

static void Check(bool b, const char *message)
{
	++checks;
	if (!b)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

int main()
{
	Route r;
	Check(!r.Build({}), "empty route rejected");
	Check(!r.Build({{{0, 0, 0}, false}, {{0, 0, 0}, false}}), "zero length rejected");
	Check(!r.Build({{{0, 0, 0}, false}, {{std::numeric_limits<f32>::quiet_NaN(), 0, 0}, false}}), "NaN rejected");
	Check(r.Build({{{0, 0, 0}, false}, {{100, 0, 0}, false}, {{100, 100, 0}, false}, {{0, 100, 0}, false}}), "U-shaped route");
	Check(std::abs(r.totalLength - 300) < 0.001, "arc length, not endpoint distance");
	Match m, next;
	Check(FindMatch(r, {50, 10, 0}, {10, 0, 0}, {0, 0, 0}, m, true, false, next), "global projection");
	Check(std::abs(next.distance - 50) < 0.001, "segment projection, not nearest vertex");
	m = next;
	Check(FindMatch(r, {80, 0, 0}, {100, 0, 0}, {50, 10, 0}, m, false, false, next) && next.distance > m.distance, "forward");
	m = next;
	Check(FindMatch(r, {30, 0, 0}, {-100, 0, 0}, {80, 0, 0}, m, false, false, next) && next.distance < m.distance, "backward");
	Check(!FindMatch(r, {10000, 10000, 10000}, {0, 0, 0}, {0, 0, 0}, m, true, false, next), "off-route hidden");
	Check(!FindMatch(r, {std::numeric_limits<f32>::infinity(), 0, 0}, {0, 0, 0}, {0, 0, 0}, m, true, false, next),
		  "infinite player position rejected");
	Check(r.Build({{{0, 0, 0}, false}, {{100, 0, 0}, false}, {{10000, 0, 0}, true}, {{10100, 0, 0}, false}}), "teleporter route");
	Check(std::abs(r.totalLength - 200) < 0.001, "teleport world distance excluded");
	m = {};
	m.segment = 1;
	m.distance = 99;
	Check(FindMatch(r, {10001, 0, 0}, {0, 0, 0}, {99, 0, 0}, m, true, true, next) && std::abs(next.distance - 101) < 0.01,
		  "map teleport destination continuity");
	Check(r.Build({{{0, 0, 0}, false}, {{1000, 0, 0}, false}, {{1000, 0, 40}, false}, {{0, 0, 40}, false}}), "stacked floors");
	m = {};
	m.segment = 1;
	m.distance = 90;
	Check(FindMatch(r, {100, 0, 28}, {200, 0, 0}, {90, 0, 0}, m, false, false, next) && next.distance < 200,
		  "history holds lower floor even if upper is nearer");
	m = {};
	m.segment = 1;
	m.distance = 90;
	Check(FindMatch(r, {90, 0, 0}, {0, 0, 0}, {800, 0, 40}, m, false, false, next) && next.distance < 200, "restored CP anchor holds saved floor");
	// An exact crossing cannot be resolved without history: never fabricate 20% vs 80%.
	Check(r.Build({{{-100, 0, 0}, false}, {{100, 0, 0}, false}, {{100, 1000, 0}, false}, {{0, 1000, 0}, false}, {{0, -100, 0}, false}}),
		  "crossing route");
	m = {};
	Check(!FindMatch(r, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, m, true, true, next), "ambiguous crossing hidden");

	// Regression: a non-teleport fall from a late floor to an early one must shed history.
	for (f32 height : {40.0f, 160.0f, 512.0f})
	{
		Check(r.Build({{{0, 0, 0}, false}, {{4096, 0, 0}, false}, {{4096, 0, height}, false}, {{0, 0, height}, false}}), "fall route builds");
		m = {};
		Vector previous {200, 0, height};
		Check(FindMatch(r, previous, {0, 0, 0}, previous, m, true, false, next), "acquire late upper floor");
		m = next;
		Check(m.distance > r.totalLength * 0.8, "initial late-floor progress");
		for (f32 z = height - 5; z >= 0; z -= 5)
		{
			Vector pos {200, 0, z};
			if (FindMatch(r, pos, {0, 0, -200}, previous, m, false, false, next))
			{
				m = next;
			}
			previous = pos;
		}
		Check(FindMatch(r, {200, 0, 0}, {0, 0, 0}, previous, m, false, false, next) && std::abs(next.distance - 200) < 1,
			  "landing immediately restores early progress without teleport");
		m = next;
		previous = {200, 0, 0};
		for (f32 x = 210; x <= 600; x += 10)
		{
			Check(FindMatch(r, {x, 0, 0}, {200, 0, 0}, previous, m, false, false, next) && next.distance > m.distance,
				  "after fall progress rises immediately, not down then up");
			m = next;
			previous = {x, 0, 0};
		}
	}
	Check(r.Build({{{0, 0, 0}, false}, {{0, 2000, 0}, false}, {{2000, 2000, 0}, false}, {{2000, 0, 0}, false}}), "long detour route");
	m = {};
	m.segment = 1;
	m.distance = 100;
	Check(FindMatch(r, {0, 100, 0}, {0, 200, 0}, {0, 100, 0}, m, false, false, next), "shortcut initial anchor");
	m = next;
	Check(!FindMatch(r, {1000, 1000, 0}, {200, 0, 0}, {900, 1000, 0}, m, true, false, next), "distant shortcut gap remains unknown");
	Check(FindMatch(r, {2000, 1000, 0}, {0, -200, 0}, {1990, 1000, 0}, m, true, false, next) && std::abs(next.distance - 5000) < 1,
		  "shortcut rejoin escapes obsolete arc window");
	m = next;
	Check(FindMatch(r, {2000, 980, 0}, {0, -200, 0}, {2000, 1000, 0}, m, false, false, next) && next.distance > m.distance,
		  "shortcut rejoin continues increasing");
	Check(FindMatch(r, {2300, 980, 0}, {0, -200, 0}, {2000, 980, 0}, next, false, false, m) && m.distanceToRoute > 256,
		  "nearby alternative line within configured corridor stays visible");
	Check(!FindMatch(r, {2300, 980, 0}, {0, -200, 0}, {2000, 980, 0}, next, true, false, m, 128), "configured corridor limit enforced");

	// 50k non-collinear points; test actual matcher work, not a model of it.
	std::vector<Point> points;
	for (i32 i = 0; i < 50000; ++i)
	{
		points.push_back({Vector((f32)i * 16, (i % 2) ? 8.0f : 0.0f, 0), false});
	}
	Check(r.Build(std::move(points)), "50k point route builds");
	m = {};
	m.segment = 25000;
	m.distance = r.segments[25000].distance;
	Vector p = r.segments[25000].start;
	auto begin = std::chrono::steady_clock::now();
	for (i32 i = 0; i < 64000; ++i)
	{
		Check(FindMatch(r, p, {100, 0, 0}, p, m, false, false, next), "64-player local tracking");
		Check(next.evaluated <= 97, "local search bounded to 97 segments");
	}
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
	m = {};
	Check(FindMatch(r, p, {100, 0, 0}, p, m, true, true, next) && next.evaluated < 4096, "spatial reacquire avoids whole route");
	std::printf("PASS: %d checks; 64000 updates in %lld ms; %zu segments; reacquire %u projections\n", checks, (long long)ms, r.segments.size(),
				next.evaluated);
}
