#include "kz_db.h"
#include "kz/mode/kz_mode.h"
#include "queries/modes.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

using namespace KZ::Database;

void KZDatabaseService::UpdateModeIDs()
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	// clang-format off
	KZDatabaseService::GetDatabaseConnection()->Query(sql_modes_fetch_all,
		[](ISQLQuery *query)
		{
			auto resultSet = query->GetResultSet();
			while (resultSet->FetchRow())
			{
				KZ::mode::UpdateModeDatabaseID(query->GetResultSet()->GetString(1), query->GetResultSet()->GetInt(0));
			}
		});
	// clang-format on
}

void KZDatabaseService::InsertAndUpdateModeIDs(CUtlString modeName, CUtlString shortName)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	Transaction txn;
	char query[1024];
	switch (KZDatabaseService::GetDatabaseType())
	{
		case DatabaseType::SQLite:
		{
			V_snprintf(query, sizeof(query), sqlite_modes_insert, modeName.Get(), shortName.Get());
			break;
		}
		case DatabaseType::MySQL:
		{
			V_snprintf(query, sizeof(query), mysql_modes_insert, modeName.Get(), shortName.Get());
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
			auto resultSet = queries[1]->GetResultSet();
			while (resultSet->FetchRow())
			{
				KZ::mode::UpdateModeDatabaseID(modeName, queries[1]->GetResultSet()->GetInt(0));
			}
		},
		OnGenericTxnFailure);
	// clang-format on
}
