#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "queries/map_top.h"

void KZDatabaseService::QueryRecords(CUtlString mapName, CUtlString courseName, u32 modeID, TransactionSuccessCallbackFunc onSuccess,
									 TransactionFailureCallbackFunc onFailure)
{
	std::string cleanedMapName = KZDatabaseService::GetDatabaseConnection()->Escape(mapName.Get());
	std::string cleanedCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(courseName.Get());

	Transaction txn;

	char query[1024];
	// Get PB
	V_snprintf(query, sizeof(query), sql_getmaptop, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, 1);
	txn.queries.push_back(query);

	// Get Rank
	V_snprintf(query, sizeof(query), sql_getmaptoppro, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, 1);
	txn.queries.push_back(query);

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}
