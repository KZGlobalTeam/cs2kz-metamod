#pragma once
#include "kz/mappingapi/hud/course_chips.h"
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#define KZ_COURSES_LAYOUT "panorama/layout/custom_game/cs2kz/courses.xml"

class CoursesWindow : public KZ::ui::Window
{
public:
	CoursesWindow(KZPlayer *player) : player(player) {}

	// Closes the list if it is on top and opens it otherwise.
	// Returns false if the list cannot open.
	bool Toggle();

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
	bool IsShown();
	void Hide();
	void Render();
	u64 GetWorkshopId();

	KZPlayer *player;
	KZ::ui::PlayerLayout layout {this->player, KZ_COURSES_LAYOUT, "kzcourses"};
	KZ::course::ChipRows chips {"courses"};

	struct Applied
	{
		KZ::ui::WindowStyle style {};
		bool rootHidden {true};
		bool metaHidden {true};
		bool workshopHidden {true};
		const char *stateClass {};
		bool creditHidden {};
		bool descriptionHidden {true};
	} applied;
};
