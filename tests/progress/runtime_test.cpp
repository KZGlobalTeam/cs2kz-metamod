// Optional --progress-tests build only. Run on an isolated LAN server with bots.
#include "kz/progress/kz_progress.h"
#include "kz/replays/compression.h"
#include "kz/timer/kz_timer.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/option/kz_option.h"
#include "kz/mode/kz_mode.h"
#include "kz/spec/kz_spec.h"
#include "utils/simplecmds.h"
#include "filesystem.h"
#include <ctime>
#include <cmath>

static CHandle<CCSPlayerController> testViewer, testTarget;

static KZPlayer *Bot(i32 number)
{
	for (u32 i = 0; i <= MAXPLAYERS; ++i)
	{
		auto *p = g_pKZPlayerManager->ToPlayer(i);
		if (p && p->IsFakeClient() && p->IsAlive() && p->GetPlayerPawn() && --number == 0)
		{
			return p;
		}
	}
	return nullptr;
}

CON_COMMAND(kz_progress_test_prepare, "TEST BUILD ONLY: write a synthetic local reference replay for the current map.")
{
	using namespace KZ::replaysystem::compression;
	auto *course = KZ::course::GetFirstCourse();
	auto *p = Bot(1);
	if (!course || !p)
	{
		Msg("[ProgressTest] prepare requires a map and a live bot.\n");
		return;
	}
	ReplayHeader h;
	h.set_version(KZ_REPLAY_VERSION);
	h.set_type(cs2kz::replay::RP_RUN);
	h.set_timestamp((u64)std::time(nullptr));
	h.mutable_map()->set_name(g_pKZUtils->GetCurrentMapName().Get());
	char md5[33] {};
	g_pKZUtils->GetCurrentMapMD5(md5, sizeof(md5));
	h.mutable_map()->set_md5(md5);
	h.mutable_player()->set_name("Progress synthetic test fixture");
	auto *run = h.mutable_run();
	run->set_course_name(course->name);
	run->mutable_mode()->set_name(p->modeService->GetModeName());
	run->set_time(1);
	run->set_num_teleports(0);
	std::vector<TickData> ticks(41);
	std::vector<SubtickData> subticks(41);
	for (u32 i = 0; i < ticks.size(); ++i)
	{
		ticks[i].serverTick = 100 + i;
		ticks[i].pre.origin = ticks[i].post.origin = Vector(i * 25.0f, 0, 0);
	}
	using TE = RpEvent::RpEventData::TimerEvent;
	RpEvent start {}, finish {};
	start.type = finish.type = RPEVENT_TIMER_EVENT;
	start.serverTick = 100;
	finish.serverTick = 140;
	start.data.timer.type = TE::TIMER_START;
	finish.data.timer.type = TE::TIMER_END;
	start.data.timer.index = finish.data.timer.index = course->id;
	finish.data.timer.time = 1;
	auto serialized = h.SerializeAsString();
	u32 size = (u32)serialized.size();
	std::vector<char> bytes((char *)&size, (char *)&size + sizeof(size));
	bytes.insert(bytes.end(), serialized.begin(), serialized.end());
	WriteTickDataCompressed(bytes, ticks, subticks);
	WriteWeaponsCompressed(bytes, {});
	WriteJumpsCompressed(bytes, {});
	WriteEventsCompressed(bytes, {start, finish});
	g_pFullFileSystem->CreateDirHierarchy(KZ_REPLAY_PATH, "GAME");
	auto file = g_pFullFileSystem->Open(KZ_REPLAY_PATH "/11111111-1111-4111-8111-111111111111.replay", "wb", "GAME");
	if (!file)
	{
		Msg("[ProgressTest] fixture write failed\n");
		return;
	}
	g_pFullFileSystem->Write(bytes.data(), (i32)bytes.size(), file);
	g_pFullFileSystem->Close(file);
	Msg("[ProgressTest] fixture ready: map=%s course=%s mode=%s md5=%s\n", h.map().name().c_str(), course->name, run->mode().name().c_str(), md5);
	KZ::progress::Reload();
}

