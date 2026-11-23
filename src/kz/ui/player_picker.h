#pragma once
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#include <functional>
#include <string>
#include <vector>

#define KZ_PICKER_LAYOUT "panorama/layout/custom_game/cs2kz/player_picker.xml"
// Must match the rows in player_picker.xml.
#define KZ_PICKER_ROWS 32

namespace KZ::ui
{
	// A list of players to click one of for the commands that take a player name.
	class PlayerPicker : public Window
	{
	public:
		using PickCallback = std::function<void(KZPlayer *player, KZPlayer *target)>;

		PlayerPicker(KZPlayer *player) : player(player) {}

		// Lists the candidates by name and calls onPick with the one the player clicks if they are still on the server.
		// Returns false if the window can't open.
		bool Show(const char *titlePhrase, const std::vector<KZPlayer *> &candidates, KZPlayer *selected, PickCallback onPick);

		virtual PlayerLayout *GetLayout() override
		{
			return &this->layout;
		}

		virtual bool CanOpen() override;
		virtual void OnOpen() override;
		virtual void OnClose(CloseReason reason) override;
		virtual void OnSuspend() override;
		virtual void OnResume() override;
		virtual void OnClick(const char *buttonId) override;

	private:
		struct Row
		{
			CPlayerUserId userID;
			std::string name;
			bool selected {};
		};

		void Hide();
		void Render();

		KZPlayer *player;
		PlayerLayout layout {this->player, KZ_PICKER_LAYOUT, "kzpicker"};

		const char *titlePhrase {};
		std::vector<Row> rows;
		PickCallback onPick;
		i32 page {};

		struct Applied
		{
			WindowStyle style {};
			bool rootHidden {true};
			bool rowHidden[KZ_PICKER_ROWS] {};
			bool rowSelected[KZ_PICKER_ROWS] {};
			bool navHidden {};
			bool noteHidden {true};

			// Matches the initial classes in player_picker.xml.
			Applied()
			{
				for (bool &hidden : this->rowHidden)
				{
					hidden = true;
				}
			}
		} applied;
	};
} // namespace KZ::ui
