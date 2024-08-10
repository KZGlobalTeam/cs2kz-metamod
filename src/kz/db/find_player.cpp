#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "queries/players.h"

void KZDatabaseService::FindPlayerByAlias(CUtlString playerName, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}

	Transaction txn;
	char query[1024];

	// Get player's steamID through their alias.
	std::string cleanedPlayerName = KZDatabaseService::GetDatabaseConnection()->Escape(playerName.Get());
	V_snprintf(query, sizeof(query), sql_players_searchbyalias, cleanedPlayerName.c_str(), cleanedPlayerName.c_str());
	txn.queries.push_back(query);

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}
