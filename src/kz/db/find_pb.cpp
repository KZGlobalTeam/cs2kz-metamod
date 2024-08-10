#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "queries/personal_best.h"

void KZDatabaseService::QueryPB(u64 steamID64, CUtlString mapName, CUtlString courseName, u32 modeID, TransactionSuccessCallbackFunc onSuccess,
								TransactionFailureCallbackFunc onFailure)
{
	std::string cleanedMapName = KZDatabaseService::GetDatabaseConnection()->Escape(mapName.Get());
	std::string cleanedCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(courseName.Get());

	Transaction txn;

	char query[1024];
	// Get PB
	V_snprintf(query, sizeof(query), sql_getpb, steamID64, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, 0ull, 1);
	txn.queries.push_back(query);

	// Get Rank
	V_snprintf(query, sizeof(query), sql_getmaprank, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, steamID64, cleanedMapName.c_str(),
			   cleanedCourseName.c_str(), modeID);
	txn.queries.push_back(query);

	// Get Number of Players with Times
	V_snprintf(query, sizeof(query), sql_getlowestmaprank, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID);
	txn.queries.push_back(query);

	// Get PRO PB
	V_snprintf(query, sizeof(query), sql_getpbpro, steamID64, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, 0ull, 1);
	txn.queries.push_back(query);

	// Get PRO Rank
	V_snprintf(query, sizeof(query), sql_getmaprankpro, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, steamID64, cleanedMapName.c_str(),
			   cleanedCourseName.c_str(), modeID);
	txn.queries.push_back(query);

	// Get Number of Players with Times
	V_snprintf(query, sizeof(query), sql_getlowestmaprankpro, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID);
	txn.queries.push_back(query);

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::QueryPBRankless(u64 steamID64, CUtlString mapName, CUtlString courseName, u32 modeID, u64 styleIDFlags,
										TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	std::string cleanedMapName = KZDatabaseService::GetDatabaseConnection()->Escape(mapName.Get());

	std::string cleanedCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(courseName.Get());

	char query[1024];
	Transaction txn;
	// Get PB
	V_snprintf(query, sizeof(query), sql_getpb, steamID64, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, styleIDFlags, 1);
	txn.queries.push_back(query);
	// Get PRO PB
	V_snprintf(query, sizeof(query), sql_getpbpro, steamID64, cleanedMapName.c_str(), cleanedCourseName.c_str(), modeID, styleIDFlags, 1);
	txn.queries.push_back(query);

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}
