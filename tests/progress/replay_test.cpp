#include "kz/hud/progress/replay_route.h"
#include "kz/replays/compression.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <limits>

// Standalone executable supplies only the plugin logging-channel bridge.
// Geometry, replay encoding and decoding are the production implementations.
LoggingChannelID_t GetServiceChannel(LogChannel)
{
	return LoggingSystem_FindChannel("General");
}

const char *GetServiceChannelName(LoggingChannelID_t)
{
	return "ProgressTests";
}

using namespace KZ::progress;
using namespace KZ::replaysystem::compression;
static int checks;

static void Check(bool b, const char *message)
{
	++checks;
	if (!b)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

static ReplayHeader Header()
{
	ReplayHeader h;
	h.set_version(KZ_REPLAY_VERSION);
	h.set_type(cs2kz::replay::RP_RUN);
	h.mutable_map()->set_name("kz_test");
	h.mutable_map()->set_md5("01234567890123456789012345678901");
	h.mutable_run()->set_course_name("Main");
	h.mutable_run()->mutable_mode()->set_name("Classic");
	h.mutable_run()->set_time(3);
	h.mutable_run()->set_num_teleports(0);
	return h;
}

static RpEvent Timer(u32 tick, RpEvent::RpEventData::TimerEvent::TimerEventType type, f32 time)
{
	RpEvent e {};
	e.type = RPEVENT_TIMER_EVENT;
	e.serverTick = tick;
	e.data.timer.type = type;
	e.data.timer.index = 1;
	e.data.timer.time = time;
	return e;
}

static void Write(const std::string &path, const std::vector<char> &data)
{
	std::ofstream f(path, std::ios::binary);
	f.write(data.data(), (std::streamsize)data.size());
}

static std::vector<char> Encode(const ReplayHeader &h, const std::vector<TickData> &ticks, const std::vector<RpEvent> &events)
{
	std::string header = h.SerializeAsString();
	u32 size = (u32)header.size();
	std::vector<char> bytes((char *)&size, (char *)&size + sizeof(size));
	bytes.insert(bytes.end(), header.begin(), header.end());
	std::vector<SubtickData> subticks(ticks.size());
	WriteTickDataCompressed(bytes, ticks, subticks);
	WriteWeaponsCompressed(bytes, {});
	WriteJumpsCompressed(bytes, {});
	WriteEventsCompressed(bytes, events);
	return bytes;
}

int main(int argc, char **argv)
{
	std::atomic<bool> cancel {false};
	// Read-only inspection of a real server recording; no player identity is printed.
	if (argc == 4 && (std::string(argv[1]) == "--inspect" || std::string(argv[1]) == "--audit-falls"))
	{
		std::ifstream input(argv[2], std::ios::binary);
		u32 size = 0;
		if (!input.read((char *)&size, sizeof(size)) || size == 0 || size > 5 * 1024 * 1024)
		{
			return 2;
		}
		std::string data(size, '\0');
		ReplayHeader h;
		if (!input.read(data.data(), size) || !h.ParseFromString(data) || !h.has_run())
		{
			return 2;
		}
		ReplayCandidate candidate {
			argv[2], "local-inspection", h.map().name(), h.map().md5(), h.run().course_name(), h.run().mode().name(), std::atoi(argv[3]),
			1,       h.run().time()};
		auto route = ReadReferenceReplay(candidate, cancel);
		std::printf("%s: map=%s course=%s version=%u teleports=%d styles=%d points=%zu length=%.2f\n", route ? "ACCEPT" : "REJECT",
					h.map().name().c_str(), h.run().course_name().c_str(), h.version(), h.run().num_teleports(), h.run().styles_size(),
					route ? route->points.size() : 0, route ? route->totalLength : 0);
		std::printf("mode=%s time=%.3f mapMD5=%s\n", h.run().mode().name().c_str(), h.run().time(), h.map().md5().c_str());
		if (route && std::string(argv[1]) == "--audit-falls")
		{
			u32 tested = 0, passed = 0;
			for (u32 late = 1; late < route->segments.size(); ++late)
			{
				const auto &upper = route->segments[late];
				for (u32 early = 0; early < late; ++early)
				{
					const auto &lower = route->segments[early];
					Vector delta = lower.start - upper.start;
					if (upper.distance - lower.distance < 1024 || delta.Length2D() > 48 || delta.z > -40 || delta.z < -768)
					{
						continue;
					}
					Match check, history, next;
					if (!FindMatch(*route, lower.start, vec3_origin, lower.start, history, true, false, check)
						|| std::abs(check.distance - lower.distance) > 64)
					{
						continue;
					}
					history.segment = (i32)late;
					history.distance = upper.distance;
					Vector last = upper.start;
					bool lost = false;
					i32 steps = (i32)std::ceil(delta.Length() / 8);
					for (i32 step = 1; step <= steps; ++step)
					{
						Vector position = upper.start + delta * ((f32)step / steps);
						lost = !FindMatch(*route, position, delta * (20.0f / steps), last, history, lost, false, next);
						if (!lost)
						{
							history = next;
						}
						last = position;
					}
					++tested;
					if (!lost && std::abs(history.distance - lower.distance) <= 128)
					{
						++passed;
					}
					else
					{
						std::printf("FALL mismatch: upper=%u lower=%u expected=%.2f actual=%.2f lost=%d\n", late, early, lower.distance,
									history.distance, lost);
					}
				}
			}
			std::printf("Actual replay geometry, simulated falling trajectories: %u/%u passed\n", passed, tested);
			return tested && passed == tested ? 0 : 4;
		}
		return route ? 0 : 3;
	}
	ReplayCandidate c {"progress-fixture.replay", "test-uuid", "kz_test", "01234567890123456789012345678901", "Main", "Classic", 1, 42, 3};
	auto header = Header();
	std::vector<TickData> ticks(6);
	for (u32 i = 0; i < ticks.size(); ++i)
	{
		ticks[i].serverTick = 99 + i;
		ticks[i].pre.origin = Vector((f32)i * 10, 0, 0);
		ticks[i].post.origin = Vector((f32)i * 10 + 5, 0, 0);
	}
	using TE = RpEvent::RpEventData::TimerEvent;
	std::vector<RpEvent> events {Timer(96, TE::TIMER_START, 0), Timer(98, TE::TIMER_STOP, 2), Timer(100, TE::TIMER_START, 0),
								 Timer(103, TE::TIMER_END, 3), Timer(104, TE::TIMER_START, 0)};
	auto bytes = Encode(header, ticks, events);
	Write(c.path, bytes);
	auto r = ReadReferenceReplay(c, cancel);
	Check(r && std::abs(r->totalLength - 35) < 0.01, "breather trimmed, previous failed run ignored");
	Check(r && r->courseGUID == 42 && r->courseID == 1 && r->mode == "Classic", "actual course identifiers and mode retained");
	ReplayCandidate wrong = c;
	wrong.courseID = 2;
	Check(!ReadReferenceReplay(wrong, cancel), "bonus never consumes main replay");
	wrong = c;
	wrong.mapMD5 = "changed";
	Check(!ReadReferenceReplay(wrong, cancel), "map revision mismatch rejected");
	wrong = c;
	wrong.mode = "Vanilla";
	Check(!ReadReferenceReplay(wrong, cancel), "mode mismatch rejected");
	cancel = true;
	Check(!ReadReferenceReplay(c, cancel), "cancel during unload");
	cancel = false;
	header.mutable_run()->set_num_teleports(1);
	Write(c.path, Encode(header, ticks, events));
	Check(!ReadReferenceReplay(c, cancel), "checkpoint routes rejected");
	header = Header();
	header.mutable_run()->add_styles()->set_name("AutoBhop");
	Write(c.path, Encode(header, ticks, events));
	Check(!ReadReferenceReplay(c, cancel), "style routes rejected");
	header = Header();
	auto noEnd = events;
	noEnd.erase(noEnd.begin() + 3);
	Write(c.path, Encode(header, ticks, noEnd));
	Check(!ReadReferenceReplay(c, cancel), "missing finish rejected");
	header.mutable_run()->set_timer_valid(false);
	Write(c.path, Encode(header, ticks, events));
	Check(!ReadReferenceReplay(c, cancel), "explicitly invalid timer rejected");
	header.mutable_run()->set_timer_valid(true);
	Write(c.path, Encode(header, ticks, events));
	Check(!!ReadReferenceReplay(c, cancel), "explicitly valid timer accepted");
	auto badTicks = ticks;
	badTicks[2].post.origin.x = std::numeric_limits<f32>::quiet_NaN();
	Write(c.path, Encode(header, badTicks, events));
	Check(!ReadReferenceReplay(c, cancel), "NaN replay coordinate rejected");
	// Actual teleport event must split a short jump too: distance fallback is insufficient.
	RpEvent tp {};
	tp.type = RPEVENT_TELEPORT;
	tp.serverTick = 102;
	tp.data.teleport.hasOrigin = true;
	tp.data.teleport.origin[0] = 140;
	auto teleEvents = events;
	teleEvents.insert(teleEvents.begin() + 3, tp);
	auto teleTicks = ticks;
	teleTicks[3].post.origin.x = 145;
	teleTicks[4].pre.origin.x = 150;
	teleTicks[4].post.origin.x = 155;
	Write(c.path, Encode(header, teleTicks, teleEvents));
	r = ReadReferenceReplay(c, cancel);
	Check(r && std::abs(r->totalLength - 35) < 0.01, "real short teleport excluded, not distance guessed");
	for (size_t length = 0; length < bytes.size(); length += (std::max<size_t>)(1, bytes.size() / 41))
	{
		Write(c.path, std::vector<char>(bytes.begin(), bytes.begin() + length));
		Check(!ReadReferenceReplay(c, cancel), "truncated replay safely rejected");
	}
	u32 headerSize;
	memcpy(&headerSize, bytes.data(), sizeof(headerSize));
	auto corrupt = bytes;
	CompressedSectionHeader invalid {1, 0xffffffffu, 0xffffffffu};
	memcpy(corrupt.data() + 4 + headerSize, &invalid, sizeof(invalid));
	Write(c.path, corrupt);
	Check(!ReadReferenceReplay(c, cancel), "oversized decoder allocation rejected before decoding");
	Write(c.path, bytes);
	std::remove(c.path.c_str());
	std::printf("PASS: %d actual replay encoder/decoder checks\n", checks);
}
