#include "cs2kz.h"
#include "kz/kz.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/spec/kz_spec.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "sdk/usercmd.h"
#include "utils/simplecmds.h"
#include "utils/utils.h"
#include "controls.h"
#include "kz_replaysystem.h"
#include "commands.h"
#include "playback.h"
#include "data.h"
#include "compression.h"
#include "bot.h"

#include <vector>

#include "tier0/memdbgon.h"

using KZ::replaysystem::controls::ControlsWindow;

static_assert(KZ_REPLAY_CONTROLS_ZONE_MARKERS == KZ_MAX_REPLAY_MARKED_ZONES, "The mapping API warns about the zones the bar cannot mark");

#define SLOT_ID(fn, fmt) \
	static_function const char *fn(i32 i) \
	{ \
		static_persist char buf[24]; \
		V_snprintf(buf, sizeof(buf), fmt, i); \
		return buf; \
	}

SLOT_ID(PercentPanel, "rp_pct%i")
SLOT_ID(ZonePosPanel, "rp_zone_pos%i")
SLOT_ID(ZoneAtPanel, "rp_zone_at%i")
SLOT_ID(ZoneFinePanel, "rp_zone_fine%i")
SLOT_ID(ZonePanel, "rp_zone%i")
SLOT_ID(ZoneLabel, "rp_zone_label%i")
SLOT_ID(ZoneVar, "rz%i")
SLOT_ID(SpeedPanel, "rp_speed%i")
#undef SLOT_ID

static_global const f32 SPEEDS[KZ_REPLAY_CONTROLS_SPEEDS] = {0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};

static_global const struct
{
	const char *panelId;
	i32 seconds;
} SKIPS[KZ_REPLAY_CONTROLS_SKIPS] = {
	{"rp_back300", -300}, {"rp_back60", -60}, {"rp_back5", -5}, {"rp_next5", 5}, {"rp_next60", 60}, {"rp_next300", 300},
};

// Static labels, and the phrase each one shows.
static_global const char *const TEXTS[][3] = {
	{"rp_breather_label", "rpbreather", "Replay Controls - Skip Breather"},
	{"rp_skip_pause_label", "rpskippause", "Replay Controls - Skip Pause"},
	{"rp_tick_title", "rptickname", "Replay Controls - Tick"},
	{"rp_tick_back_label", "rpprevtick", "Replay Controls - Previous Tick"},
	{"rp_tick_next_label", "rpnexttick", "Replay Controls - Next Tick"},
	{"rp_split_label", "rpsplit", "Replay Controls - Zone Split"},
	{"rp_cp_label", "rpcp", "Replay Controls - Zone Checkpoint"},
	{"rp_stage_label", "rpstage", "Replay Controls - Zone Stage"},
	{"rp_tp_label", "rptp", "Replay Controls - Zone Teleport"},
	{"rp_time_title", "rptimename", "Replay Controls - Time"},
	{"rp_visibility_label", "rpvisibility", "Replay Controls - Hide Controls"},
};

// Labels with a fixed text. It is still sent from here, because only a new text makes a label measure itself in a new font.
static_global const char *const LITERAL_TEXTS[][3] = {
	{"rp_speed0_label", "rps0", "0.1x"},         {"rp_speed1_label", "rps1", "0.25x"},        {"rp_speed2_label", "rps2", "0.5x"},
	{"rp_speed3_label", "rps3", "1x"},           {"rp_speed4_label", "rps4", "2x"},           {"rp_speed5_label", "rps5", "4x"},
	{"rp_back300_label", "rpback300", "-5m"},    {"rp_back60_label", "rpback60", "-1m"},      {"rp_back5_label", "rpback5", "-5s"},
	{"rp_next5_label", "rpnext5", "+5s"},        {"rp_next60_label", "rpnext60", "+1m"},      {"rp_next300_label", "rpnext300", "+5m"},
	{"rp_prev_split_label", "rpprevsplit", "<"}, {"rp_next_split_label", "rpnextsplit", ">"}, {"rp_prev_cp_label", "rpprevcp", "<"},
	{"rp_next_cp_label", "rpnextcp", ">"},       {"rp_prev_stage_label", "rpprevstage", "<"}, {"rp_next_stage_label", "rpnextstage", ">"},
	{"rp_prev_tp_label", "rpprevtp", "<"},       {"rp_next_tp_label", "rpnexttp", ">"},
};

