#include "kz_db.h"
#include "queries/modes.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

using namespace KZ::Database;

void KZDatabaseService::GetModeID(CUtlString modeName)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	Transaction txn;
	char query[1024];
	switch (KZDatabaseService::GetDatabaseType())
	{
		case DatabaseType_SQLite:
		{
			V_snprintf(query, sizeof(query), sqlite_modes_insert, modeName.Get());
			break;
		}
		case DatabaseType_MySQL:
		{
			V_snprintf(query, sizeof(query), mysql_modes_insert, modeName.Get());
			break;
		}
		default:
		{
			// Should never happen.
			query[0] = 0;
		}
	}
	txn.queries.push_back(query);

	V_snprintf(query, sizeof(query), sql_modes_findid, modeName.Get());
	txn.queries.push_back(query);
	// clang-format off
	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(
		txn, 
		[modeName](std::vector<ISQLQuery *> queries) 
		{ 
			KZ::mode::UpdateModeDatabaseID(modeName, queries[1]->GetResultSet()->GetInt(0));
		},
		OnGenericTxnFailure);
	// clang-format on
}