CON_COMMAND(kz_progress_test_run, "TEST BUILD ONLY: exercise actual timer, checkpoint, spectator and preference services with bots.")
{
	auto *p = Bot(1);
	auto *viewer = Bot(2);
	auto *course = KZ::course::GetFirstCourse();
	if (!p || !viewer || !course)
	{
		Msg("[ProgressTest] need two live bots and a course\n");
		return;
	}
	auto route = KZ::progress::GetRoute(course->guid, p->modeService->GetModeName());
	if (!route)
	{
		Msg("[ProgressTest] route not ready; retry after watcher scan\n");
		return;
	}
	i32 passed = 0, failed = 0;
	auto check = [&](bool condition, const char *name)
	{
		Msg("[ProgressTest] %s: %s\n", condition ? "PASS" : "FAIL", name);
		condition ? ++passed : ++failed;
	};
	check(route->source == "11111111-1111-4111-8111-111111111111", "ReplayWatcher -> background decoder -> main-thread route publication");
	p->timerService->TimerStop(false);
	p->timerService->StartZoneStartTouch(course);
	f64 value = -1;
	check(p->progressService->GetDisplay(value) && value == 0, "enter start zone = 0");
	p->GetPlayerPawn()->m_fFlags(p->GetPlayerPawn()->m_fFlags() | FL_ONGROUND);
	p->inPerf = false;
	p->landingTime = 0;
	if (!p->timerService->TimerStart(course, false))
	{
		check(false, "actual TimerStart accepted");
		return;
	}
	check(p->progressService->GetDisplay(value) && value == 0, "timer event starts progress at 0");
	p->SetOrigin(Vector(100, 0, 0));
	p->progressService->Update(true);
	check(p->progressService->GetDisplay(value) && std::abs(value - 10) < 0.01, "position -> 10 percent");
	p->GetPlayerPawn()->m_fFlags(p->GetPlayerPawn()->m_fFlags() | FL_ONGROUND);
	p->checkpointService->SetCheckpoint();
	p->SetOrigin(Vector(600, 0, 0));
	p->progressService->Update(true);
	check(p->progressService->GetDisplay(value) && std::abs(value - 60) < 0.01, "position -> 60 percent");
	p->SetOrigin(Vector(500, 0, 0));
	p->progressService->Update(true);
	check(p->progressService->GetDisplay(value) && std::abs(value - 50) < 0.01, "backward movement reduces progress");
	p->progressService->Start(course->guid, false);
	p->progressService->Update(true);
	check(p->progressService->GetDisplay(value) && std::abs(value - 50) < 0.01, "mid-run reacquire does not seed a false start anchor");
	p->checkpointService->DoTeleport(0);
	check(p->progressService->GetDisplay(value) && std::abs(value - 10) < 0.01, "actual checkpoint teleport rolls progress back to 10");
	check(KZ::progress::HUDText(viewer, p) == "Progress: 10.00%", "viewer HUD uses target progress");
	check(KZ::progress::HUDText(viewer, nullptr).empty(), "invalid target hidden");
	viewer->optionService->SetPreferenceBool("showProgress", true);
	CCommand command;
	command.Tokenize("kz_progress");
	CPlayerSlot slot = viewer->GetPlayerSlot();
	scmd::OnClientCommand(slot, command);
	check(!viewer->optionService->GetPreferenceBool("showProgress", true) && KZ::progress::HUDText(viewer, p).empty(),
		  "registered kz_progress command toggles persisted preference");
	viewer->optionService->SetPreferenceBool("showProgress", true);
	check(p->timerService->TimerEnd(course), "actual TimerEnd accepted");
	check(p->progressService->GetDisplay(value) && value == 100, "TimerEndPost forces exact 100");
	p->timerService->InvalidateRun();
	check(!p->progressService->GetDisplay(value), "timer invalidation hides progress");
	testViewer = viewer->GetController();
	testTarget = p->GetController();
	check(viewer->specService->SpectatePlayer(p), "spectate request accepted; target checked after team transition");
	KZ::progress::Reload();
	check(!p->progressService->GetDisplay(value), "reload clears player state and routes");
	Msg("[ProgressTest] SUMMARY passed=%d failed=%d\n", passed, failed);
}

CON_COMMAND(kz_progress_test_spectator, "TEST BUILD ONLY: inspect observer state after the team transition.")
{
	auto *viewer = g_pKZPlayerManager->ToPlayer(testViewer.Get());
	auto *target = g_pKZPlayerManager->ToPlayer(testTarget.Get());
	const bool success = viewer && target && viewer->specService->GetSpectatedPlayer() == target;
	Msg("[ProgressTest] %s: delayed observer target (viewer alive=%d team=%d)\n", success ? "PASS" : "FAIL", viewer && viewer->IsAlive(),
		viewer && viewer->GetController() ? viewer->GetController()->GetTeam() : -1);
}
