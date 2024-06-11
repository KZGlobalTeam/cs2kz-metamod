#include "kz_db.h"

#include "kz/option/kz_option.h"
#include "queries/courses.h"
#include "queries/jumpstats.h"
#include "queries/maps.h"
#include "queries/modes.h"
#include "queries/players.h"
#include "queries/styles.h"
#include "queries/startpos.h"
#include "queries/times.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "vendor/sql_mm/src/public/sqlite_mm.h"
#include "vendor/sql_mm/src/public/mysql_mm.h"

using namespace KZ::Database;

void KZDatabaseService::CreateTables()
{
	Transaction txn;
	switch (KZDatabaseService::GetDatabaseType())
	{
		case DatabaseType_MySQL:
		{
			txn.queries.push_back(mysql_players_create);
			txn.queries.push_back(mysql_modes_create);
			txn.queries.push_back(mysql_styles_create);
			txn.queries.push_back(mysql_maps_create);
			txn.queries.push_back(mysql_mapcourses_create);
			txn.queries.push_back(mysql_times_create);
			txn.queries.push_back(mysql_jumpstats_create);
			txn.queries.push_back(mysql_startpos_create);
			break;
		}
		case DatabaseType_SQLite:
		{
			txn.queries.push_back(sqlite_players_create);
			txn.queries.push_back(sqlite_modes_create);
			txn.queries.push_back(sqlite_styles_create);
			txn.queries.push_back(sqlite_maps_create);
			txn.queries.push_back(sqlite_mapcourses_create);
			txn.queries.push_back(sqlite_times_create);
			txn.queries.push_back(sqlite_jumpstats_create);
			txn.queries.push_back(sqlite_startpos_create);
			break;
		}
	}

	GetDatabaseConnection()->ExecuteTransaction(txn, OnGenericTxnSuccess, OnGenericTxnFailure);
}
