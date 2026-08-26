#pragma once
#include "kz/kz.h"

#include <string>
#include <vector>

class CCSCustomHudLayout;
class KZMenuPage;

// Row label helpers.
std::string KZMenuPhrase(KZPlayer *player, const char *key);
std::string KZMenuToggleLabel(KZPlayer *player, const char *phraseKey, bool on);
std::string KZMenuValueLabel(KZPlayer *player, const char *phraseKey, const char *value);

// Keep in step with the row count in the mhud addon's menu.xml.
#define KZ_MENU_ROWS 20

struct KZMenuContext
{
	i32 index {};
	i32 sub {};
};

struct KZMenuRow
{
	std::string label;
	const char *colorClass {}; // NULL: the player's menu color
	KZMenuPage *submenu {};    // non-NULL: a click opens it with param as its context
	KZMenuContext param {};
	bool disabled {}; // drawn greyed out and not clickable
};

// One screen of the options menu. `context` is the param of the row that opened the page.
class KZMenuPage
{
public:
	virtual ~KZMenuPage() {}

	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) = 0;

	virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) {}

	virtual void OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param) {}

	// A stepper page draws the -5/-1/+1/+5 block instead of the row list.
	virtual bool IsStepper() const
	{
		return false;
	}

	virtual bool HasVerticalStep() const
	{
		return false;
	}

	virtual std::string GetStepReadout(KZPlayer *player, KZMenuContext context)
	{
		return "";
	}

	virtual void OnStep(KZPlayer *player, KZMenuContext context, bool vertical, i32 delta) {}

	virtual void OnEnter(KZPlayer *player, KZMenuContext context) {}

	virtual void OnLeave(KZPlayer *player, KZMenuContext context) {}
};

class KZMenuService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static void Init();
	// Adds a row to the root page. Registration order is row order.
	static void RegisterCategory(KZMenuPage *page, KZMenuContext context = {});

	virtual void Reset() override;

	void Toggle();
	void Close();

	bool IsOpen() const
	{
		return this->open;
	}

	void OpenPage(KZMenuPage *page, KZMenuContext context);
	void GoBack();
	void Refresh();

	static void OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId);
	// The plugin is going away with the click handler in it, so nothing may stay captured.
	static void Cleanup();

private:
	void Render();
	void PickRow(i32 row);
	void ApplyStep(bool vertical, i32 delta);
	void DropCapture();

	struct Frame
	{
		KZMenuPage *page {};
		KZMenuContext context {};
		i32 scroll {};
	};

	bool open {};
	std::vector<Frame> stack;
	// Bumped by anything that changes the page, so a row action that navigated is not re-rendered.
	i32 navSerial {};

	// TODO: Is this really necessary?
	struct WrittenState
	{
		std::string title {};
		std::string step {};
		std::string rowText[KZ_MENU_ROWS] {};
	} written;

	// Every default has to match what menu.xml ships.
	struct AppliedClasses
	{
		const char *rowColor[KZ_MENU_ROWS] {};
		const char *fontClass {};
		bool rowHidden[KZ_MENU_ROWS] {};
		bool rowDisabled[KZ_MENU_ROWS] {};
		bool rootHidden {true};
		bool rowsHidden {};
		bool stepHidden {true};
		bool vStepHidden {true};
		bool prevHidden {true};
		bool nextHidden {true};

		AppliedClasses()
		{
			for (i32 i = 0; i < KZ_MENU_ROWS; i++)
			{
				rowHidden[i] = true;
			}
		}
	} applied;

	// A new map means a new entity holding none of those classes.
	CHandle<CBaseEntity> layoutEntity {};

	// The page as it is on screen, so a click routes without rebuilding it.
	KZMenuPage *rowPage[KZ_MENU_ROWS] {};
	KZMenuContext rowParam[KZ_MENU_ROWS] {};
	bool rowUsed[KZ_MENU_ROWS] {};
};
