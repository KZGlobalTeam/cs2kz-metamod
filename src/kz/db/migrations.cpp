#include "kz_db.h"
#include "kz/option/kz_option.h"
#include "utils/ctimer.h"

#include <regex>
#include "checksum_crc.h"

#include "queries/migrations.h"
#include "queries/courses.h"
#include "queries/jumpstats.h"
#include "queries/maps.h"
#include "queries/modes.h"
#include "queries/players.h"
#include "queries/bans.h"
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
	trimString(mysql_times_alter_id_column),
	trimString(mysql_bans_create),
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
	trimString(sqlite_times_alter_id_column_1),
	trimString(sqlite_times_alter_id_column_2),
	trimString(sqlite_times_alter_id_column_3),
	trimString(sqlite_times_alter_id_column_4),
	trimString(sqlite_bans_create),
};

// clang-format on

static_global constexpr f64 migrationRetryInterval = 5.0;
// With the 10s lock wait, roughly 15 minutes before giving up.
static_global constexpr u32 maxMigrationAttempts = 60;
static_global u32 migrationAttempts;

static_function f64 RetryMigrations()
{
	if (KZDatabaseService::GetDatabaseConnection())
	{
		KZDatabaseService::RunMigrations();
	}
	return 0.0;
}

static_function void ReleaseMigrationLock()
{
	if (KZDatabaseService::GetDatabaseType() != DatabaseType::MySQL)
	{
		return;
	}
	Transaction txn;
	txn.queries.push_back(mysql_migrations_unlock);
	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(
		txn, [](std::vector<ISQLQuery *> queries) {}, KZDatabaseService::OnGenericTxnFailure);
}

void KZDatabaseService::RunMigrations()
{
	Transaction txn;
	switch (KZDatabaseService::GetDatabaseType())
	{
		case DatabaseType::MySQL:
		{
			// Must come first so the migration count below is read while holding the lock.
			txn.queries.push_back(mysql_migrations_lock);
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

	auto onFailure = [](std::string error, int failIndex)
	{
		KZ_LOG_WARN(LogChannel::DB, "Failed to read database migrations at %i (%s).\n", failIndex, error.c_str());
		// The lock may have been acquired before the failing query, and a rollback doesn't release it.
		ReleaseMigrationLock();
		ScheduleMigrationRetry();
	};

	GetDatabaseConnection()->ExecuteTransaction(txn, KZDatabaseService::CheckMigrations, onFailure);
}

void KZDatabaseService::ScheduleMigrationRetry()
{
	if (++migrationAttempts >= maxMigrationAttempts)
	{
		KZ_LOG_WARN(LogChannel::DB, "Database migration failed after %u attempts. LocalDB will not be available.\n", migrationAttempts);
		migrationAttempts = 0;
		databaseConnection->Destroy();
		databaseConnection = nullptr;
		return;
	}
	StartTimer(RetryMigrations, migrationRetryInterval, true, true);
}

void KZDatabaseService::CheckMigrations(std::vector<ISQLQuery *> queries)
{
	if (KZDatabaseService::GetDatabaseType() == DatabaseType::MySQL)
	{
		// GET_LOCK returns 0 on timeout and NULL on error.
		ISQLResult *lockResult = queries[0]->GetResultSet();
		if (!lockResult->FetchRow() || lockResult->IsNull(0) || lockResult->GetInt(0) != 1)
		{
			KZ_LOG_WARN(LogChannel::DB, "Timed out waiting for another server to finish migrating the database, retrying.\n");
			ScheduleMigrationRetry();
			return;
		}
	}

	ISQLResult *result = queries.back()->GetResultSet();

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
		KZ_LOG_WARN(LogChannel::DB, "Fatal error: Number of current migrations are higher than the maximum!\n");
		ReleaseMigrationLock();
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
		KZ_LOG_DEBUG(LogChannel::DB, "crc = %u, currentCRC = %u\n", crc, currentCRC);
		if (currentCRC != crc)
		{
			KZ_LOG_WARN(LogChannel::DB, "Fatal error: Migration query %s with CRC %u does not match the database's %u!\n", migrationQuery.c_str(),
						crc, currentCRC);
			KZ_LOG_WARN(LogChannel::DB, "Database migration failed. LocalDB will not be available.");
			databaseConnection->Destroy();
			databaseConnection = nullptr;
			return;
		}
	}

	// The failure paths destroy the connection, which drops the lock with it.
	auto onSuccess = []()
	{
		KZ_LOG_INFO(LogChannel::DB, "Database migration successful.\n");
		ReleaseMigrationLock();
		migrationAttempts = 0;
		localDBConnected = true;
		KZDatabaseService::SetupMap();
		CALL_FORWARD(eventListeners, OnDatabaseSetup);
	};

	auto onFailure = [](std::string error, int failIndex)
	{
		KZ_LOG_WARN(LogChannel::DB, "Database migration failed at %i (%s). LocalDB will not be available.\n", failIndex, error.c_str());
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
	char query[2048];
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

	GetDatabaseConnection()->ExecuteTransaction(txn, [onSuccess](std::vector<ISQLQuery *> queries) { onSuccess(); }, onFailure);
}

bool KZDatabaseService::IsReady()
{
	return localDBConnected;
}
