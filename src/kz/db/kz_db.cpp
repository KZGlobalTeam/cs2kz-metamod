#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"
#include <algorithm>

using namespace KZ::Database;

KZ::Database::DatabaseType KZDatabaseService::databaseType;
ISQLConnection *KZDatabaseService::databaseConnection;

std::vector<KZDatabaseServiceEventListener *> KZDatabaseService::eventListeners;

bool KZDatabaseService::RegisterEventListener(KZDatabaseServiceEventListener *eventListener)
{
	if (std::find(eventListeners.begin(), eventListeners.end(), eventListener) != eventListeners.end())
	{
		return false;
	}
	eventListeners.push_back(eventListener);
	return true;
}

bool KZDatabaseService::UnregisterEventListener(KZDatabaseServiceEventListener *eventListener)
{
	auto it = std::find(eventListeners.begin(), eventListeners.end(), eventListener);
	if (it == eventListeners.end())
	{
		return false;
	}
	eventListeners.erase(it);
	return true;
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
