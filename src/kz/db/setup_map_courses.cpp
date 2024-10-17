#include "kz_db.h"
#include "kz/course/kz_course.h"

#include "queries/courses.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

using namespace KZ::Database;

static_global bool coursesSetUp = false;

bool KZDatabaseService::AreCoursesSetUp()
{
	return coursesSetUp;
}

void KZDatabaseService::SetupCourses(CUtlVector<KZCourse> &courses)
{
	char query[1024];
	Transaction txn;
	FOR_EACH_VEC(courses, i)
	{
		KZCourse &course = courses[i];
		std::string cleanCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(course.GetName());
		switch (databaseType)
		{
			case DatabaseType::SQLite:
			{
				V_snprintf(query, sizeof(query), sqlite_mapcourses_insert, KZDatabaseService::GetMapID(), cleanCourseName.c_str(), course.id);
				txn.queries.push_back(query);
				break;
			}
			case DatabaseType::MySQL:
			{
				V_snprintf(query, sizeof(query), mysql_mapcourses_insert, KZDatabaseService::GetMapID(), cleanCourseName.c_str(), course.id);
				txn.queries.push_back(query);
				break;
			}
			default:
			{
				// This shouldn't happen.
				query[0] = 0;
				break;
			}
		}
	}
	V_snprintf(query, sizeof(query), sql_mapcourses_findall, KZDatabaseService::GetMapID());
	txn.queries.push_back(query);
	// clang-format off
	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(
		txn,
		[](std::vector<ISQLQuery *> queries) 
		{
			auto resultSet = queries.back()->GetResultSet();
			while (resultSet->FetchRow())
			{
				const char* name = resultSet->GetString(0);
				KZ::course::UpdateCourseLocalID(name, resultSet->GetInt(1));
				META_CONPRINTF("[KZ::DB] Course '%s' registered with ID %i\n", name, resultSet->GetInt(1));
			}
			coursesSetUp = true;
		},
		OnGenericTxnFailure);
	// clang-format on
}
