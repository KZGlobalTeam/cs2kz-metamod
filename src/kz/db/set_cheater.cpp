#include "kz/db/kz_db.h"
#include "queries/players.h"
#include "queries/bans.h"
#include "utils/uuid.h"

#include <vendor/sql_mm/src/public/sql_mm.h>

void KZDatabaseService::Ban(u64 steamID64, const char *reason, f32 duration, const UUID_t banId, const UUID_t replayUuid,
							TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}

	if (duration < 0.0f)
	{
		// No ban to be applied
		return;
	}
	char expiryValue[128];
	if (duration > 0.0f)
	{
		// Temporary ban with specific duration in seconds
		V_snprintf(expiryValue, sizeof(expiryValue), "DATE_ADD(CURRENT_TIMESTAMP, INTERVAL %.0f SECOND)", duration);
	}
	else if (duration == 0.0f)
	{
		// Permanent ban set to far future date
		V_snprintf(expiryValue, sizeof(expiryValue), "'9999-12-31 23:59:59'");
	}
	KZDatabaseService::AddOrUpdateBan(steamID64, reason ? reason : "No reason provided", expiryValue, banId, replayUuid, onSuccess, onFailure);
	// clang-format on
}

void KZDatabaseService::AddOrUpdateBan(u64 steamID64, const char *reason, const char *endTime, const UUID_t banId, const UUID_t replayUuid,
									   TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	char query[1024];

	Transaction txn;

	// Set other active bans to expire now
	V_snprintf(query, sizeof(query), sql_bans_remove_active, steamID64);
	txn.queries.push_back(query);

	// Insert or update a new ban record with specific end time
	const UUID_t &useId = banId == UUID_t(false) ? banId : UUID_t();
	std::string banIdStr = useId.ToString();
	std::string escapedReason = GetDatabaseConnection()->Escape(reason ? reason : "No reason provided");
	std::string replayUuidStr = replayUuid == UUID_t(false) ? replayUuid.ToString() : "NULL";

	const char *insertQuery = GetDatabaseType() == KZ::Database::DatabaseType::MySQL ? mysql_bans_insert : sqlite_bans_insert;
	V_snprintf(query, sizeof(query), insertQuery, banIdStr.c_str(), steamID64, escapedReason.c_str(), replayUuidStr.c_str(), endTime);

	txn.queries.push_back(query);

	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::Unban(u64 steamID64)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}

	char query[1024];
	Transaction txn;

	// Set active bans to expire now
	V_snprintf(query, sizeof(query), sql_bans_remove_active, steamID64);
	txn.queries.push_back(query);

	// clang-format off
	GetDatabaseConnection()->ExecuteTransaction(
		txn,
		[steamID64](std::vector<ISQLQuery *> queries)
		{
			META_CONPRINTF("[KZ::DB] Player (%llu) has been unbanned.\n", steamID64);
		},
		OnGenericTxnFailure
	);
	// clang-format on
}
