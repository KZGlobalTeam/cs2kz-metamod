#pragma once
#include "kz/global/api.h"
#include "kz/ui/player_layout.h"

// Must match the chip slots written out per row in each layout.
#define KZ_COURSE_CHIPS 64
// Ranked courses go in the first row and all others in the second.
#define KZ_COURSE_CHIP_ROWS 2

struct KZCourseDescriptor;

namespace KZ::course
{
	// A layout's course chips in two rows with the ids <prefix>_ranked and <prefix>_unranked.
	// Each row has a _label and KZ_COURSE_CHIPS chip buttons <row>_chip<i> holding _name and _tier labels.
	// Slot i belongs to the course at index i.
	class ChipRows
	{
	public:
		ChipRows(const char *prefix) : prefix(prefix) {}

		// Clears the cached classes after the layout entity is respawned.
		void Reset()
		{
			this->applied = Applied();
		}

		// Shows the current map's courses with their tiers and ranked states in a mode and marks one as current.
		void Render(KZPlayer *player, KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, KZ::api::Mode mode, bool proTier,
					const KZCourseDescriptor *current);
		// Shows another map's courses using its API data.
		// Slot i is map.courses[i] and `current` names the course to mark.
		void RenderMap(KZPlayer *player, KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, const KZ::api::Map &map, KZ::api::Mode mode,
					   bool proTier, const char *current);
		// Shows course names without API data in the unranked row.
		void RenderNames(KZPlayer *player, KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, const std::vector<std::string> &names,
						 const char *current);
		void Hide(CCSCustomHudLayout *entity);
		// The course behind a clicked chip or nullptr if the button isn't one.
		const KZCourseDescriptor *GetClickedCourse(const char *buttonId);
		// The slot of a clicked chip or -1 if the button isn't one.
		i32 GetClickedSlot(const char *buttonId);

	private:
		const char *GetRowId(i32 row);
		const char *GetSlotId(i32 row, i32 i, const char *suffix = "");
		// Hides slot i if `name` is null and shows it in the row of its state otherwise.
		// A course without API data counts as unranked.
		void RenderSlot(KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, i32 i, const char *name, const KZ::api::Map::Course *apiCourse,
						KZ::api::Mode mode, bool proTier, bool current, bool rowUsed[]);
		void RenderRowLabels(KZPlayer *player, KZ::ui::PlayerLayout &layout, CCSCustomHudLayout *entity, const bool rowUsed[]);

		const char *prefix;
		char idBuffer[64] {};

		struct Applied
		{
			bool rowHidden[KZ_COURSE_CHIP_ROWS] {};
			bool chipHidden[KZ_COURSE_CHIP_ROWS][KZ_COURSE_CHIPS] {};
			bool chipCurrent[KZ_COURSE_CHIP_ROWS][KZ_COURSE_CHIPS] {};
			bool chipBare[KZ_COURSE_CHIP_ROWS][KZ_COURSE_CHIPS] {};
			const char *tierClass[KZ_COURSE_CHIP_ROWS][KZ_COURSE_CHIPS] {};
			const char *stateClass[KZ_COURSE_CHIP_ROWS][KZ_COURSE_CHIPS] {};

			Applied()
			{
				for (auto &row : this->chipHidden)
				{
					for (bool &hidden : row)
					{
						hidden = true;
					}
				}
			}
		} applied;
	};
} // namespace KZ::course
