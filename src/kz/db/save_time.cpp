#include "kz_db.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"
#include "queries/save_time.h"
#include "queries/times.h"
#include "utils/uuid.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

using namespace KZ::Database;

void KZDatabaseService::SaveTime(const char *runUUID, u64 steamID, u32 courseID, i32 modeID, f64 time, u64 teleportsUsed, u64 styleIDs,
								 std::string_view metadata, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}

	char query[1024];
	Transaction txn;

	// Always use UUID insert since all migrations must be applied for the plugin to run
	V_snprintf(query, sizeof(query), sql_times_insert, runUUID, steamID, courseID, modeID, styleIDs, time, teleportsUsed, metadata.data());
	txn.queries.push_back(query);
	if (styleIDs != 0)
	{
		KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, OnGenericTxnSuccess, OnGenericTxnFailure);
	}
	else
	{
		// Get Top 2 PRO PBs
		V_snprintf(query, sizeof(query), sql_getpb, courseID, steamID, modeID, styleIDs, 2);
		txn.queries.push_back(query);
		// Get Rank
		V_snprintf(query, sizeof(query), sql_getmaprank, courseID, modeID, steamID, courseID, modeID);
		txn.queries.push_back(query);
		// Get Number of Players with Times
		V_snprintf(query, sizeof(query), sql_getlowestmaprank, courseID, modeID);
		txn.queries.push_back(query);
		if (teleportsUsed == 0)
		{
			// Get Top 2 PRO PBs
			V_snprintf(query, sizeof(query), sql_getpbpro, courseID, steamID, modeID, styleIDs, 2);
			txn.queries.push_back(query);
			// Get PRO Rank
			V_snprintf(query, sizeof(query), sql_getmaprankpro, courseID, modeID, steamID, courseID, modeID);
			txn.queries.push_back(query);
			// Get Number of Players with Times
			V_snprintf(query, sizeof(query), sql_getlowestmaprankpro, courseID, modeID);
			txn.queries.push_back(query);
		}
		KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
	}
}