// Indexed by ZoneMarker::kind, then teleports, which have no marker.
#define TELEPORT_KIND 3

static_global const struct
{
	const char *groupPanel;
	const char *prevPanel;
	const char *nextPanel;
} ZONE_JUMPS[KZ_REPLAY_CONTROLS_ZONE_KINDS] = {
	{"rp_jump_split", "rp_prev_split", "rp_next_split"},
	{"rp_jump_cp", "rp_prev_cp", "rp_next_cp"},
	{"rp_jump_stage", "rp_prev_stage", "rp_next_stage"},
	{"rp_jump_tp", "rp_prev_tp", "rp_next_tp"},
};

struct ZoneMarker
{
	i32 kind;
	const char *typeClass;
	const char *phraseKey;
	i32 index;
	f32 time;
	u32 tick;
};

static_global std::vector<ZoneMarker> zoneMarkers;
// The ticks the previous and next buttons jump between, in tick order, per entry of ZONE_JUMPS.
static_global std::vector<u32> jumpTicks[KZ_REPLAY_CONTROLS_ZONE_KINDS];
// Where the run's timer starts and ends, or -1 when the replay has none.
static_global i64 runStartTick = -1;
static_global i64 runEndTick = -1;

// The ticks from a timer pause up to where skipping it lands: a tick before its resume, or the last tick when it never resumes.
struct PauseSegment
{
	u32 start;
	u32 skipTick;
};

static_global std::vector<PauseSegment> pauseSegments;

// Jumping to an event lands a tick before it, so the event plays out instead of having already happened.
static_function u32 GetEventSeekTick(i64 eventTick)
{
	return eventTick > 0 ? (u32)(eventTick - 1) : 0;
}

// Where skipping the pause the current tick is in lands, or -1 outside a pause.
static_function i64 FindPauseSkipTick(u32 currentTick)
{
	for (const PauseSegment &pause : pauseSegments)
	{
		if (pause.start <= currentTick && currentTick < pause.skipTick)
		{
			return pause.skipTick;
		}
	}
	return -1;
}

static_global u32 currentReplayId;

// Uses the same rule as CInButtonState::IsButtonNewlyPressed, where any state from UP_DOWN upwards had a press.
static_function u64 GetNewlyPressedButtons(const CInButtonStatePB &buttons)
{
	return buttons.buttonstate3() | (buttons.buttonstate1() & buttons.buttonstate2());
}

using KZ::replaysystem::data::GetTickAtServerTick;

static_function bool IsTimerEvent(const RpEvent &event, RpEvent::RpEventData::TimerEvent::TimerEventType type)
{
	return event.type == RPEVENT_TIMER_EVENT && event.data.timer.type == type;
}

static_function const RpEvent *FindRunStart(const KZ::replaysystem::data::ReplayPlayback *replay)
{
	if (static_cast<ReplayType>(replay->header.type()) == ReplayType::RP_RUN && replay->header.has_run())
	{
		const u32 index = replay->header.run().start_event_index();
		if (index < replay->numEvents && IsTimerEvent(replay->events[index], RpEvent::RpEventData::TimerEvent::TIMER_START))
		{
			return &replay->events[index];
		}
	}
	for (u32 i = 0; i < replay->numEvents; i++)
	{
		if (IsTimerEvent(replay->events[i], RpEvent::RpEventData::TimerEvent::TIMER_START))
		{
			return &replay->events[i];
		}
	}
	return nullptr;
}

