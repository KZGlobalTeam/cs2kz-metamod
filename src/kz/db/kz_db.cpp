#include "kz_db.h"
#include <vendor/sql_mm/src/public/mysql_mm.h>

IMySQLClient *g_pMysqlClient;
IMySQLConnection *g_pConnection;
KZDBService::DatabaseType g_DatabaseType;

void KZDBService::Init()
{
	KZDBService::RegisterCommands();
	KZDBService::SetupDatabase();
}

void KZDBService::SetupDatabase()
{
	MySQLConnectionInfo info {}; // todo: fill this
	g_pMysqlClient = ((ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr))->GetMySQLClient();
	g_pConnection = g_pMysqlClient->CreateMySQLConnection(info);

	g_pConnection->Connect(
		[](bool connect)
		{
			if (connect)
			{
				ConMsg("CONNECTED\n");
			}
			else
			{
				ConMsg("Failed to connect\n");

				// make sure to properly destroy the connection
				g_pConnection->Destroy();
				g_pConnection = nullptr;
			}
		});
}

KZDBService::DatabaseType KZDBService::GetDatabaseType()
{
	return g_DatabaseType;
}
