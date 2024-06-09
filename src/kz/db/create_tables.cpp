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
			txn.AddQuery(mysql_players_create);
			txn.AddQuery(mysql_modes_create);
			txn.AddQuery(mysql_styles_create);
			txn.AddQuery(mysql_maps_create);
			txn.AddQuery(mysql_mapcourses_create);
			txn.AddQuery(mysql_times_create);
			txn.AddQuery(mysql_jumpstats_create);
			txn.AddQuery(mysql_startpos_create);
			break;
		}
		case DatabaseType_SQLite:
		{
			txn.AddQuery(sqlite_players_create);
			txn.AddQuery(sqlite_modes_create);
			txn.AddQuery(sqlite_styles_create);
			txn.AddQuery(sqlite_maps_create);
			txn.AddQuery(sqlite_mapcourses_create);
			txn.AddQuery(sqlite_times_create);
			txn.AddQuery(sqlite_jumpstats_create);
			txn.AddQuery(sqlite_startpos_create);
			break;
		}
	}
	GetDatabaseConnection()->ExecuteTransaction(&txn, OnGenericTxnSuccess, OnGenericTxnFailure);
}
