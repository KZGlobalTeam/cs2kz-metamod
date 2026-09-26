#include "progress_routes.h"
#include "cs2kz.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
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

void KZHUDService::ResetProgress()
{
	this->progress = {};
}

void KZHUDService::EnterProgressStart(u32 guid)
{
	ResetProgress();
	this->progress.courseGUID = guid;
	this->progress.atStart = true;
	this->progress.route = KZ::progress::GetRoute(guid, player->modeService->GetModeName());
	this->progress.visible = !!this->progress.route;
}

void KZHUDService::StartProgress(u32 guid, bool fromStart)
{
	EnterProgressStart(guid);
	this->progress.atStart = false;
	this->progress.running = true;
	player->GetOrigin(&this->progress.lastPosition);
	if (this->progress.route && fromStart)
	{
		this->progress.match.segment = 0;
		this->progress.initialized = true;
	}
	if (!fromStart)
	{
		this->progress.visible = false;
	}
	this->progress.nextUpdate = g_pKZUtils->GetServerGlobals()->curtime + 0.05;
}

void KZHUDService::FinishProgress(u32 guid)
{
	if (guid != this->progress.courseGUID || !player->timerService->GetValidTimer())
	{
		ResetProgress();
		return;
	}
	if (!this->progress.route)
	{
		this->progress.route = KZ::progress::GetRoute(guid, player->modeService->GetModeName());
	}
	this->progress.running = false;
	this->progress.finished = true;
	this->progress.atStart = false;
	this->progress.visible = !!this->progress.route;
	this->progress.rawProgress = this->progress.displayedProgress = 100;
	this->progress.approximate = false;
	this->progress.stoppedAt = g_pKZUtils->GetServerGlobals()->curtime;
}

void KZHUDService::StopProgress()
{
	this->progress.running = this->progress.atStart = false;
	this->progress.stoppedAt = g_KZPlugin.unloading ? 0 : g_pKZUtils->GetServerGlobals()->curtime;
}

void KZHUDService::OnProgressTeleport(const Vector *origin)
{
	if (!origin)
	{
		return;
	}
	if (this->progress.finished || (!this->progress.running && !this->progress.atStart))
	{
		this->progress.visible = false;
	}
	this->progress.teleported = true;
	this->progress.reacquire = true;
	this->progress.nextUpdate = 0;
}

void KZHUDService::UpdateProgress(bool force)
{
	if (!kz_progress_enable.Get() || !player->GetPlayerPawn() || !player->IsAlive() || KZ::replaysystem::IsReplayBot(player))
	{
		this->progress.visible = false;
		return;
	}
	f64 now = g_pKZUtils->GetServerGlobals()->curtime;
	if (!force && now < this->progress.nextUpdate)
	{
		return;
	}
	const auto *course = KZ::course::GetCourse(this->progress.courseGUID);
	if (!course)
	{
		ResetProgress();
		return;
	}
	// Read the latest immutable route. A hot update never reuses the old segment index/CP anchors.
	auto latest = KZ::progress::GetRoute(this->progress.courseGUID, player->modeService->GetModeName());
	if (latest != this->progress.route)
	{
		this->progress.route = std::move(latest);
		this->progress.match = {};
		this->progress.initialized = false;
		this->progress.reacquire = true;
		this->progress.teleported = true;
		this->progress.visible = false;
		this->progress.approximate = false;
		this->progress.lastMatchedAt = -1;
		if (this->progress.atStart)
		{
			this->progress.rawProgress = this->progress.displayedProgress = 0;
		}
		else if (this->progress.finished)
		{
			this->progress.rawProgress = this->progress.displayedProgress = 100;
			this->progress.visible = !!this->progress.route;
		}
	}
	if (!this->progress.route)
	{
		this->progress.visible = false;
		this->progress.nextUpdate = now + 0.5;
		return;
	}
	if (this->progress.atStart)
	{
		this->progress.visible = true;
		this->progress.nextUpdate = now + 0.05;
		return;
	}
	if (!this->progress.running)
	{
		return;
	}
	auto *timer = player->timerService;
	if (!timer->GetValidTimer())
	{
		ResetProgress();
		return;
	}
	const auto *timerCourse = timer->GetCourse();
	if (!timer->GetTimerRunning() || !timerCourse || timerCourse->guid != this->progress.courseGUID)
	{
		ResetProgress();
		return;
	}
	if (timer->GetPaused())
	{
		return;
	}
	f32 interval = kz_progress_update_interval.Get();
	this->progress.nextUpdate = now + (std::isfinite(interval) ? std::clamp(interval, 0.05f, 0.25f) : 0.05f);
	Vector position, velocity;
	player->GetOrigin(&position);
	player->GetVelocity(&velocity);
	if (this->progress.initialized && (position - this->progress.lastPosition).Length() > 1024)
	{
		this->progress.teleported = true;
		this->progress.reacquire = true;
	}
	if (this->progress.reacquire)
	{
		++this->progress.reacquireCount;
	}
	KZ::progress::Match result;
	bool matched = KZ::progress::FindMatch(*this->progress.route, position, velocity, this->progress.lastPosition, this->progress.match,
										   this->progress.reacquire, this->progress.teleported, result, kz_progress_max_distance.Get());
	this->progress.visible = matched;
	if (matched)
	{
		this->progress.match = result;
		this->progress.lastMatchedAt = now;
		this->progress.approximate = this->progress.match.distanceToRoute > 96;
		this->progress.initialized = true;
		this->progress.reacquire = false;
		this->progress.rawProgress = std::clamp(this->progress.match.distance / this->progress.route->totalLength * 100.0, 0.0, 99.99);
		// Continuity is enforced by the matcher. No monotonic clamp or delayed CP rollback.
		this->progress.displayedProgress = this->progress.rawProgress;
	}
	else
	{
		this->progress.reacquire = true;
		f32 hold = kz_progress_hold_time.Get();
		hold = std::isfinite(hold) ? std::clamp(hold, 0.0f, 5.0f) : 1.5f;
		this->progress.visible = this->progress.initialized && !this->progress.teleported && this->progress.lastMatchedAt >= 0
								 && now - this->progress.lastMatchedAt <= hold;
		this->progress.approximate = this->progress.visible;
		this->progress.nextUpdate = now + 0.1;
		if (kz_progress_debug.Get())
		{
			KZ_LOG_DEBUG(LogChannel::General, "[Progress] Lost match: slot %d course %u\n", player->GetPlayerSlot().Get(), this->progress.courseGUID);
		}
	}
	// Keep actual sample history even while off-route; it is not the last matched position.
	// Preserve the pre-teleport position only until the teleport can be disambiguated.
	if (matched)
	{
		this->progress.teleported = false;
	}
	if (!this->progress.teleported)
	{
		this->progress.lastPosition = position;
	}
}

