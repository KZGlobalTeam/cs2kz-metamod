#pragma once

#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#define KZ_REPLAY_CONTROLS_LAYOUT "panorama/layout/custom_game/cs2kz/replay.xml"

// Fixed slot counts (5% steps), see replay.xml.
#define KZ_REPLAY_CONTROLS_PERCENT_MARKERS 20
// KZ_MAX_REPLAY_MARKED_ZONES, which the mapping API warns about.
#define KZ_REPLAY_CONTROLS_ZONE_MARKERS 64
#define KZ_REPLAY_CONTROLS_SPEEDS       6
#define KZ_REPLAY_CONTROLS_SKIPS        6
// The bar's width in layout pixels, see replay.css.
#define KZ_REPLAY_CONTROLS_BAR_WIDTH 960
// Splits, checkpoints, stages and teleports.
#define KZ_REPLAY_CONTROLS_ZONE_KINDS 4

class CUserCmd;

namespace KZ::replaysystem::controls
{
	// The playback bar everyone watching the replay bot in first or third person sees.
	// Update draws it every tick whether or not the window is on the UI stack.
	// The window is only on the stack while the viewer has the cursor (E), which is what lets them click the bar.
	class ControlsWindow : public KZ::ui::Window
	{
	public:
		ControlsWindow(KZPlayer *player) : player(player) {}

		void Update();
		void OnProcessUsercmds(CUserCmd *cmds, i32 numCmds);
		// Both return false when the player is not watching the replay bot.
		bool ToggleCursor();
		bool ToggleVisibility();

		// Gives the player the cursor once they are watching the replay that just started.
		void OpenCursorForReplay();

		void OnStyleChanged()
		{
			this->styleChanged = true;
		}

		virtual KZ::ui::PlayerLayout *GetLayout() override
		{
			return &this->layout;
		}

		virtual bool CanOpen() override;
		virtual void OnOpen() override;
		virtual void OnClose(KZ::ui::CloseReason reason) override;
		virtual void OnClick(const char *buttonId) override;

	private:
		bool IsWatching();
		CCSCustomHudLayout *EnsureLayout();
		void Hide();
		void Render();
		void RenderCursor(CCSCustomHudLayout *entity);
		void RenderStatic(CCSCustomHudLayout *entity);
		void RenderTexts();
		void RenderCursorText();
		void SetText(const char *panelId, const char *var, const char *text);

		KZPlayer *player;
		KZ::ui::PlayerLayout layout {this->player, KZ_REPLAY_CONTROLS_LAYOUT, "kzreplay"};
		// The replay the layout was built for.
		// Each one gets a fresh layout because every marker position is persistent in the layout entity.
		// Not to be confused with the replay UUID.
		u32 replayId {};
		i32 lastCmdNum {};
		bool styleChanged {true};
		bool textsChanged {};
		bool textMark {};
		// Hidden by the viewer, which lasts until they show it again, across replays.
		bool controlsHidden {};
		// The viewer had the cursor when they hid the bar, so showing it gives the cursor back.
		bool cursorWhenHidden {};
		// The replay to open the cursor for, or 0.
		u32 cursorReplayId {};

		struct Applied
		{
			KZ::ui::WindowStyle style {};
			bool rootHidden {true};
			bool cursor {};
			bool paused {};
			bool breatherHidden {true};
			bool breatherDisabled {};
			bool skipPauseHidden {true};
			bool skipPauseDisabled {};
			bool stepBackDisabled {};
			bool stepForwardDisabled {};
			bool skipDisabled[KZ_REPLAY_CONTROLS_SKIPS] {};
			bool zonePrevDisabled[KZ_REPLAY_CONTROLS_ZONE_KINDS] {};
			bool zoneNextDisabled[KZ_REPLAY_CONTROLS_ZONE_KINDS] {};
			bool zoneJumpHidden[KZ_REPLAY_CONTROLS_ZONE_KINDS] {true, true, true, true};
			bool zoneJumpsHidden {true};
			// The run's start and end marks.
			bool runMarkHidden[2] {true, true};
			i32 runMarkAt[2] {-1, -1};
			i32 runMarkFine[2] {-1, -1};
			bool staticRendered {};
			i32 fill {-1};
			i32 fillFine {-1};
			bool speedSelected[KZ_REPLAY_CONTROLS_SPEEDS] {};
			bool zoneHidden[KZ_REPLAY_CONTROLS_ZONE_MARKERS] {};
			i32 zoneX[KZ_REPLAY_CONTROLS_ZONE_MARKERS] {};
			i32 zoneFine[KZ_REPLAY_CONTROLS_ZONE_MARKERS] {};
			const char *zoneType[KZ_REPLAY_CONTROLS_ZONE_MARKERS] {};

			Applied()
			{
				for (i32 i = 0; i < KZ_REPLAY_CONTROLS_ZONE_MARKERS; i++)
				{
					this->zoneHidden[i] = true;
					this->zoneX[i] = -1;
					this->zoneFine[i] = -1;
				}
			}
		} applied;
	};

	void Init();
	// Collects the markers of the replay that just started.
	void OnReplayStart();
} // namespace KZ::replaysystem::controls
