#include "kz_progress.h"
#include "cs2kz.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/replays/kz_replaysystem.h"
#include "utils/simplecmds.h"
#include <algorithm>
#include <cmath>

CConVar<bool> kz_progress_enable("kz_progress_enable", FCVAR_NONE, "Enable reference-route progress.", true);
CConVar<bool> kz_progress_hud_enable("kz_progress_hud_enable", FCVAR_NONE, "Show progress in the CS2KZ HUD.", true);
CConVar<f32> kz_progress_update_interval("kz_progress_update_interval", FCVAR_NONE, "Progress update interval in seconds (0.05 to 0.25).", 0.05f);
CConVar<bool> kz_progress_debug("kz_progress_debug", FCVAR_NONE, "Log progress route acquisition failures.", false);

CConVar<f32> kz_progress_max_distance("kz_progress_max_distance", FCVAR_NONE, "Maximum route distance (64 to 768 units).", 384.0f);
CConVar<f32> kz_progress_hold_time("kz_progress_hold_time", FCVAR_NONE, "Seconds to hold approximate progress off-route (0 to 5).", 1.5f);

void KZProgressService::Reset()
{
	route.reset();
	courseGUID = 0;
	match = {};
	lastPosition = vec3_origin;
	rawProgress = displayedProgress = nextUpdate = stoppedAt = 0;
	initialized = running = finished = atStart = teleported = visible = false;
	reacquire = true;
	reacquireCount = 0;
	lastMatchedAt = -1;
	approximate = false;
}

void KZProgressService::EnterStart(u32 guid)
{
	Reset();
	courseGUID = guid;
	atStart = true;
	route = KZ::progress::GetRoute(guid, player->modeService->GetModeName());
	visible = !!route;
}

void KZProgressService::Start(u32 guid, bool fromStart)
{
	EnterStart(guid);
	atStart = false;
	running = true;
	player->GetOrigin(&lastPosition);
	if (route && fromStart)
	{
		match.segment = 0;
		initialized = true;
	}
	if (!fromStart)
	{
		visible = false;
	}
	nextUpdate = g_pKZUtils->GetServerGlobals()->curtime + 0.05;
}

void KZProgressService::Finish(u32 guid)
{
	if (guid != courseGUID || !player->timerService->GetValidTimer())
	{
		Reset();
		return;
	}
	if (!route)
	{
		route = KZ::progress::GetRoute(guid, player->modeService->GetModeName());
	}
	running = false;
	finished = true;
	atStart = false;
	visible = !!route;
	rawProgress = displayedProgress = 100;
	approximate = false;
	stoppedAt = g_pKZUtils->GetServerGlobals()->curtime;
}

void KZProgressService::Stop()
{
	running = atStart = false;
	stoppedAt = g_KZPlugin.unloading ? 0 : g_pKZUtils->GetServerGlobals()->curtime;
}

void KZProgressService::OnTeleport(const Vector *origin)
{
	if (!origin)
	{
		return;
	}
	if (finished || (!running && !atStart))
	{
		visible = false;
	}
	teleported = true;
	reacquire = true;
	nextUpdate = 0;
}

void KZProgressService::Update(bool force)
{
	if (!kz_progress_enable.Get() || !player->GetPlayerPawn() || !player->IsAlive() || KZ::replaysystem::IsReplayBot(player))
	{
		visible = false;
		return;
	}
	f64 now = g_pKZUtils->GetServerGlobals()->curtime;
	if (!force && now < nextUpdate)
	{
		return;
	}
	const auto *course = KZ::course::GetCourse(courseGUID);
	if (!course)
	{
		Reset();
		return;
	}
	// Keep one immutable route for a run; reload clears all services before releasing routes.
	if (!route)
	{
		route = KZ::progress::GetRoute(courseGUID, player->modeService->GetModeName());
		if (route)
		{
			reacquire = true;
			initialized = false;
		}
	}
	if (!route)
	{
		visible = false;
		nextUpdate = now + 0.5;
		return;
	}
	if (atStart)
	{
		visible = true;
		nextUpdate = now + 0.05;
		return;
	}
	if (!running)
	{
		return;
	}
	auto *timer = player->timerService;
	if (!timer->GetValidTimer())
	{
		Reset();
		return;
	}
	const auto *timerCourse = timer->GetCourse();
	if (!timer->GetTimerRunning() || !timerCourse || timerCourse->guid != courseGUID)
	{
		Reset();
		return;
	}
	if (timer->GetPaused())
	{
		return;
	}
	f32 interval = kz_progress_update_interval.Get();
	nextUpdate = now + (std::isfinite(interval) ? std::clamp(interval, 0.05f, 0.25f) : 0.05f);
	Vector position, velocity;
	player->GetOrigin(&position);
	player->GetVelocity(&velocity);
	if (initialized && (position - lastPosition).Length() > 1024)
	{
		teleported = true;
		reacquire = true;
	}
	if (reacquire)
	{
		++reacquireCount;
	}
	KZ::progress::Match result;
	bool matched =
		KZ::progress::FindMatch(*route, position, velocity, lastPosition, match, reacquire, teleported, result, kz_progress_max_distance.Get());
	visible = matched;
	if (matched)
	{
		match = result;
		lastMatchedAt = now;
		approximate = match.distanceToRoute > 96;
		initialized = true;
		reacquire = false;
		rawProgress = std::clamp(match.distance / route->totalLength * 100.0, 0.0, 99.99);
		// Continuity is enforced by the matcher. No monotonic clamp or delayed CP rollback.
		displayedProgress = rawProgress;
	}
	else
	{
		reacquire = true;
		f32 hold = kz_progress_hold_time.Get();
		hold = std::isfinite(hold) ? std::clamp(hold, 0.0f, 5.0f) : 1.5f;
		visible = initialized && !teleported && lastMatchedAt >= 0 && now - lastMatchedAt <= hold;
		approximate = visible;
		nextUpdate = now + 0.1;
		if (kz_progress_debug.Get())
		{
			KZ_LOG_DEBUG(LogChannel::General, "[Progress] Lost match: slot %d course %u\n", player->GetPlayerSlot().Get(), courseGUID);
		}
	}
	// Keep actual sample history even while off-route; it is not the last matched position.
	// Preserve the pre-teleport position only until the teleport can be disambiguated.
	if (matched)
	{
		teleported = false;
	}
	if (!teleported)
	{
		lastPosition = position;
	}
}

