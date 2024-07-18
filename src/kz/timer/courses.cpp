#include "kz/timer/kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz_timer.h"

using namespace KZ::timer;
static_global CUtlVector<CourseInfo> courses;
static_global u32 internalCourseCount = 0;

void KZ::timer::ClearCourses()
{
	courses.RemoveAll();
}

void KZ::timer::InsertCourse(const char *courseName, i32 stageID)
{
	internalCourseCount++;
	courses.AddToTail({internalCourseCount, courseName, stageID, -1});
}

void KZ::timer::SetupCourses()
{
	ClearCourses();
	KZ::timer::InsertCourse("Main", 1);
	if (KZDatabaseService::IsMapSetUp())
	{
		KZDatabaseService::SetupCourses(courses);
	}
	// TODO: Loop through timer_start_zone entities, check their mapping api data (course name), then insert the course into the database.
	/*
	 * for entities in map
	 * {
	 *	get course name from entities
	 *	add it to courses with default value
	 *	call setupcourse
	 * }
	 */
	// Get every course names and id and store it in the map.
}

void KZ::timer::UpdateCourseDatabaseID(u32 uid, i32 databaseID)
{
	FOR_EACH_VEC(courses, i)
	{
		if (courses[i].uid == uid)
		{
			courses[i].databaseID = databaseID;
			return;
		}
	}
}

bool KZ::timer::GetCourseInformation(const char *courseName, CourseInfo &info)
{
	FOR_EACH_VEC(courses, i)
	{
		if (courses[i].courseName == courseName)
		{
			info = courses[i];
			return true;
		}
	}
	return false;
}

bool KZ::timer::GetFirstCourseInformation(CourseInfo &info)
{
	i32 minStageID = INT_MAX;
	FOR_EACH_VEC(courses, i)
	{
		if (courses[i].stageID < minStageID)
		{
			info = courses[i];
			return true;
		}
	}
	return false;
}
