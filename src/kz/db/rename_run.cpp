#include "kz_db.h"
#include "queries/times.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

using namespace KZ::Database;

void KZDatabaseService::UpdateRunUUID(const char *oldUUID, const char *newUUID, TransactionSuccessCallbackFunc onSuccess,
									  TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}

	char query[256];
	Transaction txn;
	V_snprintf(query, sizeof(query), sql_times_update_id, newUUID, oldUUID);
	txn.queries.push_back(query);
	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess ? onSuccess : OnGenericTxnSuccess,
																   onFailure ? onFailure : OnGenericTxnFailure);
}
