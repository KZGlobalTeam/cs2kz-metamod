#include "kz/mappingapi/hud/course_chips.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/global/kz_global.h"
#include "kz/language/kz_language.h"

#include "tier0/memdbgon.h"

static_global const char *const rowNames[KZ_COURSE_CHIP_ROWS] = {"ranked", "unranked"};
static_global const char *const rowPhrases[KZ_COURSE_CHIP_ROWS] = {"Courses - Ranked", "Courses - Unranked"};

static_global const char *const tierClasses[] = {
	"tier-1", "tier-2", "tier-3", "tier-4", "tier-5", "tier-6", "tier-7", "tier-8", "tier-9", "tier-10",
};

struct CourseStateClass
{
	KZ::api::Map::Course::Filter::State state;
	const char *className;
};

static_global const CourseStateClass courseStateClasses[] = {
	{KZ::api::Map::Course::Filter::State::Unranked, "state-unranked"},
	{KZ::api::Map::Course::Filter::State::Pending, "state-pending"},
	{KZ::api::Map::Course::Filter::State::Ranked, "state-ranked"},
};

static_function const char *GetTierClass(i32 tier)
{
	return tier >= 1 && tier <= (i32)KZ_ARRAYSIZE(tierClasses) ? tierClasses[tier - 1] : nullptr;
}

static_function const char *GetStateClass(KZ::api::Map::Course::Filter::State state)
{
	for (const CourseStateClass &entry : courseStateClasses)
	{
		if (entry.state == state)
		{
			return entry.className;
		}
	}
	return nullptr;
}

namespace KZ::course
{
	const char *ChipRows::GetRowId(i32 row)
	{
		V_snprintf(this->idBuffer, sizeof(this->idBuffer), "%s_%s", this->prefix, rowNames[row]);
		return this->idBuffer;
	}

	const char *ChipRows::GetSlotId(i32 row, i32 i, const char *suffix)
	{
		V_snprintf(this->idBuffer, sizeof(this->idBuffer), "%s_%s_chip%i%s", this->prefix, rowNames[row], i, suffix);
		return this->idBuffer;
	}

	void ChipRows::RenderSlot(KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, i32 i, const char *name,
							  const KZ::api::Map::Course *apiCourse, KZ::api::Mode mode, bool proTier, bool current, bool rowUsed[])
	{
		if (!name)
		{
			for (i32 row = 0; row < KZ_COURSE_CHIP_ROWS; row++)
			{
				KZ::ui::SetBoolClass(entity, this->GetSlotId(row, i), "hidden", this->applied.chipHidden[row][i], true);
			}
			return;
		}

		std::string tier;
		const char *tierClass = nullptr;
		const char *stateClass = nullptr;
		// A course the API doesn't know is not ranked.
		i32 row = 1;
		if (apiCourse)
		{
			const KZ::api::Map::Course::Filter &filter = mode == KZ::api::Mode::Classic ? apiCourse->filters.classic : apiCourse->filters.vanilla;
			const i32 tierNumber = (i32)(proTier ? filter.proTier : filter.nubTier);
			tier = std::to_string(tierNumber);
			tierClass = GetTierClass(tierNumber);
			stateClass = GetStateClass(filter.state);
			row = filter.state == KZ::api::Map::Course::Filter::State::Ranked ? 0 : 1;
		}

		// A course keeps its slot number and shows in the row of its state only.
		rowUsed[row] = true;
		for (i32 other = 0; other < KZ_COURSE_CHIP_ROWS; other++)
		{
			KZ::ui::SetBoolClass(entity, this->GetSlotId(other, i), "hidden", this->applied.chipHidden[other][i], other != row);
		}

		// Without API data a chip shows only the course name.
		KZ::ui::SetBoolClass(entity, this->GetSlotId(row, i), "current", this->applied.chipCurrent[row][i], current);
		KZ::ui::SetBoolClass(entity, this->GetSlotId(row, i), "bare", this->applied.chipBare[row][i], !apiCourse);
		KZ::ui::SetSwapClass(entity, this->GetSlotId(row, i), this->applied.stateClass[row][i], stateClass);
		KZ::ui::SetSwapClass(entity, this->GetSlotId(row, i, "_tier"), this->applied.tierClass[row][i], tierClass);
		std::string nameId = this->GetSlotId(row, i, "_name");
		layout.SetVar(nameId.c_str(), nameId.c_str(), name);
		std::string tierId = this->GetSlotId(row, i, "_tier");
		layout.SetVar(tierId.c_str(), tierId.c_str(), tier.c_str());
	}

