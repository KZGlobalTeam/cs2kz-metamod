#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include "vendor/sql_mm/src/public/sqlite_mm.h"
#include "vendor/sql_mm/src/public/mysql_mm.h"

void KZDBService::CreateTables()
{
	switch (KZDBService::GetDatabaseType())
	{
		case KZDBService::DatabaseType_MySQL:
		{
			break;
		}
		case KZDBService::DatabaseType_SQLite:
		{
			break;
		}
	}
}
