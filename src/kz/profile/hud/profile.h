#pragma once
#include "kz/global/api.h"
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#include <optional>

#define KZ_PROFILE_LAYOUT "panorama/layout/custom_game/cs2kz/profile.xml"
// Must match the run rows in profile.xml.
#define KZ_PROFILE_ROWS 10

// A player's global or server profile and a page of their latest personal bests.
class ProfileWindow : public KZ::ui::Window
{
public:
	ProfileWindow(KZPlayer *player) : player(player) {}

	// True if either the API or the server's database can provide profiles.
	static bool IsAvailable();

	// Opens a player's profile on top of the open windows.
	// `server` opens the server profile instead of the global one.
	void ShowSteamId(u64 steamId, KZ::api::Mode mode, bool server = false);
	// Opens the caller's profile if `query` is empty and otherwise the profile of the SteamID or name it holds.
	// A name matches players in the server first and then the highest-rated player whose name contains it.
	// Returns false if the window can't open.
	bool ShowQuery(const char *query);

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
	enum Type
	{
		Overall,
		Pro,
		TYPE_COUNT
	};

	enum Scope
	{
		Global,
		Server,
		SCOPE_COUNT
	};

	enum class State
	{
		Loading,
		Loaded,
		NotFound,
		Failed,
	};

	struct Profile
	{
		u64 steamId {};
		std::string name;
		// Indexed by mode in switch order (CKZ, VNL).
		// A rank of 0 means unranked.
		f64 rating[2] {};
		u32 globalRank[2] {};
		std::string joined;

		// Parses a player returned by the API's /players endpoints.
		bool FromJson(const Json &json);
	};

	bool Begin(const std::string &identifier, KZ::api::Mode mode, KZ::ui::OpenMode openMode, Scope scope);
	void RequestPlayer();
	void RequestRuns();
	void RequestServerPlayer();
	void RequestServerRuns();
	void Hide();
	void Render();
	bool HasReplay(i32 row);

	KZPlayer *player;
	KZ::ui::PlayerLayout layout {this->player, KZ_PROFILE_LAYOUT, "kzprofile"};

	// A SteamID64 or a name for the API to search for.
	std::string identifier;
	KZ::api::Mode mode = KZ::api::Mode::Classic;
	Type type = Overall;
	Scope scope = Global;
	u64 offset {};

	u32 playerSerial {};
	State playerState = State::Loading;
	std::optional<Profile> profile;

	u32 runsSerial {};
	State runsState = State::Loading;
	std::vector<KZ::api::Record> runs;
	// Global runs always have a replay.
	// Server runs only have one if this server has the replay file.
	std::vector<bool> runReplays;
	u64 total {};

	struct Applied
	{
		KZ::ui::WindowStyle style {};
		bool rootHidden {true};
		bool closeBack {};
		bool modeSelected[2] {};
		bool typeSelected[TYPE_COUNT] {};
		bool scopeSelected[SCOPE_COUNT] {};
		const char *modePick {};
		const char *typePick {};
		const char *scopePick {};
		bool scopeHidden {true};
		bool globalLineHidden {true};
		const char *globalRankClass {};
		bool rankLineHidden {};
		bool runsServer {};
		bool cardHidden {true};
		bool statusHidden {};
		const char *rankClass {};
		bool runsHidden {true};
		bool rowHidden[KZ_PROFILE_ROWS] {};
		const char *rowTier[KZ_PROFILE_ROWS] {};
		const char *rowBadge[KZ_PROFILE_ROWS] {};
		const char *rowPoints[KZ_PROFILE_ROWS] {};
		const char *rowRank[KZ_PROFILE_ROWS] {};
		bool rowReplayDisabled[KZ_PROFILE_ROWS] {};
		bool rowCopyHidden[KZ_PROFILE_ROWS] {};
		bool runsStatusHidden {};
		bool prevDisabled {};
		bool nextDisabled {};

		// Matches the initial classes in profile.xml.
		Applied()
		{
			for (bool &hidden : this->rowHidden)
			{
				hidden = true;
			}
		}
	} applied;
};