KZ::progress::Anchor KZProgressService::SaveAnchor()
{
	Update(true);
	return visible && !approximate && route && initialized ? KZ::progress::Anchor {route->generation, courseGUID, match.segment, match.distance}
														   : KZ::progress::Anchor {};
}

void KZProgressService::RestoreAnchor(const KZ::progress::Anchor &a)
{
	if (!route || !running || a.generation != route->generation || a.courseGUID != courseGUID || a.segment < 0
		|| (size_t)a.segment >= route->segments.size())
	{
		return;
	}
	match.segment = a.segment;
	match.distance = a.distance;
	rawProgress = displayedProgress = std::clamp(a.distance / route->totalLength * 100.0, 0.0, 99.99);
	initialized = visible = true;
	approximate = false;
	lastMatchedAt = g_pKZUtils->GetServerGlobals()->curtime;
	teleported = false;
	reacquire = false;
	nextUpdate = 0;
	player->GetOrigin(&lastPosition);
}

bool KZProgressService::GetDisplay(f64 &value) const
{
	if (!kz_progress_enable.Get() || !visible || !route || !player->IsAlive() || !KZ::course::GetCourse(courseGUID))
	{
		return false;
	}
	if (!running && !atStart && g_pKZUtils->GetServerGlobals()->curtime - stoppedAt > KZ_HUD_TIMER_STOPPED_GRACE_TIME)
	{
		return false;
	}
	value = displayedProgress;
	return true;
}

void KZProgressService::PrintDebug()
{
	player->PrintConsole(false, false,
						 "[Progress] Player=%s Course=%u running=%d initialized=%d visible=%d segment=%d raw=%.4f displayed=%.2f distance=%.2f "
						 "reacquires=%u points=%zu length=%.2f source=%s",
						 player->GetName(), courseGUID, running, initialized, visible, match.segment, rawProgress, displayedProgress,
						 match.distanceToRoute, reacquireCount, route ? route->points.size() : 0, route ? route->totalLength : 0,
						 route ? route->source.c_str() : "No progress route available.");
}

std::string KZ::progress::HUDText(KZPlayer *viewer, KZPlayer *source)
{
	f64 value;
	if (!viewer || !source || !kz_progress_hud_enable.Get() || !viewer->optionService->GetPreferenceBool("showProgress", true)
		|| KZ::replaysystem::IsReplayBot(source) || !source->progressService->GetDisplay(value))
	{
		return {};
	}
	char text[48];
	V_snprintf(text, sizeof(text), "Progress: %s%.2f%%", source->progressService->IsApproximate() ? "~" : "", value);
	return text;
}

SCMD(kz_progress, SCFL_HUD | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return true;
	}
	bool enabled = !player->optionService->GetPreferenceBool("showProgress", true);
	player->optionService->SetPreferenceBool("showProgress", enabled);
	player->PrintChat(true, false, "Progress: %s", enabled ? "ON" : "OFF");
	return true;
}

SCMD(kz_progress_info, SCFL_HUD)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (player)
	{
		player->progressService->PrintDebug();
	}
	return true;
}

// Server console/RCON only, like other administrative CON_COMMAND commands.
CON_COMMAND(kz_progress_reload, "Reload progress routes from completed local replays.")
{
	KZ::progress::Reload();
}