// The end that belongs to the run's start, after it in the replay.
static_function const RpEvent *FindRunEnd(const KZ::replaysystem::data::ReplayPlayback *replay, const RpEvent *start)
{
	if (static_cast<ReplayType>(replay->header.type()) == ReplayType::RP_RUN && replay->header.has_run())
	{
		const u32 index = replay->header.run().end_event_index();
		if (index < replay->numEvents && IsTimerEvent(replay->events[index], RpEvent::RpEventData::TimerEvent::TIMER_END))
		{
			return &replay->events[index];
		}
	}
	for (u32 i = (u32)(start - replay->events) + 1; i < replay->numEvents; i++)
	{
		if (IsTimerEvent(replay->events[i], RpEvent::RpEventData::TimerEvent::TIMER_END))
		{
			return &replay->events[i];
		}
		if (IsTimerEvent(replay->events[i], RpEvent::RpEventData::TimerEvent::TIMER_START))
		{
			return nullptr;
		}
	}
	return nullptr;
}

// Position along the bar in layout pixels.
static_function i32 GetBarPosition(u32 tick, u32 tickCount)
{
	return tickCount > 1 ? (i32)((u64)MIN(tick, tickCount - 1) * KZ_REPLAY_CONTROLS_BAR_WIDTH / (tickCount - 1)) : 0;
}

// Swaps the panel's prefix--N class for prefix--want. A negative cache stands for no class.
static_function void SetValueClass(CCSCustomHudLayout *entity, const char *panelId, const char *prefix, i32 &cache, i32 want)
{
	if (cache == want)
	{
		return;
	}
	char className[32];
	if (cache >= 0)
	{
		V_snprintf(className, sizeof(className), "%s--%i", prefix, cache);
		KZ::ui::SetClass(entity, panelId, className, false);
	}
	V_snprintf(className, sizeof(className), "%s--%i", prefix, want);
	KZ::ui::SetClass(entity, panelId, className, true);
	cache = want;
}

// A skip is only offered while the whole of it fits in the replay.
static_function bool CanSkip(const KZ::replaysystem::data::ReplayPlayback *replay, i32 seconds)
{
	const i64 target = (i64)replay->currentTick + (i64)seconds * (i64)ENGINE_FIXED_TICK_RATE;
	return target >= 0 && target <= (i64)replay->tickCount - 1;
}

// The closest jump target of that kind before or after the current tick, or -1 when there is none.
static_function i64 FindZoneTick(u32 currentTick, i32 kind, bool forward)
{
	i64 found = -1;
	for (u32 tick : jumpTicks[kind])
	{
		if (forward && tick > currentTick)
		{
			return tick;
		}
		if (!forward && tick < currentTick)
		{
			found = tick;
		}
	}
	return found;
}

static_function bool CanStepBack(const KZ::replaysystem::data::ReplayPlayback *replay)
{
	return replay->currentTick > 0;
}

// Same rule as playback::StepTick, which never plays the last tick.
static_function bool CanStepForward(const KZ::replaysystem::data::ReplayPlayback *replay)
{
	return replay->currentTick + replay->stepTicks + 1 < replay->tickCount;
}

static_function void Seek(KZPlayer *player, i64 tick, bool paused)
{
	auto replay = KZ::replaysystem::data::GetCurrentReplay();
	tick = MAX(tick, 0);
	tick = MIN(tick, (i64)replay->tickCount - 1);
	KZ::replaysystem::commands::NavigateReplay(player, (u32)tick);
	// Navigating resumes the replay.
	replay->replayPaused = paused;
}

static_global class KZOptionServiceEventListener_ReplayControls : public KZOptionServiceEventListener
{
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player)
	{
		if (!player->IsFakeClient())
		{
			player->uiService->GetWindow<ControlsWindow>()->OnStyleChanged();
		}
	}

	virtual void OnPlayerPreferenceChanged(KZPlayer *player, const char *optionName)
	{
		if (!player->IsFakeClient()
			&& (KZ_STREQI(optionName, "menuFont") || KZ_STREQI(optionName, "menuColor") || KZ_STREQI(optionName, "menuSounds")))
		{
			player->uiService->GetWindow<ControlsWindow>()->OnStyleChanged();
		}
	}
} optionEventListener;

namespace KZ::replaysystem::controls
{
	void Init()
	{
		KZOptionService::RegisterEventListener(&optionEventListener);
	}

