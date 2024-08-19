#include "kz_db.h"
#include "kz/option/kz_option.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "vendor/sql_mm/src/public/sqlite_mm.h"
#include "vendor/sql_mm/src/public/mysql_mm.h"

using namespace KZ::Database;

void KZDatabaseService::SetupDatabase()
{
	KeyValues *config = KZOptionService::GetOptionKV("db");
	if (!config)
	{
		META_CONPRINT("[KZDB] No database config detected.\n");
		return;
	}
	ISQLInterface *sqlInterface = (ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr);
	if (!sqlInterface)
	{
		META_CONPRINT("[KZDB] Database plugin not found. Local database is disabled.\n");
		return;
	}
	const char *driver = config->GetString("driver");
	if (!V_stricmp(driver, "sqlite"))
	{
		SQLiteConnectionInfo info;
		char path[MAX_PATH];
		V_snprintf(path, sizeof(path), "addons/cs2kz/data/%s.sqlite3", config->GetString("database"));
		info.database = path;
		databaseConnection = sqlInterface->GetSQLiteClient()->CreateSQLiteConnection(info);
		databaseType = DatabaseType::SQLite;
	}
	else if (!V_stricmp(driver, "mysql"))
	{
		MySQLConnectionInfo info = {config->GetString("host"),     config->GetString("user"),    config->GetString("pass"),
									config->GetString("database"), config->GetInt("port", 3306), config->GetInt("timeout", 60)};
		databaseConnection = sqlInterface->GetMySQLClient()->CreateMySQLConnection(info);
		databaseType = DatabaseType::MySQL;
	}
	else
	{
		META_CONPRINT("[KZDB] No database config detected.\n");
	}
	databaseConnection->Connect(OnDatabaseConnected);
}

void KZDatabaseService::OnDatabaseConnected(bool connect)
{
	if (connect)
	{
		META_CONPRINT("[KZDB] LocalDB connected.\n");
		KZDatabaseService::RunMigrations();
	}
	else
	{
		META_CONPRINT("[KZDB] Failed to connect\n");
		// make sure to properly destroy the connection
		databaseConnection->Destroy();
		databaseConnection = nullptr;
	}
	return;
}
