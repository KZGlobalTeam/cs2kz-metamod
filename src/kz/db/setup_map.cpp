#include "kz_db.h"

/*
	Inserts the map information into the database.
	Retrieves the MapID of the map and stores it in a global variable.
*/

internal bool mapSetUp = false;

bool KZDatabaseService::IsMapSetUp()
{
	return mapSetUp;
}

void KZDatabaseService::SetupMap()
{
	mapSetUp = false;

	char query[1024];
	META_CONPRINTF("[KZDB] Map name: %s\n", g_pKZUtils->GetServerGlobals()->mapname);
}