	void OnReplayStart()
	{
		currentReplayId++;
		zoneMarkers.clear();
		for (std::vector<u32> &ticks : jumpTicks)
		{
			ticks.clear();
		}
		runStartTick = -1;
		runEndTick = -1;
		pauseSegments.clear();
		auto replay = data::GetCurrentReplay();
		if (replay->tickCount == 0)
		{
			return;
		}
		i64 pauseStart = -1;
		for (u32 i = 0; i < replay->numEvents; i++)
		{
			const RpEvent &event = replay->events[i];
			if (pauseStart < 0 && IsTimerEvent(event, RpEvent::RpEventData::TimerEvent::TIMER_PAUSE))
			{
				pauseStart = GetTickAtServerTick(replay, event.serverTick);
			}
			else if (pauseStart >= 0 && IsTimerEvent(event, RpEvent::RpEventData::TimerEvent::TIMER_RESUME))
			{
				pauseSegments.push_back({(u32)pauseStart, GetEventSeekTick(GetTickAtServerTick(replay, event.serverTick))});
				pauseStart = -1;
			}
		}
		if (pauseStart >= 0)
		{
			pauseSegments.push_back({(u32)pauseStart, replay->tickCount - 1});
		}
		if (const RpEvent *start = FindRunStart(replay))
		{
			runStartTick = GetTickAtServerTick(replay, start->serverTick);
			if (const RpEvent *end = FindRunEnd(replay, start))
			{
				runEndTick = GetTickAtServerTick(replay, end->serverTick);
			}
		}
		for (u32 i = 0; i < replay->numEvents; i++)
		{
			const RpEvent &event = replay->events[i];
			if (event.type != RPEVENT_TIMER_EVENT)
			{
				continue;
			}
			ZoneMarker marker {};
			switch (event.data.timer.type)
			{
				case RpEvent::RpEventData::TimerEvent::TIMER_SPLIT:
				{
					marker.kind = 0;
					marker.typeClass = "zone-split";
					marker.phraseKey = "Replay Controls - Split";
					break;
				}
				case RpEvent::RpEventData::TimerEvent::TIMER_CPZ:
				{
					marker.kind = 1;
					marker.typeClass = "zone-checkpoint";
					marker.phraseKey = "Replay Controls - Checkpoint";
					break;
				}
				case RpEvent::RpEventData::TimerEvent::TIMER_STAGE:
				{
					marker.kind = 2;
					marker.typeClass = "zone-stage";
					marker.phraseKey = "Replay Controls - Stage";
					break;
				}
				default:
				{
					continue;
				}
			}
			marker.index = event.data.timer.index;
			marker.time = event.data.timer.time;
			marker.tick = GetTickAtServerTick(replay, event.serverTick);
			zoneMarkers.push_back(marker);
			jumpTicks[marker.kind].push_back(GetEventSeekTick(marker.tick));
		}
		for (u32 tick : replay->ticks->TeleportTicks())
		{
			jumpTicks[TELEPORT_KIND].push_back(GetEventSeekTick(tick));
		}
	}

	bool ControlsWindow::IsWatching()
	{
		KZPlayer *bot = bot::GetBotPlayer();
		if (!bot || !data::IsReplayPlaying() || !KZHUDService::IsLayoutHudAvailable() || this->player->specService->GetSpectatedPlayer() != bot)
		{
			return false;
		}
		CPlayer_ObserverServices *observer = this->player->GetController()->m_hObserverPawn()->m_pObserverServices;
		return observer->m_iObserverMode() == OBS_MODE_IN_EYE || observer->m_iObserverMode() == OBS_MODE_CHASE;
	}

	CCSCustomHudLayout *ControlsWindow::EnsureLayout()
	{
		if (this->replayId != currentReplayId)
		{
			this->replayId = currentReplayId;
			this->player->uiService->Close(this);
			this->layout.Destroy();
		}
		bool created = false;
		CCSCustomHudLayout *entity = this->layout.Ensure(created);
		if (created)
		{
			this->applied = Applied();
			this->styleChanged = true;
		}
		return entity;
	}

