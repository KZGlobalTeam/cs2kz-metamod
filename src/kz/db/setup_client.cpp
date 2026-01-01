#include "kz_db.h"
#include "kz/option/kz_option.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "queries/players.h"
#include "queries/bans.h"

using namespace KZ::Database;

void KZDatabaseService::SetupClient()
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	if (!this->player || this->player->IsFakeClient())
	{
		return;
	}
	// Setup Client Step 1 - Upsert them into Players Table
	char query[1024];

	// Note: The player must have been authenticated and have a valid steamID at this point.
	const char *clientName = this->player->GetName();
	std::string escapedName = GetDatabaseConnection()->Escape(clientName);
	u64 steamID64 = this->player->GetClient()->GetClientSteamID().ConvertToUint64();
	const char *clientIP = this->player->GetIpAddress();

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

	V_snprintf(query, sizeof(query), sql_players_get_infos, steamID64);
	txn.queries.push_back(query);

	V_snprintf(query, sizeof(query), sql_bans_get_active, steamID64);
	txn.queries.push_back(query);

	CPlayerUserId userID = this->player->GetClient()->GetUserID();

	GetDatabaseConnection()->ExecuteTransaction(
		txn,
		[&, userID, steamID64](std::vector<ISQLQuery *> queries)
		{
			KZPlayer *pl = g_pKZPlayerManager->ToPlayer(userID);
			if (!pl || !pl->IsAuthenticated())
			{
				return;
			}
			ISQLResult *result = NULL;
			ISQLResult *banResult = NULL;
			switch (GetDatabaseType())
			{
				case DatabaseType::SQLite:
				{
					result = queries[2]->GetResultSet();
					banResult = queries[3]->GetResultSet();
					break;
				}
				case DatabaseType::MySQL:
				{
					result = queries[1]->GetResultSet();
					banResult = queries[2]->GetResultSet();
					break;
				}
			}
			if (result)
			{
				bool isBanned = false;
				if (banResult && banResult->FetchRow())
				{
					isBanned = true;
				}
				const char *prefs = result->FetchRow() ? result->GetString(0) : "";
				this->isSetUp = true;
				pl->optionService->InitializeLocalPrefs(prefs);
				CALL_FORWARD(KZDatabaseService::eventListeners, OnClientSetup, pl, pl->GetSteamId64(), isBanned);
			}
		},
		OnGenericTxnFailure);
}
