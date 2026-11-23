#pragma once
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#define KZ_JUMP_TOP_LAYOUT "panorama/layout/custom_game/cs2kz/jump_top.xml"
// Must match the rows in jump_top.xml.
#define KZ_JUMP_TOP_ROWS 10
// Must match the jump type switch in jump_top.xml (LJ through JB in JumpType order).
#define KZ_JUMP_TOP_TYPES 7

namespace KZ::jumpstats::hud
{
	// The best saved jump of each player on this server for a mode and jump type by distance or by block.
	class JumpTopWindow : public KZ::ui::Window
	{
	public:
		JumpTopWindow(KZPlayer *player) : player(player) {}

		// Opens the list for a jump type (index into the switch) and mode (0 for CKZ, 1 for VNL).
		// Returns false if the window can't open.
		bool Show(i32 typeIndex, i32 modeIndex, bool block);

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
		enum class State
		{
			Loading,
			Loaded,
			Failed,
		};

		struct Row
		{
			u64 steamId {};
			std::string name;
			f64 distance {};
			// 0 if the jump wasn't on a block.
			u32 block {};
			u32 strafes {};
			f64 sync {};
			f64 pre {};
			f64 max {};
			f64 airtime {};
			std::string uuid;
			// The map of the jump's replay.
			// Empty if there is no replay because the database doesn't store the map.
			std::string map;
			bool hasReplay {};
			// Only a jump replay of the current map can be watched here.
			bool playable {};
		};

		void Query();
		void Hide();
		void Render();

		KZPlayer *player;
		KZ::ui::PlayerLayout layout {this->player, KZ_JUMP_TOP_LAYOUT, "kzjumptop"};

		i32 typeIndex {};
		i32 modeIndex {};
		bool block {};
		u64 offset {};
		// Bumped by every query so a reply that arrives after another query is dropped.
		u32 serial {};
		State state = State::Loading;
		std::vector<Row> rows;
		u64 total {};

		struct Applied
		{
			KZ::ui::WindowStyle style {};
			bool rootHidden {true};
			bool modeSelected[2] {};
			const char *modePick {};
			bool listSelected[2] {};
			const char *listPick {};
			bool typeSelected[KZ_JUMP_TOP_TYPES] {};
			const char *typePick {};
			bool rowHidden[KZ_JUMP_TOP_ROWS] {};
			bool rowPlayDisabled[KZ_JUMP_TOP_ROWS] {};
			bool rowProfileStatic[KZ_JUMP_TOP_ROWS] {};
			bool statusHidden {};
			bool prevDisabled {};
			bool nextDisabled {};

			// Matches the initial classes in jump_top.xml.
			Applied()
			{
				for (bool &hidden : this->rowHidden)
				{
					hidden = true;
				}
			}
		} applied;
	};
} // namespace KZ::jumpstats::hud
