#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "queries/courses.h"
using namespace KZ::Database;

static_global bool coursesSetUp = false;

bool KZDatabaseService::AreCoursesSetUp()
{
	return coursesSetUp;
}

void KZDatabaseService::SetupCourses(CUtlVector<KZ::timer::CourseInfo> &courseInfos)
{
	char query[1024];
	Transaction txn;
	FOR_EACH_VEC(courseInfos, i)
	{
		auto courseInfo = courseInfos[i];
		std::string cleanCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(courseInfo.courseName.Get());
		switch (databaseType)
		{
			case DatabaseType::SQLite:
			{
				V_snprintf(query, sizeof(query), sqlite_mapcourses_insert, KZDatabaseService::GetMapID(), cleanCourseName.c_str(),
						   courseInfo.stageID);
				txn.queries.push_back(query);
				break;
			}
			case DatabaseType::MySQL:
			{
				V_snprintf(query, sizeof(query), mysql_mapcourses_insert, KZDatabaseService::GetMapID(), cleanCourseName.c_str(), courseInfo.stageID);
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
				KZ::timer::CourseInfo info;
				if (!KZ::timer::GetCourseInformation(name, info))
				{
					continue;
				}
				KZ::timer::UpdateCourseDatabaseID(info.uid, resultSet->GetInt(1));
				META_CONPRINTF("[KZDB] Course '%s' registered with ID %i\n", info.courseName.Get(), resultSet->GetInt(1));
			}
			coursesSetUp = true;
		},
		OnGenericTxnFailure);
	// clang-format on
}