	void ControlsWindow::Update()
	{
		if (!this->IsWatching() || this->controlsHidden)
		{
			this->player->uiService->Close(this);
			this->Hide();
			return;
		}
		if (this->EnsureLayout())
		{
			this->Render();
		}
		if (this->cursorReplayId != 0)
		{
			// The request expires with its replay, so a player who never got to watch it does not get the cursor later.
			if (this->cursorReplayId == currentReplayId && !this->player->uiService->IsOpen(this))
			{
				this->player->uiService->Open(this, KZ::ui::OpenMode::Push);
			}
			this->cursorReplayId = 0;
		}

		CCSPlayerPawn *botPawn = bot::GetBotPlayer()->GetPlayerPawn();
		if (botPawn && !(botPawn->m_iHideHUD() & KZ_HIDEHUD_WEAPONSELECTION))
		{
			botPawn->m_iHideHUD(botPawn->m_iHideHUD() | KZ_HIDEHUD_WEAPONSELECTION);
		}
	}

	void ControlsWindow::OnProcessUsercmds(CUserCmd *cmds, i32 numCmds)
	{
		for (i32 i = 0; i < numCmds; i++)
		{
			// A command can arrive more than once, and only counts the first time.
			if (cmds[i].cmdNum <= this->lastCmdNum)
			{
				continue;
			}
			this->lastCmdNum = cmds[i].cmdNum;
			const u64 pressed = GetNewlyPressedButtons(cmds[i].base().buttons_pb());
			if (pressed & IN_RELOAD)
			{
				this->ToggleVisibility();
			}
			if (pressed & IN_USE)
			{
				this->ToggleCursor();
			}
		}
	}

	bool ControlsWindow::ToggleCursor()
	{
		if (!this->player->uiService->IsOpen(this) && (!this->IsWatching() || this->controlsHidden))
		{
			return false;
		}
		this->player->uiService->Toggle(this, KZ::ui::OpenMode::Push);
		return true;
	}

	void ControlsWindow::OpenCursorForReplay()
	{
		this->cursorReplayId = currentReplayId;
	}

	bool ControlsWindow::ToggleVisibility()
	{
		if (!this->IsWatching())
		{
			return false;
		}
		this->controlsHidden = !this->controlsHidden;
		if (this->controlsHidden)
		{
			// Update hides the bar on the next tick and gives up the cursor with it.
			this->cursorWhenHidden = this->player->uiService->IsOpen(this);
		}
		else if (this->cursorWhenHidden)
		{
			this->cursorWhenHidden = false;
			this->player->uiService->Open(this, KZ::ui::OpenMode::Push);
		}
		return true;
	}

	bool ControlsWindow::CanOpen()
	{
		if (this->player->hudService->IsEditingHud() || !this->IsWatching() || this->controlsHidden)
		{
			return false;
		}
		CCSCustomHudLayout *entity = this->EnsureLayout();
		return entity && entity->GetPlayerLayoutState(this->player->GetPlayerSlot());
	}

	void ControlsWindow::OnOpen()
	{
		if (CCSCustomHudLayout *entity = this->layout.Get())
		{
			this->RenderCursor(entity);
		}
	}

	void ControlsWindow::OnClose(KZ::ui::CloseReason reason)
	{
		if (CCSCustomHudLayout *entity = this->layout.Get())
		{
			this->RenderCursor(entity);
		}
	}

	void ControlsWindow::Hide()
	{
		if (CCSCustomHudLayout *entity = this->layout.Get())
		{
			KZ::ui::SetBoolClass(entity, "rp_root", "hidden", this->applied.rootHidden, true);
		}
	}

