#include "kz/hud/layout/layout.h"
#include "kz/global/kz_global.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/timer/kz_timer.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

// How often the rows are rebuilt, in seconds.
#define MHUD_COURSE_REFRESH_TIME 0.5

enum CourseRow
{
	COURSE_ROW_ALL,
	COURSE_ROW_PRO,
};

static_assert(COURSE_ROW_PRO + 1 == MHUD_COURSE_ROW_COUNT, "one row per record");

// Each record row's panels, from the row itself to the gap from the viewer's personal best.
static_global const char *const COURSE_ROWS[MHUD_COURSE_ROW_COUNT][6] = {
	{"mhud_ci_r0", "mhud_ci_l0", "mhud_ci_t0", "mhud_ci_b0", "mhud_ci_n0", "mhud_ci_g0"},
	{"mhud_ci_r1", "mhud_ci_l1", "mhud_ci_t1", "mhud_ci_b1", "mhud_ci_n1", "mhud_ci_g1"},
};

static_global const char *const COURSE_ROW_PHRASES[MHUD_COURSE_ROW_COUNT] = {"HUD - Course All", "HUD - Course Pro"};

// Slots in LayoutCourseState::texts.
#define COURSE_TEXT_MAP    0
#define COURSE_TEXT_STATUS 1
#define COURSE_TEXT_COURSE 2
#define COURSE_TEXT_ROWS   3
#define COURSE_TEXT_STATE  (COURSE_TEXT_ROWS + MHUD_COURSE_ROW_COUNT * 5)

// The course's ranked state in the player's mode, as the API reports it.
struct CourseStateDef
{
	KZ::api::Map::Course::Filter::State state;
	const char *phrase;
	const char *className;
};

static_global const CourseStateDef COURSE_STATES[] = {
	{KZ::api::Map::Course::Filter::State::Ranked, "HUD - Course Ranked", "ci-ranked"},
	{KZ::api::Map::Course::Filter::State::Pending, "HUD - Course Pending", "ci-pending"},
	{KZ::api::Map::Course::Filter::State::Unranked, "HUD - Course Unranked", "ci-unranked"},
};

struct CourseInfoText
{
	std::string map;
	std::string status;
	const char *statusClass {};
	std::string course;
	std::string state; // empty off global maps, where there is none
	const char *stateClass {};
	std::string times[MHUD_COURSE_ROW_COUNT];
	std::string badges[MHUD_COURSE_ROW_COUNT];
	const char *badgeClasses[MHUD_COURSE_ROW_COUNT] {};
	std::string names[MHUD_COURSE_ROW_COUNT];
	const char *nameClasses[MHUD_COURSE_ROW_COUNT] {};
	std::string gaps[MHUD_COURSE_ROW_COUNT];
};

// Fills one record row the way the overlay draws it.
static_function void FormatRecordRow(KZLanguageService *lang, CourseInfoText &out, i32 row, f64 time, bool teleports, const std::string &holder,
									 f64 pb)
{
	if (time <= 0)
	{
		out.times[row] = lang->PrepareMessage("HUD - Course No Records");
		out.badgeClasses[row] = "hidden";
		return;
	}
	out.times[row] = utils::FormatTime(time).Get();
	out.badges[row] = lang->PrepareMessage(teleports ? "HUD - Course Badge TP" : "HUD - Course Badge Pro");
	out.badgeClasses[row] = teleports ? "ci-badge-tp" : "ci-badge-pro";
	out.names[row] = holder.empty() ? std::string() : lang->PrepareMessage("HUD - Course By", holder.c_str());
	// The overlay colors the holder green when the viewer holds the record.
	out.nameClasses[row] = pb > 0 && pb <= time ? "ci-own" : NULL;
	if (pb > time)
	{
		out.gaps[row] = lang->PrepareMessage("HUD - Course Gap", KZTimerService::FormatDiffTime(pb - time).Get());
	}
}

