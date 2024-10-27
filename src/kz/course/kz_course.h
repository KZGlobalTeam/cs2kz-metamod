// Middleman for managing courses related stuff.

#pragma once
#include "kz/kz.h"

#define KZ_MAX_COURSE_COUNT       128
#define KZ_MAX_COURSE_NAME_LENGTH 65

struct PBData
{
	PBData()
	{
		Reset();
	}

	void Reset()
	{
		overall.pbTime = {};
		overall.pbSplitZoneTimes.FillWithValue(-1.0);
		overall.pbCpZoneTimes.FillWithValue(-1.0);
		overall.pbStageZoneTimes.FillWithValue(-1.0);
		pro.pbTime = {};
		pro.pbSplitZoneTimes.FillWithValue(-1.0);
		pro.pbCpZoneTimes.FillWithValue(-1.0);
		pro.pbStageZoneTimes.FillWithValue(-1.0);
	}

	struct
	{
		f64 pbTime {};
		CUtlVectorFixed<f64, KZ_MAX_SPLIT_ZONES> pbSplitZoneTimes;
		CUtlVectorFixed<f64, KZ_MAX_CHECKPOINT_ZONES> pbCpZoneTimes;
		CUtlVectorFixed<f64, KZ_MAX_STAGE_ZONES> pbStageZoneTimes;
	} overall, pro;
};

// Convert mode and course ID to one single value.
typedef u64 PBDataKey;

inline PBDataKey ToPBDataKey(u32 modeID, u32 courseID)
{
	return modeID | ((u64)courseID << 32);
}

inline void ConvertFromPBDataKey(PBDataKey key, uint32_t *modeID, uint32_t *courseID)
{
	if (modeID)
	{
		*modeID = (uint32_t)key;
	}
	if (courseID)
	{
		*courseID = (uint32_t)(key >> 32);
	}
}

struct KZCourseDescriptor;

struct KZCourse
{
	// Course's unique ID assigned by the server, used to identify course related requests.
	// This will increment by 1 every time a course is registered across the server
	KZCourse(u32 guid, i32 courseID, const char *courseName) : guid(guid), id(courseID)
	{
		V_snprintf(name, sizeof(name), "%s", courseName);
	}

	// Shared identifiers
	u32 guid {};

	// Mapper assigned course ID.
	i32 id;
	// Mapper assigned course name.
	char name[KZ_MAX_COURSE_NAME_LENGTH] {};

	bool HasMatchingIdentifiers(i32 id, const char *name) const
	{
		return this->id == id && (!V_stricmp(this->name, name));
	}

	CUtlString GetName() const
	{
		return name;
	}

	// ID used for local database.
	u32 localDatabaseID {};

	// ID used for global database.
	u32 globalDatabaseID {};

	// Mapping API descriptor reference.
	const KZCourseDescriptor *descriptor = nullptr;
};

namespace KZ
{
	namespace course
	{
		// Clear the list of current courses.
		void ClearCourses();

		// Get the number of courses on this map.
		u32 GetCourseCount();

		// Insert a new course into the course list with the associated mapper name and ID.
		// Will return nullptr if there is collision in the course name or ID.
		KZCourse *InsertCourse(i32 id, const char *name);

		// Get a course's information given its map-defined course id.
		const KZCourse *GetCourseByCourseID(i32 id);

		// Get a course's information given its local course id.
		const KZCourse *GetCourseByLocalCourseID(i32 id);

		// Get a course's information given its name.
		const KZCourse *GetCourse(const char *courseName);

		// Get a course's information given its GUID.
		const KZCourse *GetCourse(u32 guid);

		// Get the first course's information sorted by map-defined ID.
		const KZCourse *GetFirstCourse();

		// Setup all the courses to the local database.
		void SetupLocalCourses();

		// Update the course's database ID given its name.
		bool UpdateCourseLocalID(const char *courseName, u32 databaseID);

		// Update the course's global ID given its map-defined name and ID.
		bool UpdateCourseGlobalID(const char *courseName, i32 courseID, u32 globalID);
	} // namespace course
};    // namespace KZ
