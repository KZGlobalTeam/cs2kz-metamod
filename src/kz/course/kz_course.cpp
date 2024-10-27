#include "kz_course.h"
#include "kz/db/kz_db.h"

#include "UtlSortVector.h"

using namespace KZ::course;

class CourseLessFunc
{
public:
	bool Less(const KZCourse &src1, const KZCourse &src2, void *pCtx)
	{
		return src1.id < src2.id;
	}
};

static_global u32 internalCourseCount = 0;
static_global CUtlSortVector<KZCourse, CourseLessFunc> courseList(KZ_MAX_COURSE_COUNT, KZ_MAX_COURSE_COUNT);

void KZ::course::ClearCourses()
{
	courseList.RemoveAll();
	KZTimerService::ClearRecordCache();
}

u32 KZ::course::GetCourseCount()
{
	return courseList.Count();
}

KZCourse *KZ::course::InsertCourse(i32 id, const char *name)
{
	KZCourse *currentCourse = nullptr;
	FOR_EACH_VEC(courseList, i)
	{
		if (courseList[i].HasMatchingIdentifiers(id, name))
		{
			// If we find a 100% match, return it immediately.
			return &courseList.Element(i);
		}
		else if (courseList[i].id == id || !V_stricmp(name, courseList[i].name))
		{
			// One of two conditions matches... not good.
			META_CONPRINTF("[KZ::course] ERROR: Course %s (ID %i) is found but %s (ID %i) is found instead!\n", name, id, courseList[i].name,
						   courseList[i].id);
			return nullptr;
		}
	}
	// Can't find anything, insert a new course.
	internalCourseCount++;
	return &courseList.Element(courseList.Insert({internalCourseCount, id, name}));
}

const KZCourse *KZ::course::GetCourseByCourseID(i32 id)
{
	FOR_EACH_VEC(courseList, i)
	{
		if (courseList[i].id == id)
		{
			return &courseList[i];
		}
	}
	return nullptr;
}

const KZCourse *KZ::course::GetCourseByLocalCourseID(i32 id)
{
	FOR_EACH_VEC(courseList, i)
	{
		if (courseList[i].localDatabaseID == id)
		{
			return &courseList[i];
		}
	}
	return nullptr;
}

const KZCourse *KZ::course::GetCourse(const char *courseName)
{
	FOR_EACH_VEC(courseList, i)
	{
		if (courseList[i].GetName() == courseName)
		{
			return &courseList[i];
		}
	}
	return nullptr;
}

const KZCourse *KZ::course::GetCourse(u32 guid)
{
	FOR_EACH_VEC(courseList, i)
	{
		if (courseList[i].guid == guid)
		{
			return &courseList[i];
		}
	}
	return nullptr;
}

const KZCourse *KZ::course::GetFirstCourse()
{
	if (courseList.Count() >= 1)
	{
		return &courseList[0];
	}
	return nullptr;
}

void KZ::course::SetupLocalCourses()
{
	if (KZDatabaseService::IsMapSetUp())
	{
		KZDatabaseService::SetupCourses(courseList);
	}
}

bool KZ::course::UpdateCourseLocalID(const char *courseName, u32 databaseID)
{
	FOR_EACH_VEC(courseList, i)
	{
		if (courseList[i].GetName() == courseName)
		{
			courseList[i].localDatabaseID = databaseID;
			return true;
		}
	}
	return false;
}

bool KZ::course::UpdateCourseGlobalID(const char *courseName, i32 courseID, u32 globalID)
{
	FOR_EACH_VEC(courseList, i)
	{
		if (courseList[i].HasMatchingIdentifiers(courseID, courseName))
		{
			courseList[i].globalDatabaseID = globalID;
			return true;
		}
	}
	return false;
}