static_function void GetCourseInfo(KZPlayer *viewer, KZPlayer *source, CourseInfoText &out)
{
	KZLanguageService *lang = viewer->languageService;
	out.map = g_pKZUtils->GetCurrentMapName().Get();

	// Before the timer has started on a course, the map's first one stands in for it.
	const KZCourseDescriptor *course = source->timerService->GetCourse();
	if (!course)
	{
		course = KZ::course::GetFirstCourse();
	}
	const KZModeManager::ModePluginInfo modeInfo = KZ::mode::GetModeInfo(source->modeService);
	const bool classic = modeInfo.id == KZ::mode::GetModeInfo(KZ::api::Mode::Classic).id;

	i32 nubTier = 0, proTier = 0;
	KZGlobalService::WithCurrentMapState(
		[&](const std::optional<KZ::api::Map> &map, bool confirmed)
		{
			// Like the overlay, a map is non-global until the API confirms it.
			const bool global = confirmed && map;
			out.status = lang->PrepareMessage(global ? "HUD - Course Global" : "HUD - Course Non Global");
			out.statusClass = global ? "ci-global" : "ci-nonglobal";
			if (!global || !course)
			{
				return;
			}
			for (const KZ::api::Map::Course &globalCourse : map->courses)
			{
				if (globalCourse.id == course->globalDatabaseID)
				{
					const KZ::api::Map::Course::Filter &filter = classic ? globalCourse.filters.classic : globalCourse.filters.vanilla;
					nubTier = (i32)filter.nubTier;
					proTier = (i32)filter.proTier;
					for (const CourseStateDef &def : COURSE_STATES)
					{
						if (def.state == filter.state)
						{
							out.state = lang->PrepareMessage(def.phrase);
							out.stateClass = def.className;
						}
					}
					break;
				}
			}
		});

	if (!course)
	{
		for (i32 i = 0; i < MHUD_COURSE_ROW_COUNT; i++)
		{
			FormatRecordRow(lang, out, i, 0, false, "", 0);
		}
		return;
	}

	out.course = lang->PrepareMessage("HUD - Course Line", course->name, modeInfo.shortModeName.Get());
	if (nubTier > 0)
	{
		out.course +=
			nubTier == proTier ? lang->PrepareMessage("HUD - Course Tier", nubTier) : lang->PrepareMessage("HUD - Course Tiers", nubTier, proTier);
	}

	// World records where there are any, and the server's records and personal bests otherwise.
	const PBDataKey key = ToPBDataKey(modeInfo.id, course->guid);
	const PBData *wr = KZTimerService::GetCachedRecord(key, true);
	const bool global = wr && (wr->overall.pbTime > 0 || wr->pro.pbTime > 0);
	const PBData *record = global ? wr : KZTimerService::GetCachedRecord(key, false);
	const PBData *pb = source->timerService->GetCachedPB(key, global);
	// The overall record only counts as a pro run when it is also the pro record.
	const f64 overall = record ? record->overall.pbTime : 0;
	const f64 pro = record ? record->pro.pbTime : 0;
	FormatRecordRow(lang, out, COURSE_ROW_ALL, overall, overall != pro, record ? record->overall.holder : "", pb ? pb->overall.pbTime : 0);
	FormatRecordRow(lang, out, COURSE_ROW_PRO, pro, false, record ? record->pro.holder : "", pb ? pb->pro.pbTime : 0);
}

static_function void GetCourseSample(KZLanguageService *lang, CourseInfoText &out)
{
	out.map = "kz_longest_map_name_v2";
	out.status = lang->PrepareMessage("HUD - Course Global");
	out.statusClass = "ci-global";
	out.course = lang->PrepareMessage("HUD - Course Line", "Main", "CKZ") + lang->PrepareMessage("HUD - Course Tiers", 7, 8);
	out.state = lang->PrepareMessage("HUD - Course Unranked");
	out.stateClass = "ci-unranked";
	for (i32 i = 0; i < MHUD_COURSE_ROW_COUNT; i++)
	{
		FormatRecordRow(lang, out, i, 3599.999, i == COURSE_ROW_ALL, "WWWWWWWWWWWWWWWW", 7199.998);
	}
}

void KZHUDService::ClearCoursePreview(CCSCustomHudLayout *layout)
{
	LayoutCourseState &ci = this->layoutCourse;
	ci.preview = false;
	ci.nextRefresh = 0;
}