KZ::progress::Anchor KZHUDService::SaveProgressAnchor()
{
	UpdateProgress(true);
	return this->progress.visible && !this->progress.approximate && this->progress.route && this->progress.initialized
			   ? KZ::progress::Anchor {this->progress.route->generation, this->progress.courseGUID, this->progress.match.segment,
									   this->progress.match.distance}
			   : KZ::progress::Anchor {};
}

void KZHUDService::RestoreProgressAnchor(const KZ::progress::Anchor &a)
{
	if (!this->progress.route || !this->progress.running || a.generation != this->progress.route->generation
		|| a.courseGUID != this->progress.courseGUID || a.segment < 0 || (size_t)a.segment >= this->progress.route->segments.size())
	{
		return;
	}
	this->progress.match.segment = a.segment;
	this->progress.match.distance = a.distance;
	this->progress.rawProgress = this->progress.displayedProgress = std::clamp(a.distance / this->progress.route->totalLength * 100.0, 0.0, 99.99);
	this->progress.initialized = this->progress.visible = true;
	this->progress.approximate = false;
	this->progress.lastMatchedAt = g_pKZUtils->GetServerGlobals()->curtime;
	this->progress.teleported = false;
	this->progress.reacquire = false;
	this->progress.nextUpdate = 0;
	player->GetOrigin(&this->progress.lastPosition);
}

bool KZHUDService::GetProgressDisplay(f64 &value) const
{
	if (!kz_progress_enable.Get() || !this->progress.visible || !this->progress.route || !player->IsAlive()
		|| !KZ::course::GetCourse(this->progress.courseGUID))
	{
		return false;
	}
	if (!this->progress.running && !this->progress.atStart
		&& g_pKZUtils->GetServerGlobals()->curtime - this->progress.stoppedAt > KZ_HUD_TIMER_STOPPED_GRACE_TIME)
	{
		return false;
	}
	value = this->progress.displayedProgress;
	return true;
}

void KZHUDService::PrintProgressDebug()
{
	player->PrintConsole(false, false,
						 "[Progress] Player=%s Course=%u running=%d initialized=%d visible=%d segment=%d raw=%.4f displayed=%.2f distance=%.2f "
						 "reacquires=%u points=%zu length=%.2f source=%s",
						 player->GetName(), this->progress.courseGUID, this->progress.running, this->progress.initialized, this->progress.visible,
						 this->progress.match.segment, this->progress.rawProgress, this->progress.displayedProgress,
						 this->progress.match.distanceToRoute, this->progress.reacquireCount,
						 this->progress.route ? this->progress.route->points.size() : 0, this->progress.route ? this->progress.route->totalLength : 0,
						 this->progress.route ? this->progress.route->source.c_str() : "No progress route available.");
}

std::string KZHUDService::GetProgressText(const MHUDPrefs &prefs, const char *language) const
{
	f64 value;
	if (!kz_progress_hud_enable.Get() || !prefs.showProgress || KZ::replaysystem::IsReplayBot(this->player) || !this->GetProgressDisplay(value))
	{
		return {};
	}
	char percentage[32];
	V_snprintf(percentage, sizeof(percentage), "%s%.2f%%", this->progress.approximate ? "~" : "", value);
	return KZLanguageService::PrepareMessageWithLang(language, "HUD - Progress Text", percentage);
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
	player->languageService->PrintChat(true, false, enabled ? "Progress - Enabled" : "Progress - Disabled");
	return true;
}

SCMD(kz_progress_info, SCFL_HUD)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (player)
	{
		player->hudService->PrintProgressDebug();
	}
	return true;
}

// Server console/RCON only, like other administrative CON_COMMAND commands.
CON_COMMAND_F(kz_progress_reload, "Reload progress routes from cached record references.", FCVAR_NONE)
{
	if (context.GetPlayerSlot().Get() >= 0)
	{
		return;
	}
	KZ::progress::Reload();
}
