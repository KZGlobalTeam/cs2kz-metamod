#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "queries/players.h"

using namespace KZ::Database;

void KZDatabaseService::SetupClient(KZPlayer *player)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	if (!player || player->IsFakeClient())
	{
		return;
	}
	// Setup Client Step 1 - Upsert them into Players Table
	char query[1024];

	// Note: The player must have been authenticated and have a valid steamID at this point.
	const char *clientName = player->GetName();
	std::string escapedName = GetDatabaseConnection()->Escape(clientName);
	u64 steamID64 = player->GetClient()->GetClientSteamID()->ConvertToUint64();
	const char *clientIP = player->GetClient()->GetRemoteAddress()->ToString(true);

	Transaction txn;

	switch (GetDatabaseType())
	{
		case DatabaseType::SQLite:
		{
			// UPDATE OR IGNORE
			V_snprintf(query, sizeof(query), sqlite_players_update, escapedName.c_str(), clientIP, steamID64);
			txn.queries.push_back(query);
			// INSERT OR IGNORE
			V_snprintf(query, sizeof(query), sqlite_players_insert, escapedName.c_str(), clientIP, steamID64);
			txn.queries.push_back(query);
			break;
		}
		case DatabaseType::MySQL:
		{
			// INSERT ... ON DUPLICATE KEY ...
			V_snprintf(query, sizeof(query), mysql_players_upsert, escapedName.c_str(), clientIP, steamID64);
			txn.queries.push_back(query);
			break;
		}
	}

	V_snprintf(query, sizeof(query), sql_players_get_cheater, steamID64);
	txn.queries.push_back(query);
	for (u32 i = 0; i < txn.queries.size(); i++)
	{
		auto queryStr = txn.queries[i];
	}
	CPlayerUserId userID = player->GetClient()->GetUserID();

	GetDatabaseConnection()->ExecuteTransaction(
		txn,
		[userID, steamID64](std::vector<ISQLQuery *> queries)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			if (!player || !player->IsAuthenticated())
			{
				return;
			}
			ISQLResult *result = NULL;
			switch (GetDatabaseType())
			{
				case DatabaseType::SQLite:
				{
					result = queries[2]->GetResultSet();
					break;
				}
				case DatabaseType::MySQL:
				{
					result = queries[1]->GetResultSet();
					break;
				}
			}
			if (result)
			{
				bool isCheater = (result->FetchRow() && result->GetInt(0) == 1);
				META_CONPRINTF("SetupClient done, steamID %lld\n", steamID64);
			}
		},
		OnGenericTxnFailure);
}
