#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "queries/course_top.h"

void KZDatabaseService::QueryAllRecords(CUtlString mapName, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	std::string cleanedMapName = KZDatabaseService::GetDatabaseConnection()->Escape(mapName.Get());

	Transaction txn;

	char query[1024];
	// Get PB
	V_snprintf(query, sizeof(query), sql_getsrs, cleanedMapName.c_str());
	txn.queries.push_back(query);

	// Get Rank
	V_snprintf(query, sizeof(query), sql_getsrspro, cleanedMapName.c_str());
	txn.queries.push_back(query);

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::QueryRecords(CUtlString mapName, CUtlString courseName, u32 modeID, u32 count, u32 offset,
									 TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	std::string cleanedMapName = KZDatabaseService::GetDatabaseConnection()->Escape(mapName.Get());
	std::string cleanedCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(courseName.Get());

	Transaction txn;

	char query[1024];
	// Get PB
	V_snprintf(query, sizeof(query), sql_getcoursetop, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, count, offset);
	txn.queries.push_back(query);

	// Get Rank
	V_snprintf(query, sizeof(query), sql_getcoursetoppro, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, count, offset);
	txn.queries.push_back(query);

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}