	void ControlsWindow::Render()
	{
		CCSCustomHudLayout *entity = this->layout.Get();
		if (!entity)
		{
			return;
		}
		auto replay = data::GetCurrentReplay();
		KZ::ui::SetBoolClass(entity, "rp_root", "hidden", this->applied.rootHidden, false);
		// A tick after the font changes, so the labels measure their new text in the new font rather than the old one.
		if (this->textsChanged)
		{
			this->textsChanged = false;
			this->textMark = !this->textMark;
			this->RenderTexts();
		}
		// Reading the preferences every tick is too slow, so the style is only applied again once one of them changes.
		if (this->styleChanged)
		{
			this->styleChanged = false;
			const char *font = this->applied.style.font;
			KZ::ui::ApplyWindowStyle(this->player, entity, "rp_root", this->applied.style);
			// The first texts can reach the client before the font class does, so they are sent again for the first font too.
			this->textsChanged = font != this->applied.style.font;
		}
		this->RenderCursor(entity);
		this->RenderStatic(entity);

		char text[80];
		char time[32];
		char totalTime[32];
		utils::FormatTime(replay->currentTick * ENGINE_FIXED_TICK_INTERVAL, time, sizeof(time));
		utils::FormatTime((replay->tickCount - 1) * ENGINE_FIXED_TICK_INTERVAL, totalTime, sizeof(totalTime));
		V_snprintf(text, sizeof(text), "%s / %s", time, totalTime);
		this->layout.SetVar("rp_time", "rptime", text);
		V_snprintf(text, sizeof(text), "%u / %u", replay->currentTick, replay->tickCount - 1);
		this->layout.SetVar("rp_tick", "rptick", text);

		const i32 position = GetBarPosition(replay->currentTick, replay->tickCount);
		SetValueClass(entity, "rp_fill", "rp-w", this->applied.fill, position / 10);
		SetValueClass(entity, "rp_fill_fine", "rp-fw", this->applied.fillFine, position % 10);
		KZ::ui::SetBoolClass(entity, "rp_root", "paused", this->applied.paused, replay->replayPaused);
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_SPEEDS; i++)
		{
			KZ::ui::SetBoolClass(entity, SpeedPanel(i), "selected", this->applied.speedSelected[i], fabsf(replay->timescale - SPEEDS[i]) < 0.001f);
		}
		// A run that starts on the first tick has no breather to skip.
		KZ::ui::SetBoolClass(entity, "rp_breather", "hidden", this->applied.breatherHidden, runStartTick < 0 || GetEventSeekTick(runStartTick) == 0);
		KZ::ui::SetBoolClass(entity, "rp_breather", "disabled", this->applied.breatherDisabled,
							 runStartTick < 0 || replay->currentTick >= GetEventSeekTick(runStartTick));
		KZ::ui::SetBoolClass(entity, "rp_skip_pause", "hidden", this->applied.skipPauseHidden, pauseSegments.empty());
		KZ::ui::SetBoolClass(entity, "rp_skip_pause", "disabled", this->applied.skipPauseDisabled, FindPauseSkipTick(replay->currentTick) < 0);
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_SKIPS; i++)
		{
			KZ::ui::SetBoolClass(entity, SKIPS[i].panelId, "disabled", this->applied.skipDisabled[i], !CanSkip(replay, SKIPS[i].seconds));
		}
		KZ::ui::SetBoolClass(entity, "rp_tick_back", "disabled", this->applied.stepBackDisabled, !CanStepBack(replay));
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_ZONE_KINDS; i++)
		{
			KZ::ui::SetBoolClass(entity, ZONE_JUMPS[i].prevPanel, "disabled", this->applied.zonePrevDisabled[i],
								 FindZoneTick(replay->currentTick, i, false) < 0);
			KZ::ui::SetBoolClass(entity, ZONE_JUMPS[i].nextPanel, "disabled", this->applied.zoneNextDisabled[i],
								 FindZoneTick(replay->currentTick, i, true) < 0);
		}
		KZ::ui::SetBoolClass(entity, "rp_tick_next", "disabled", this->applied.stepForwardDisabled, !CanStepForward(replay));
	}

	void ControlsWindow::RenderCursor(CCSCustomHudLayout *entity)
	{
		const bool cursor = this->player->uiService->GetTop() == this;
		const bool changed = this->applied.cursor != cursor;
		KZ::ui::SetBoolClass(entity, "rp_root", "cursor", this->applied.cursor, cursor);
		// RenderTexts writes it the first time, and after that it only changes with the cursor.
		if (this->applied.staticRendered && changed)
		{
			this->RenderCursorText();
		}
	}

	void ControlsWindow::RenderCursorText()
	{
		const char *key = this->applied.cursor ? "Replay Controls - Hide Cursor" : "Replay Controls - Show Cursor";
		this->SetText("rp_cursor_label", "rpcursor", this->player->languageService->PrepareMessage(key).c_str());
	}

	// A word joiner is invisible, so adding or dropping one gives every label a new text without changing what it shows.
	void ControlsWindow::SetText(const char *panelId, const char *var, const char *text)
	{
		std::string value = text;
		if (this->textMark)
		{
			value += "\xE2\x81\xA0";
		}
		this->layout.SetVar(panelId, var, value.c_str());
	}

	void ControlsWindow::RenderTexts()
	{
		KZLanguageService *language = this->player->languageService;
		for (const auto &text : TEXTS)
		{
			this->SetText(text[0], text[1], language->PrepareMessage(text[2]).c_str());
		}
		for (const auto &text : LITERAL_TEXTS)
		{
			this->SetText(text[0], text[1], text[2]);
		}
		this->SetText("rp_player", "rpplayer", KZ::replaysystem::GetPlayerName());
		this->SetText("rp_cursor_key", "rpkey", "%use%");
		this->SetText("rp_visibility_key", "rpvkey", "%reload%");
		this->RenderCursorText();
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_ZONE_MARKERS && i < (i32)zoneMarkers.size(); i++)
		{
			const ZoneMarker &marker = zoneMarkers[i];
			char time[32];
			utils::FormatTime(marker.time, time, sizeof(time));
			this->SetText(ZoneLabel(i), ZoneVar(i), language->PrepareMessage(marker.phraseKey, marker.index, time).c_str());
		}
	}

	// Writes everything that stays the same for the whole replay, once per layout.
	void ControlsWindow::RenderStatic(CCSCustomHudLayout *entity)
	{
		if (this->applied.staticRendered)
		{
			return;
		}
		this->applied.staticRendered = true;
		auto replay = data::GetCurrentReplay();
		this->RenderTexts();
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_ZONE_MARKERS; i++)
		{
			const bool used = i < (i32)zoneMarkers.size();
			if (used)
			{
				const ZoneMarker &marker = zoneMarkers[i];
				const i32 position = GetBarPosition(marker.tick, replay->tickCount);
				SetValueClass(entity, ZoneAtPanel(i), "rp-w", this->applied.zoneX[i], position / 10);
				SetValueClass(entity, ZoneFinePanel(i), "rp-fw", this->applied.zoneFine[i], position % 10);
				KZ::ui::SetSwapClass(entity, ZonePanel(i), this->applied.zoneType[i], marker.typeClass);
			}
			KZ::ui::SetBoolClass(entity, ZonePosPanel(i), "hidden", this->applied.zoneHidden[i], !used);
		}
		// Only the kinds the replay has get previous and next buttons, and none at all hides the row.
		bool anyKind = false;
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_ZONE_KINDS; i++)
		{
			KZ::ui::SetBoolClass(entity, ZONE_JUMPS[i].groupPanel, "hidden", this->applied.zoneJumpHidden[i], jumpTicks[i].empty());
			anyKind |= !jumpTicks[i].empty();
		}
		KZ::ui::SetBoolClass(entity, "rp_zone_jumps", "hidden", this->applied.zoneJumpsHidden, !anyKind);

		const i64 runTicks[2] = {runStartTick, runEndTick};
		const char *const runPanels[2][3] = {{"rp_start_pos", "rp_start_at", "rp_start_fine"}, {"rp_end_pos", "rp_end_at", "rp_end_fine"}};
		for (i32 i = 0; i < 2; i++)
		{
			if (runTicks[i] >= 0)
			{
				const i32 position = GetBarPosition((u32)runTicks[i], replay->tickCount);
				SetValueClass(entity, runPanels[i][1], "rp-w", this->applied.runMarkAt[i], position / 10);
				SetValueClass(entity, runPanels[i][2], "rp-fw", this->applied.runMarkFine[i], position % 10);
			}
			KZ::ui::SetBoolClass(entity, runPanels[i][0], "hidden", this->applied.runMarkHidden[i], runTicks[i] < 0);
		}
	}

	void ControlsWindow::OnClick(const char *buttonId)
	{
		if (!this->IsWatching())
		{
			return;
		}
		auto replay = data::GetCurrentReplay();
		if (KZ_STREQ(buttonId, "rp_visibility"))
		{
			this->ToggleVisibility();
			return;
		}
		if (KZ_STREQ(buttonId, "rp_cursor"))
		{
			this->player->uiService->Close(this);
			return;
		}
		if (KZ_STREQ(buttonId, "rp_play"))
		{
			replay->replayPaused = !replay->replayPaused;
			return;
		}
		if (KZ_STREQ(buttonId, "rp_tick_back"))
		{
			if (CanStepBack(replay))
			{
				Seek(this->player, (i64)replay->currentTick - 1, true);
			}
			return;
		}
		if (KZ_STREQ(buttonId, "rp_tick_next"))
		{
			if (CanStepForward(replay))
			{
				playback::StepTick();
			}
			return;
		}
		if (KZ_STREQ(buttonId, "rp_breather"))
		{
			if (runStartTick >= 0 && replay->currentTick < GetEventSeekTick(runStartTick))
			{
				Seek(this->player, GetEventSeekTick(runStartTick), replay->replayPaused);
			}
			return;
		}
		if (KZ_STREQ(buttonId, "rp_skip_pause"))
		{
			const i64 tick = FindPauseSkipTick(replay->currentTick);
			if (tick >= 0)
			{
				Seek(this->player, tick, replay->replayPaused);
			}
			return;
		}
		if (KZ_STREQ(buttonId, "rp_start") || KZ_STREQ(buttonId, "rp_end"))
		{
			const i64 tick = KZ_STREQ(buttonId, "rp_start") ? runStartTick : runEndTick;
			if (tick >= 0)
			{
				Seek(this->player, GetEventSeekTick(tick), replay->replayPaused);
			}
			return;
		}
		for (const auto &skip : SKIPS)
		{
			if (KZ_STREQ(buttonId, skip.panelId))
			{
				if (CanSkip(replay, skip.seconds))
				{
					Seek(this->player, (i64)replay->currentTick + (i64)skip.seconds * (i64)ENGINE_FIXED_TICK_RATE, replay->replayPaused);
				}
				return;
			}
		}
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_PERCENT_MARKERS; i++)
		{
			if (KZ_STREQ(buttonId, PercentPanel(i)))
			{
				Seek(this->player, (i64)(replay->tickCount - 1) * i / KZ_REPLAY_CONTROLS_PERCENT_MARKERS, replay->replayPaused);
				return;
			}
		}
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_ZONE_KINDS; i++)
		{
			const bool prev = KZ_STREQ(buttonId, ZONE_JUMPS[i].prevPanel);
			if (prev || KZ_STREQ(buttonId, ZONE_JUMPS[i].nextPanel))
			{
				const i64 tick = FindZoneTick(replay->currentTick, i, !prev);
				if (tick >= 0)
				{
					Seek(this->player, tick, replay->replayPaused);
				}
				return;
			}
		}
		for (i32 i = 0; i < MIN((i32)zoneMarkers.size(), KZ_REPLAY_CONTROLS_ZONE_MARKERS); i++)
		{
			if (KZ_STREQ(buttonId, ZonePanel(i)))
			{
				Seek(this->player, GetEventSeekTick(zoneMarkers[i].tick), replay->replayPaused);
				return;
			}
		}
		for (i32 i = 0; i < KZ_REPLAY_CONTROLS_SPEEDS; i++)
		{
			if (KZ_STREQ(buttonId, SpeedPanel(i)))
			{
				replay->timescale = SPEEDS[i];
				return;
			}
		}
	}
} // namespace KZ::replaysystem::controls

SCMD(kz_rpcontrols, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!player)
	{
		return true;
	}
	if (!player->uiService->GetWindow<ControlsWindow>()->ToggleVisibility())
	{
		player->languageService->PrintChat(true, false, "Replay Controls - Not Watching");
	}
	return true;
}
