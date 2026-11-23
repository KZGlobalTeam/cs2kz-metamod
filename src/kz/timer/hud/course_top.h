#pragma once
#include "kz/global/api.h"
#include "kz/mappingapi/hud/course_chips.h"
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

#include <optional>

#define KZ_COURSE_TOP_LAYOUT "panorama/layout/custom_game/cs2kz/course_top.xml"
// Must match the record rows in course_top.xml.
#define KZ_COURSE_TOP_ROWS 10

// A course leaderboard with the record and the player's own run above a page of records.
class CourseTopWindow : public KZ::ui::Window
{
public:
	enum Scope
	{
		Global,
		Server,
		SCOPE_COUNT
	};

	enum Type
	{
		Overall,
		Pro,
		TYPE_COUNT
	};

	struct Query
	{
		std::string map;
		std::string course;
		std::string mode;
		u64 offset {};
		bool local {};
		bool global {};
	};

	struct Run
	{
		// Also the UUID of its replay.
		// Old server runs may have none.
		std::string id;
		// Global runs always have a replay and server runs only if this server has the file.
		bool replay {};
		std::string name;
		f64 time {};
		u64 teleports {};
		f64 points = -1;
		u32 maxRank {};
		u64 steamId {};
	};

	struct Result
	{
		bool received[SCOPE_COUNT] {};
		std::string map;
		std::string course;
		std::string mode;
		u64 offset {};
		std::vector<Run> runs[SCOPE_COUNT][TYPE_COUNT];
	};

	CourseTopWindow(KZPlayer *player) : player(player) {}

	// Opens the window for a sent request.
	// Returns the serial its result must carry or 0 if the window can't open.
	u32 Begin(const Query &query, KZ::ui::OpenMode openMode = KZ::ui::OpenMode::Replace);
	// Opens a course leaderboard on top of the open windows.
	void ShowCourse(const std::string &map, const std::string &course, KZ::api::Mode mode, bool pro, bool server);
	void OnResult(u32 serial, const Result &result);

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
	// Sends a request whose reply comes back through OnResult.
	// Defined in course_top.cpp next to the request.
	static void SendQuery(KZPlayer *player, const Query &query, u32 serial);

	void Navigate(const std::string &course, const std::string &mode, u64 offset);
	void QueryCards();
	// Fetches another map's course chips and workshop ID from the API.
	// The course names come from the server's database instead if the API doesn't know the map.
	void QueryMapInfo();
	// Another map's course names from the server's database or at least the shown course.
	std::vector<std::string> GetMapCourseNames();
	bool IsCurrentMap();
	u64 GetWorkshopId();
	void Hide();
	void Render();
	void RenderCard(CCSCustomHudLayout *entity, i32 card, const std::optional<KZ::api::Record> &record);

	KZPlayer *player;
	KZ::ui::PlayerLayout layout {this->player, KZ_COURSE_TOP_LAYOUT, "kzmaptop"};
	KZ::course::ChipRows chips {"maptop"};

	Query query;
	Result result;
	bool hasResult {};
	bool loading {};
	u32 serial {};
	Scope scope = Global;
	Type type = Overall;

	// The record and own cards are queried once per course and mode.
	std::string cardKey;
	u32 cardSerial {};
	std::optional<KZ::api::Record> worldRecord[TYPE_COUNT];
	std::optional<KZ::api::Record> ownRecord[TYPE_COUNT];

	// Key of the fetched map details if the shown map isn't the current one.
	std::string mapInfoKey;
	u32 mapInfoSerial {};
	std::optional<KZ::api::Map> mapInfo;
	std::vector<std::string> mapCourseNames;

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
		bool scopeHidden {};
		bool cardHidden[2] {};
		bool cardWr[2] {};
		bool cardProfileStatic[2] {};
		bool statusHidden {};
		bool rowHidden[KZ_COURSE_TOP_ROWS] {};
		const char *rowBadge[KZ_COURSE_TOP_ROWS] {};
		const char *rowPoints[KZ_COURSE_TOP_ROWS] {};
		bool rowReplayDisabled[KZ_COURSE_TOP_ROWS] {};
		bool rowCopyHidden[KZ_COURSE_TOP_ROWS] {};
		bool rowProfileStatic[KZ_COURSE_TOP_ROWS] {};
		bool workshopHidden {};
		bool prevDisabled {};
		bool nextDisabled {};
	} applied;
};
