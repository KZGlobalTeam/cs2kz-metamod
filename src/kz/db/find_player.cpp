#include "kz_db.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "queries/players.h"

#include <algorithm>

void KZDatabaseService::FindPlayerByAlias(const std::string &playerName, TransactionSuccessCallbackFunc onSuccess,
										  TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}

	Transaction txn;
	char query[2048];

	// Get player's steamID through their alias.
	std::string cleanedPlayerName = KZDatabaseService::GetDatabaseConnection()->Escape(playerName.c_str());
	V_snprintf(query, sizeof(query), sql_players_searchbyalias, cleanedPlayerName.c_str(), cleanedPlayerName.c_str());
	txn.queries.push_back(query);

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::FindServerProfile(u64 steamID64, const std::string &playerName, TransactionSuccessCallbackFunc onSuccess,
										  TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}

	Transaction txn;
	char query[2048];
	if (steamID64)
	{
		V_snprintf(query, sizeof(query), sql_players_getprofile, steamID64);
	}
	else
	{
		std::string lowerName = playerName;
		std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return (char)tolower(c); });
		std::string cleanedPlayerName = KZDatabaseService::GetDatabaseConnection()->Escape(lowerName.c_str());
		V_snprintf(query, sizeof(query), sql_players_getprofile_byalias, cleanedPlayerName.c_str(), cleanedPlayerName.c_str());
	}
	txn.queries.push_back(query);

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}