	void ChipRows::RenderRowLabels(KZPlayer *player, KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, const bool rowUsed[])
	{
		for (i32 row = 0; row < KZ_COURSE_CHIP_ROWS; row++)
		{
			KZ::ui::SetBoolClass(entity, this->GetRowId(row), "hidden", this->applied.rowHidden[row], !rowUsed[row]);
			std::string labelId = std::string(this->GetRowId(row)) + "_label";
			layout.SetVar(labelId.c_str(), labelId.c_str(), player->languageService->PrepareMessage(rowPhrases[row]).c_str());
		}
	}

	void ChipRows::Render(KZPlayer *player, KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, KZ::api::Mode mode, bool proTier,
						  const KZCourseDescriptor *current)
	{
		std::vector<KZ::api::Map::Course> apiCourses;
		KZGlobalService::WithCurrentMap(
			[&](const std::optional<KZ::api::Map> &currentMap)
			{
				if (currentMap)
				{
					apiCourses = currentMap->courses;
				}
			});

		bool rowUsed[KZ_COURSE_CHIP_ROWS] {};
		for (i32 i = 0; i < KZ_COURSE_CHIPS; i++)
		{
			const KZCourseDescriptor *course = KZ::course::GetCourseByIndex(i);
			const KZ::api::Map::Course *apiCourse = nullptr;
			for (const KZ::api::Map::Course &candidate : apiCourses)
			{
				if (course && candidate.id == course->globalDatabaseID)
				{
					apiCourse = &candidate;
					break;
				}
			}
			this->RenderSlot(layout, entity, i, course ? course->name : nullptr, apiCourse, mode, proTier, course && course == current, rowUsed);
		}
		this->RenderRowLabels(player, layout, entity, rowUsed);
	}

	void ChipRows::RenderMap(KZPlayer *player, KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, const KZ::api::Map &map, KZ::api::Mode mode,
							 bool proTier, const char *current)
	{
		bool rowUsed[KZ_COURSE_CHIP_ROWS] {};
		for (i32 i = 0; i < KZ_COURSE_CHIPS; i++)
		{
			const KZ::api::Map::Course *course = i < (i32)map.courses.size() ? &map.courses[i] : nullptr;
			const bool isCurrent = course && current && KZ_STREQI(course->name.c_str(), current);
			this->RenderSlot(layout, entity, i, course ? course->name.c_str() : nullptr, course, mode, proTier, isCurrent, rowUsed);
		}
		this->RenderRowLabels(player, layout, entity, rowUsed);
	}

	void ChipRows::RenderNames(KZPlayer *player, KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, const std::vector<std::string> &names,
							   const char *current)
	{
		bool rowUsed[KZ_COURSE_CHIP_ROWS] {};
		for (i32 i = 0; i < KZ_COURSE_CHIPS; i++)
		{
			const char *name = i < (i32)names.size() ? names[i].c_str() : nullptr;
			const bool isCurrent = name && current && KZ_STREQI(name, current);
			this->RenderSlot(layout, entity, i, name, nullptr, KZ::api::Mode::Classic, false, isCurrent, rowUsed);
		}
		this->RenderRowLabels(player, layout, entity, rowUsed);
	}

	void ChipRows::Hide(CCSCustomHudLayout *entity)
	{
		for (i32 row = 0; row < KZ_COURSE_CHIP_ROWS; row++)
		{
			KZ::ui::SetBoolClass(entity, this->GetRowId(row), "hidden", this->applied.rowHidden[row], true);
		}
	}

	i32 ChipRows::GetClickedSlot(const char *buttonId)
	{
		for (i32 row = 0; row < KZ_COURSE_CHIP_ROWS; row++)
		{
			for (i32 i = 0; i < KZ_COURSE_CHIPS; i++)
			{
				if (KZ_STREQ(buttonId, this->GetSlotId(row, i)))
				{
					return i;
				}
			}
		}
		return -1;
	}

	const KZCourseDescriptor *ChipRows::GetClickedCourse(const char *buttonId)
	{
		const i32 slot = this->GetClickedSlot(buttonId);
		return slot >= 0 ? KZ::course::GetCourseByIndex(slot) : nullptr;
	}
} // namespace KZ::course
