#include "kz_db.h"
#include "kz/option/kz_option.h"

#include <regex>
#include "checksum_crc.h"

#include "queries/migrations.h"
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

static_global bool localDBConnected = false;

std::string trimString(const char *str)
{
	static_persist const std::regex pattern("\\s+");

	return std::regex_replace(str, pattern, " ");
}

// Warning: Do NOT change the content or the order of these queries! Do appends only.
// clang-format off
static_global const std::string mysqlMigrations[] = 
{
	trimString(mysql_players_create),
	trimString(mysql_modes_create),
	trimString(mysql_styles_create),
	trimString(mysql_maps_create),
	trimString(mysql_mapcourses_create),
	trimString(mysql_times_create),
	trimString(mysql_jumpstats_create),
	trimString(mysql_startpos_create),
};

static_global const std::string sqliteMigrations[] = 
{
	trimString(sqlite_players_create),
	trimString(sqlite_modes_create),
	trimString(sqlite_styles_create),
	trimString(sqlite_maps_create),
	trimString(sqlite_mapcourses_create),
	trimString(sqlite_times_create),
	trimString(sqlite_jumpstats_create),
	trimString(sqlite_startpos_create),
};

// clang-format on

void KZDatabaseService::RunMigrations()
{
	Transaction txn;
	switch (KZDatabaseService::GetDatabaseType())
	{
		case DatabaseType::MySQL:
		{
			txn.queries.push_back(mysql_migrations_create);
			break;
		}
		case DatabaseType::SQLite:
		{
			txn.queries.push_back(sqlite_migrations_create);
			break;
		}
	}
	txn.queries.push_back(sql_migrations_fetchall);

	GetDatabaseConnection()->ExecuteTransaction(txn, KZDatabaseService::CheckMigrations, OnGenericTxnFailure);
}

void KZDatabaseService::CheckMigrations(std::vector<ISQLQuery *> queries)
{
	ISQLResult *result = queries[1]->GetResultSet();

	u32 current = result->GetRowCount();
	u32 max = 0;
	switch (KZDatabaseService::GetDatabaseType())
	{
		case DatabaseType::MySQL:
		{
			max = sizeof(mysqlMigrations) / sizeof(mysqlMigrations[0]);
			break;
		}
		case DatabaseType::SQLite:
		{
			max = sizeof(sqliteMigrations) / sizeof(sqliteMigrations[0]);
			break;
		}
	}
	if (current > max)
	{
		META_CONPRINTF("[KZDB] Fatal error: Number of current migrations are higher than the maximum!\n");
		return;
	}

	for (u32 i = 0; i < current; i++)
	{
		result->FetchRow();
		u32 currentCRC = result->GetInt64(1);
		std::string migrationQuery;

		switch (KZDatabaseService::GetDatabaseType())
		{
			case DatabaseType::MySQL:
			{
				migrationQuery = mysqlMigrations[i];
				break;
			}
			case DatabaseType::SQLite:
			{
				migrationQuery = sqliteMigrations[i];
				break;
			}
		}

		u32 crc = CRC32_ProcessSingleBuffer(migrationQuery.c_str(), migrationQuery.length());
		META_CONPRINTF("crc = %lu, currentCRC = %lu\n", crc, currentCRC);
		if (currentCRC != crc)
		{
			META_CONPRINTF("[KZDB] Fatal error: Migration query %s with CRC %lu does not match the database's %lu!\n", migrationQuery.c_str(), crc,
						   currentCRC);
			META_CONPRINT("[KZDB] Database migration failed. LocalDB will not be available.");
			databaseConnection->Destroy();
			databaseConnection = nullptr;
			return;
		}
	}

	auto onSuccess = []()
	{
		META_CONPRINT("[KZDB] Database migration successful.\n");
		localDBConnected = true;
		KZDatabaseService::SetupMap();
		CALL_FORWARD(eventListeners, OnDatabaseSetup);
	};

	auto onFailure = []()
	{
		META_CONPRINT("[KZDB] Database migration failed. LocalDB will not be available.\n");
		databaseConnection->Destroy();
		databaseConnection = nullptr;
	};

	// If there's no migration needed, the database is already fully setup.
	if (current == max)
	{
		onSuccess();
		return;
	}

	Transaction txn;
	char query[1024];
	for (u32 i = current; i < max; i++)
	{
		switch (KZDatabaseService::GetDatabaseType())
		{
			case DatabaseType::MySQL:
			{
				txn.queries.push_back(mysqlMigrations[i]);
				V_snprintf(query, sizeof(query), sql_migrations_insert,
						   CRC32_ProcessSingleBuffer(mysqlMigrations[i].c_str(), mysqlMigrations[i].length()));
				break;
			}
			case DatabaseType::SQLite:
			{
				txn.queries.push_back(sqliteMigrations[i]);
				V_snprintf(query, sizeof(query), sql_migrations_insert,
						   CRC32_ProcessSingleBuffer(sqliteMigrations[i].c_str(), sqliteMigrations[i].length()));
				break;
			}
		}
		txn.queries.push_back(query);
	}

	GetDatabaseConnection()->ExecuteTransaction(
		txn, [onSuccess](std::vector<ISQLQuery *> queries) { onSuccess(); }, [onFailure](std::string error, int failIndex) { onFailure(); });
}

bool KZDatabaseService::IsReady()
{
	return localDBConnected;
}
