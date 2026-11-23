#pragma once
#include "kz/global/api.h"
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#define KZ_LEADERBOARD_LAYOUT "panorama/layout/custom_game/cs2kz/leaderboard.xml"
// Must match the rows in leaderboard.xml.
#define KZ_LEADERBOARD_ROWS 10

// The global players ranked by their rating in a mode.
class LeaderboardWindow : public KZ::ui::Window
{
public:
	LeaderboardWindow(KZPlayer *player) : player(player) {}

	// Returns false if the window can't open.
	bool Show(KZ::api::Mode mode);

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

	struct Entry
	{
		u64 steamId {};
		std::string name;
		// Indexed by mode in switch order (CKZ, VNL).
		f64 rating[2] {};

		// Parses a player returned by the API's /players endpoint.
		bool FromJson(const Json &json);
	};

private:
	enum class State
	{
		Loading,
		Loaded,
		Failed,
	};

	void Request();
	void Hide();
	void Render();

	KZPlayer *player;
	KZ::ui::PlayerLayout layout {this->player, KZ_LEADERBOARD_LAYOUT, "kzleaderboard"};

	KZ::api::Mode mode = KZ::api::Mode::Classic;
	u64 offset {};
	// Bumped by every request so a reply that arrives after another request is dropped.
	u32 serial {};
	State state = State::Loading;
	std::vector<Entry> entries;
	u64 total {};

	struct Applied
	{
		KZ::ui::WindowStyle style {};
		bool rootHidden {true};
		bool modeSelected[2] {};
		const char *modePick {};
		bool rowHidden[KZ_LEADERBOARD_ROWS] {};
		const char *ratingClass[KZ_LEADERBOARD_ROWS] {};
		const char *titleClass[KZ_LEADERBOARD_ROWS] {};
		bool statusHidden {};
		bool prevDisabled {};
		bool nextDisabled {};

		// Matches the initial classes in leaderboard.xml.
		Applied()
		{
			for (bool &hidden : this->rowHidden)
			{
				hidden = true;
			}
		}
	} applied;
};
