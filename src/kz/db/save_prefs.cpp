#include "kz_db.h"
#include "kz/option/kz_option.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

#include "queries/players.h"

void KZDatabaseService::SavePrefs(CUtlString prefs)
{
	if (!KZDatabaseService::IsReady() || !this->IsSetup())
	{
		return;
	}
	u64 steamID64 = this->player->GetSteamId64();
	std::string cleanedPrefs = KZDatabaseService::GetDatabaseConnection()->Escape(prefs);

	Transaction txn;

	CUtlString query;
	query.Format(sql_players_set_prefs, prefs.Get(), steamID64);

	txn.queries.push_back(query.Get());

	KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, OnGenericTxnSuccess, OnGenericTxnFailure);
}
