#include "kz_db.h"
#include "kz/option/kz_option.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "vendor/sql_mm/src/public/sqlite_mm.h"
#include "vendor/sql_mm/src/public/mysql_mm.h"
#include "queries/courses.h"
#include "queries/jumpstats.h"
#include "queries/maps.h"
#include "queries/modes.h"
#include "queries/players.h"
#include "queries/styles.h"
#include "queries/startpos.h"
#include "queries/times.h"

ISQLiteClient *g_pSQLiteClient;
IMySQLClient *g_pMySQLClient;
ISQLConnection *g_pConnection;
KZDBService::DatabaseType g_DatabaseType {};

internal void OnDatabaseConnected(bool success);

internal void OnGenericTxnSuccess(std::vector<ISQLQuery *> queries);

internal void OnGenericTxnFailure(std::string error, int failIndex);

void KZDBService::Init()
{
	KZDBService::RegisterCommands();
	KZDBService::SetupDatabase();
}

bool KZDBService::IsReady()
{
	return true;
}

void KZDBService::RegisterCommands() {}

void KZDBService::SetupDatabase()
{
	KeyValues *config = KZOptionService::GetOptionKV("db");
	const char *driver = config->GetString("driver");
	if (!V_stricmp(driver, "sqlite"))
	{
		SQLiteConnectionInfo info;
		info.database = config->GetString("database");
		g_pSQLiteClient = ((ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr))->GetSQLiteClient();
		g_pConnection = g_pSQLiteClient->CreateSQLiteConnection(info);
		g_DatabaseType = KZDBService::DatabaseType::DatabaseType_SQLite;
	}
	else if (!V_stricmp(driver, "mysql"))
	{
		MySQLConnectionInfo info = {config->GetString("host"),     config->GetString("user"),    config->GetString("pass"),
									config->GetString("database"), config->GetInt("port", 3306), config->GetInt("timeout", 60)};
		g_pMySQLClient = ((ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr))->GetMySQLClient();
		g_pConnection = g_pMySQLClient->CreateMySQLConnection(info);
		g_DatabaseType = KZDBService::DatabaseType::DatabaseType_MySQL;
	}
	else
	{
		META_CONPRINT("No database config detected.");
	}
	g_pConnection->Connect(OnDatabaseConnected);
}

KZDBService::DatabaseType KZDBService::GetDatabaseType()
{
	return g_DatabaseType;
}

internal void OnDatabaseConnected(bool success)
{
	if (success)
	{
		ConMsg("CONNECTED\n");
		KZDBService::CreateTables();
	}
	else
	{
		ConMsg("Failed to connect\n");

		// make sure to properly destroy the connection
		g_pConnection->Destroy();
		g_pConnection = nullptr;
	}
}

void KZDBService::SetupMap() {}

void KZDBService::SetupCourses() {}

void KZDBService::CreateTables()
{
	Transaction txn;
	switch (KZDBService::GetDatabaseType())
	{
		case KZDBService::DatabaseType_MySQL:
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
		case KZDBService::DatabaseType_SQLite:
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

	g_pConnection->ExecuteTransaction(txn, OnGenericTxnSuccess, OnGenericTxnFailure);
}

internal void OnGenericTxnSuccess(std::vector<ISQLQuery *> queries)
{
	ConMsg("[KZDB] Transaction successful.\n");
}

internal void OnGenericTxnFailure(std::string error, int failIndex)
{
	ConMsg("[KZDB] Transaction failed at %i (%s).\n", failIndex, error.c_str());
}