void KZHUDService::UpdateCourseElement(CCSCustomHudLayout *layout, KZPlayer *source, bool show, bool force, bool preview)
{
	LayoutCourseState &ci = this->layoutCourse;
	if (force)
	{
		ci = LayoutCourseState();
	}
	this->UpdateLayoutElement(layout, MHUDElement::Course, show, NULL, MHUD_DEF_BASE_COLOR, force);
	// A hidden panel keeps whatever it last showed, and a stale preview is cleared the next time it shows for real.
	if (!show)
	{
		return;
	}
	const MHUDPrefs &prefs = preview ? this->GetOwnPrefs() : this->GetPrefs();

	// js-scale.css scales the whole panel by the element's size in percent.
	const i32 scale = Clamp((i32)this->GetLayoutSize(MHUDElement::Course), MHUD_JS_SIZE_MIN, MHUD_JS_SIZE_MAX);
	if (ci.scale != scale)
	{
		const char *panelId = MHUD_ELEMENTS[(i32)MHUDElement::Course].panelId;
		char className[32];
		if (ci.scale != INT_MIN)
		{
			V_snprintf(className, sizeof(className), "js-scale--%i", ci.scale);
			layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_DoesNotHaveClass);
		}
		V_snprintf(className, sizeof(className), "js-scale--%i", scale);
		layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_HasClass);
		ci.scale = scale;
	}

	// A label only picks up a new font when it is restyled itself, so the class goes on every label.
	const char *fontClass = KZHUDService::GetMHUDFontClass(this->player, MHUDElement::Course);
	if (ci.fontClass != fontClass)
	{
		auto setFont = [&](const char *panelId)
		{
			if (ci.fontClass)
			{
				layout->SetHasClass(panelId, ci.fontClass, k_eHudPanelClassStatus_DoesNotHaveClass);
			}
			layout->SetHasClass(panelId, fontClass, k_eHudPanelClassStatus_HasClass);
		};
		for (const char *panelId : {"mhud_ci_map", "mhud_ci_status", "mhud_ci_course", "mhud_ci_state"})
		{
			setFont(panelId);
		}
		for (const auto &row : COURSE_ROWS)
		{
			for (i32 i = 1; i < KZ_ARRAYSIZE(row); i++)
			{
				setFont(row[i]);
			}
		}
		ci.fontClass = fontClass;
	}

	auto setHidden = [&](const char *panelId, i32 &cache, bool hidden)
	{
		if (cache != (i32)hidden)
		{
			cache = hidden;
			layout->SetHasClass(panelId, "hidden", hidden ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
		}
	};
	setHidden("mhud_ci_head", ci.headHidden, !prefs.courseMap);
	setHidden(COURSE_ROWS[COURSE_ROW_ALL][0], ci.rowHidden[COURSE_ROW_ALL], !prefs.courseRecords);
	setHidden(COURSE_ROWS[COURSE_ROW_PRO][0], ci.rowHidden[COURSE_ROW_PRO], !prefs.courseRecords || !prefs.coursePro);

	auto setText = [&](i32 slot, const char *panelId, const std::string &text)
	{
		if (ci.texts[slot] != text)
		{
			ci.texts[slot] = text;
			layout->SetDialogVariableString(panelId, "v", text.c_str());
		}
	};
	KZLanguageService *lang = this->player->languageService;
	auto apply = [&](const CourseInfoText &info)
	{
		setText(COURSE_TEXT_MAP, "mhud_ci_map", info.map);
		setText(COURSE_TEXT_STATUS, "mhud_ci_status", info.status);
		setText(COURSE_TEXT_COURSE, "mhud_ci_course", info.course);
		this->SetLayoutClass(layout, "mhud_ci_status", ci.statusClass, info.statusClass);
		setText(COURSE_TEXT_STATE, "mhud_ci_state", info.state);
		this->SetLayoutClass(layout, "mhud_ci_state", ci.stateClass, info.stateClass);
		for (i32 i = 0; i < MHUD_COURSE_ROW_COUNT; i++)
		{
			const i32 slot = COURSE_TEXT_ROWS + i * 5;
			setText(slot, COURSE_ROWS[i][1], lang->PrepareMessage(COURSE_ROW_PHRASES[i]));
			setText(slot + 1, COURSE_ROWS[i][2], info.times[i]);
			setText(slot + 2, COURSE_ROWS[i][3], info.badges[i]);
			setText(slot + 3, COURSE_ROWS[i][4], info.names[i]);
			setText(slot + 4, COURSE_ROWS[i][5], info.gaps[i]);
			this->SetLayoutClass(layout, COURSE_ROWS[i][3], ci.badgeClasses[i], info.badgeClasses[i]);
			this->SetLayoutClass(layout, COURSE_ROWS[i][4], ci.nameClasses[i], info.nameClasses[i]);
		}
	};

	if (preview)
	{
		if (!ci.preview)
		{
			ci.preview = true;
			CourseInfoText info;
			GetCourseSample(lang, info);
			apply(info);
		}
		return;
	}
	if (ci.preview)
	{
		this->ClearCoursePreview(layout);
	}

	const f64 now = g_pKZUtils->GetServerGlobals()->curtime;
	if (now >= ci.nextRefresh)
	{
		ci.nextRefresh = now + MHUD_COURSE_REFRESH_TIME;
		CourseInfoText info;
		GetCourseInfo(this->player, source, info);
		apply(info);
	}
}
