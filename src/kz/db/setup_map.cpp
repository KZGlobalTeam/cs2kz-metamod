#include "kz_db.h"
#include "queries/maps.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

/*
	Inserts the map information into the database.
	Retrieves the MapID of the map and stores it in a global variable.
*/

using namespace KZ::Database;

static_global bool mapSetUp = false;
i32 KZDatabaseService::currentMapID {};

bool KZDatabaseService::IsMapSetUp()
{
	return mapSetUp;
}

void KZDatabaseService::SetupMap()
{
	mapSetUp = false;
	if (!KZDatabaseService::IsReady())
	{
		META_CONPRINTF("[KZ::DB] Warning: SetupMap called too early.\n");
		return;
	}

	Transaction txn;
	char query[1024];
	CUtlString mapName = g_pKZUtils->GetServerGlobals()->mapname.ToCStr();
	auto escapedMapName = KZDatabaseService::GetDatabaseConnection()->Escape(mapName.Get());
	auto databaseType = KZDatabaseService::GetDatabaseType();
	switch (databaseType)
	{
		case DatabaseType::SQLite:
		{
			V_snprintf(query, sizeof(query), sqlite_maps_insert, escapedMapName.c_str());
			txn.queries.push_back(query);
			V_snprintf(query, sizeof(query), sqlite_maps_update, escapedMapName.c_str());
			txn.queries.push_back(query);
			break;
		}
		case DatabaseType::MySQL:
		{
			V_snprintf(query, sizeof(query), mysql_maps_upsert, escapedMapName.c_str());
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

	V_snprintf(query, sizeof(query), sql_maps_findid, escapedMapName.c_str(), escapedMapName.c_str());
	txn.queries.push_back(query);
	// clang-format off
	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(
		txn, 
		[databaseType, mapName](std::vector<ISQLQuery *> queries) 
		{
			auto currentMapName = g_pKZUtils->GetServerGlobals()->mapname.ToCStr();
			if (!KZ_STREQ(currentMapName, mapName.Get()))
			{
				META_CONPRINTF("[KZ::DB] Failed to setup map, current map name %s doesn't match %s!\n", currentMapName, mapName.Get());
				return;
			}
			switch (databaseType)
			{
				case DatabaseType::SQLite:
				{
					auto resultSet = queries[2]->GetResultSet();
					if (resultSet->FetchRow())
					{
						KZDatabaseService::currentMapID = resultSet->GetInt(0);
					}
					break;
				}
				case DatabaseType::MySQL:
				{
					auto resultSet = queries[1]->GetResultSet();
					if (resultSet->FetchRow())
					{
						KZDatabaseService::currentMapID = resultSet->GetInt(0);
					}
					break;
				}
				default:
				{
					// This shouldn't happen.
					break;
				}
			}
			mapSetUp = true;
			META_CONPRINTF("[KZ::DB] Map setup successful for %s, current map ID: %i\n", currentMapName, KZDatabaseService::currentMapID);
			CALL_FORWARD(eventListeners, OnMapSetup);
		},
		OnGenericTxnFailure);
	// clang-format on
}
