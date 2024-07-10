#include "kz_db.h"
#include "kz/style/kz_style.h"
#include "queries/styles.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

using namespace KZ::Database;

void KZDatabaseService::UpdateStyleIDs()
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	// clang-format off
	KZDatabaseService::GetDatabaseConnection()->Query(sql_styles_fetch_all,
		[](ISQLQuery *query)
		{
			auto resultSet = query->GetResultSet();
			while (resultSet->FetchRow())
			{
				KZ::style::UpdateStyleDatabaseID(query->GetResultSet()->GetString(1), query->GetResultSet()->GetInt(0));
			}
		});
	// clang-format on
}

void KZDatabaseService::InsertAndUpdateStyleIDs(CUtlString styleName, CUtlString shortName)
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
			V_snprintf(query, sizeof(query), sqlite_styles_insert, styleName.Get(), shortName.Get());
			break;
		}
		case DatabaseType::MySQL:
		{
			V_snprintf(query, sizeof(query), mysql_styles_insert, styleName.Get(), shortName.Get());
			break;
		}
		default:
		{
			// Should never happen.
			query[0] = 0;
		}
	}
	txn.queries.push_back(query);

	V_snprintf(query, sizeof(query), sql_styles_findid, styleName.Get());
	txn.queries.push_back(query);
	// clang-format off
	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(
		txn, 
		[styleName](std::vector<ISQLQuery *> queries) 
		{
			auto resultSet = queries[1]->GetResultSet();
			if (resultSet->FetchRow())
			{
				KZ::style::UpdateStyleDatabaseID(styleName, queries[1]->GetResultSet()->GetInt(0));
			}
		},
		OnGenericTxnFailure);
	// clang-format on
}
