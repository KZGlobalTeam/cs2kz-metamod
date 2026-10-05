#pragma once
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#define KZ_HELP_LAYOUT "panorama/layout/custom_game/cs2kz/help.xml"

// Fixed slot counts, kept in step with help.xml. Rows cover the whole command registry.
#define KZ_HELP_CATEGORIES 20
#define KZ_HELP_ROWS       512

class HelpWindow : public KZ::ui::Window
{
public:
	HelpWindow(KZPlayer *player) : player(player) {}

	// Returns false when the UI is unavailable; the command then prints console help.
	bool Show(i32 category = 0);

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
	KZ::ui::PlayerLayout layout {this->player, KZ_HELP_LAYOUT, "kzhelp"};
	bool shown {};
	i32 category {};

	struct Applied
	{
		KZ::ui::WindowStyle style {};
		bool rootHidden {true};
		bool emptyHidden {true};
		bool categoryHidden[KZ_HELP_CATEGORIES] {};
		bool categorySelected[KZ_HELP_CATEGORIES] {};
		bool rowHidden[KZ_HELP_ROWS] {};
		bool descriptionHidden[KZ_HELP_ROWS] {};

		Applied()
		{
			for (bool &hidden : this->categoryHidden)
			{
				hidden = true;
			}
			for (bool &hidden : this->rowHidden)
			{
				hidden = true;
			}
		}
	} applied;
};
