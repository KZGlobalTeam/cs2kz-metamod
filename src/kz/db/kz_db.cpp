#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

using namespace KZ::Database;

KZ::Database::DatabaseType KZDatabaseService::databaseType;
ISQLConnection *KZDatabaseService::databaseConnection;

CUtlVector<KZDatabaseServiceEventListener *> KZDatabaseService::eventListeners;

bool KZDatabaseService::RegisterEventListener(KZDatabaseServiceEventListener *eventListener)
{
	if (eventListeners.Find(eventListener) >= 0)
	{
		return false;
	}
	eventListeners.AddToTail(eventListener);
	return true;
}

bool KZDatabaseService::UnregisterEventListener(KZDatabaseServiceEventListener *eventListener)
{
	return eventListeners.FindAndRemove(eventListener);
}

void KZDatabaseService::Init()
{
	KZDatabaseService::SetupDatabase();
}

void KZDatabaseService::Cleanup()
{
	if (databaseConnection)
	{
		databaseConnection->Destroy();
		databaseConnection = NULL;
	}
}
