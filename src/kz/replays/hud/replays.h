#pragma once
#include "kz/global/api.h"
#include "kz/replays/kz_replay.h"
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"
#include "utils/uuid.h"

#define KZ_REPLAYS_LAYOUT "panorama/layout/custom_game/cs2kz/replays.xml"
// Must match the replay rows in replays.xml.
#define KZ_REPLAYS_ROWS 10
// Must match the generic cells per row in replays.xml.
// Each list picks and sizes its cells in replays.css.
#define KZ_REPLAYS_CELLS 5
// Jump types on the switch from LJ to JB in JumpType order.
#define KZ_REPLAYS_JUMP_TYPES 7

namespace KZ::replaysystem::hud
{
	// The replays on this server listed like !replays does in the console and narrowed by the same arguments.
	// Cheater replays are only listed in the console.
	class ReplaysWindow : public KZ::ui::Window
	{
	public:
		ReplaysWindow(KZPlayer *player) : player(player) {}

		// Opens the list described by the !replays arguments.
		// Returns false if the window can't open.
		bool Show(const char *args);

		virtual KZ::ui::PlayerLayout *GetLayout() override
		{
			return &this->layout;
		}

		virtual bool CanOpen() override;
		virtual void OnOpen() override;
		virtual void OnClose(KZ::ui::CloseReason reason) override;
		virtual void OnSuspend() override;
		virtual void OnResume() override;
		virtual void OnClick(const char *buttonId) override;

	private:
		struct Row
		{
			std::string uuid;
			std::string name;
			u64 steamId {};
			KZ::api::Mode mode = KZ::api::Mode::Classic;
			std::string map;
			std::string cells[KZ_REPLAYS_CELLS];
			std::string time;
			const char *badge {};
			// Only a replay of the current map can be watched here.
			bool playable {};
		};

		void Query();
		void OnPage(u64 total, const std::vector<std::pair<UUID_t, ReplayHeader>> &page);
		void Hide();
		void Render();

		KZPlayer *player;
		KZ::ui::PlayerLayout layout {this->player, KZ_REPLAYS_LAYOUT, "kzreplays"};

		// The !replays arguments are parsed again whenever the list changes because each list takes its own defaults from them.
		std::string args;
		ReplayType type = RP_RUN;
		// The player's mode unless a mode argument names another one.
		std::string mode;
		// LJ unless a jumptype argument names another type.
		u8 jumpType {};
		u64 offset {};
		std::string filters;
		std::vector<Row> rows;
		u64 total {};
		// A page from the replay store can arrive after the list changed again.
		u32 querySerial {};

		struct Applied
		{
			KZ::ui::WindowStyle style {};
			bool rootHidden {true};
			const char *listClass {};
			const char *typePick {};
			bool typeSelected[3] {};
			const char *modePick {};
			bool modeSelected[2] {};
			const char *jumpTypePick {};
			bool jumpTypeSelected[KZ_REPLAYS_JUMP_TYPES] {};
			bool rowHidden[KZ_REPLAYS_ROWS] {};
			const char *rowBadge[KZ_REPLAYS_ROWS] {};
			bool rowProfileStatic[KZ_REPLAYS_ROWS] {};
			bool rowPlayDisabled[KZ_REPLAYS_ROWS] {};
			bool statusHidden {};
			bool prevDisabled {};
			bool nextDisabled {};

			// Matches the initial classes in replays.xml.
			Applied()
			{
				for (bool &hidden : this->rowHidden)
				{
					hidden = true;
				}
			}
		} applied;
	};
} // namespace KZ::replaysystem::hud
