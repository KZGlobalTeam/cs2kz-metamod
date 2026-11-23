#pragma once
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#define KZ_WELCOME_LAYOUT "panorama/layout/custom_game/cs2kz/welcome.xml"
// Must match the pages in welcome.xml.
#define KZ_WELCOME_PAGES 4
// Must match the mode cards in welcome.xml.
#define KZ_WELCOME_MODES 2

namespace KZ::tip::hud
{
	// Explains KZ, the mode choice, the teleport menu and useful commands to new players one page at a time.
	// Opens by itself once for a player new to the server and on /tutorial.
	class WelcomeWindow : public KZ::ui::Window
	{
	public:
		WelcomeWindow(KZPlayer *player) : player(player) {}

		// Opens the window on its first page.
		// Returns false if it can't open.
		bool Show();

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
		void Hide();
		void Render();

		KZPlayer *player;
		KZ::ui::PlayerLayout layout {this->player, KZ_WELCOME_LAYOUT, "kzwelcome"};
		i32 page {};

		struct Applied
		{
			KZ::ui::WindowStyle style {};
			bool rootHidden {true};
			bool pageHidden[KZ_WELCOME_PAGES] {false, true, true, true};
			bool dotOn[KZ_WELCOME_PAGES] {true, false, false, false};
			bool modeSelected[KZ_WELCOME_MODES] {};
			bool prevDisabled {true};
		} applied;
	};
} // namespace KZ::tip::hud
