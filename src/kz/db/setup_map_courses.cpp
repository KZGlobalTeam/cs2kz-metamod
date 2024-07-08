#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "queries/courses.h"
using namespace KZ::Database;

std::unordered_map<std::string, i32> KZDatabaseService::courses;

i32 KZDatabaseService::GetCourseID(const char *courseName)
{
	if (courses.count(courseName) == 0)
	{
		return -1;
	}
	return courses[courseName];
}

void KZDatabaseService::ClearCourses()
{
	KZDatabaseService::courses.clear();
}

void KZDatabaseService::SetupCourse(const char *courseName)
{
	char query[1024];
	courses[courseName] = -1;
	Transaction txn;
	auto cleanCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(courseName);
	switch (databaseType)
	{
		case DatabaseType_SQLite:
		{
			V_snprintf(query, sizeof(query), sqlite_mapcourses_insert, KZDatabaseService::GetMapID(), cleanCourseName.c_str());
			txn.queries.push_back(query);
			break;
		}
		case DatabaseType_MySQL:
		{
			V_snprintf(query, sizeof(query), mysql_mapcourses_insert, KZDatabaseService::GetMapID(), cleanCourseName.c_str());
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
	V_snprintf(query, sizeof(query), sql_mapcourses_findid, KZDatabaseService::GetMapID(), cleanCourseName.c_str());
	txn.queries.push_back(query);
	// clang-format off
	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(
		txn, 
		[cleanCourseName](std::vector<ISQLQuery *> queries) 
		{
			auto resultSet = queries[1]->GetResultSet();
			if (resultSet->FetchRow())
			{
				courses[cleanCourseName.c_str()] = resultSet->GetInt(0);
				META_CONPRINTF("[KZDB] Course '%s' registered with ID %i\n", cleanCourseName.c_str(), courses[cleanCourseName.c_str()]);
			}
		},
		OnGenericTxnFailure);
	// clang-format on
}

void KZDatabaseService::SetupCourses()
{
	ClearCourses();
	SetupCourse("Main");
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
