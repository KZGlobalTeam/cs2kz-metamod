#include "kz_db.h"
#include "kz/option/kz_option.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

#include "queries/players.h"

void KZDatabaseService::SavePrefs(const std::string &prefs)
{
	if (!KZDatabaseService::IsReady() || !this->isSetUp)
	{
		return;
	}
	u64 steamID64 = this->player->GetSteamId64();
	std::string cleanedPrefs = KZDatabaseService::GetDatabaseConnection()->Escape(prefs.c_str());

	Transaction txn;

	std::string query = tfm::format(sql_players_set_prefs, cleanedPrefs.c_str(), steamID64);

	txn.queries.push_back(query.c_str());

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, OnGenericTxnSuccess, OnGenericTxnFailure);
}
