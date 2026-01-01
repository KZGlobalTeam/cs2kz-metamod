#include "kz/db/kz_db.h"
#include "queries/players.h"
#include "queries/bans.h"
#include "utils/uuid.h"

#include <vendor/sql_mm/src/public/sql_mm.h>

void KZDatabaseService::Ban(const char *reason, f32 durationDays, const UUID_t banId, const UUID_t replayUuid)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	if (!this->player || this->player->IsFakeClient() || !this->player->IsAuthenticated())
	{
		return;
	}

	char query[1024];
	u64 steamID64 = this->player->GetSteamId64();

	Transaction txn;

	// Insert a new ban record
	// Use provided ban ID if available, otherwise generate a new one

	const UUID_t &useId = banId == UUID_t(false) ? banId : UUID_t();
	std::string banIdStr = useId.ToString();
	std::string escapedReason = GetDatabaseConnection()->Escape(reason ? reason : "No reason provided");
	std::string replayUuidStr = replayUuid == UUID_t(false) ? replayUuid.ToString() : "NULL";

	char expiryValue[128];
	if (durationDays > 0.0f)
	{
		// Temporary ban with specific duration in seconds (convert days to seconds)
		V_snprintf(expiryValue, sizeof(expiryValue), "DATE_ADD(CURRENT_TIMESTAMP, INTERVAL %.0f SECOND)", durationDays * 86400.0f);
	}
	else
	{
		// Permanent ban set to far future date
		V_snprintf(expiryValue, sizeof(expiryValue), "'9999-12-31 23:59:59'");
	}
	const char *insertQuery = GetDatabaseType() == KZ::Database::DatabaseType::MySQL ? mysql_bans_insert : sqlite_bans_insert;
	V_snprintf(query, sizeof(query), insertQuery, banIdStr.c_str(), steamID64, escapedReason.c_str(), replayUuidStr.c_str(), expiryValue);
	txn.queries.push_back(query);

	std::string name = this->player->GetName();

	// clang-format off
	GetDatabaseConnection()->ExecuteTransaction(
		txn,
		[name, steamID64](std::vector<ISQLQuery *> queries)
		{
			META_CONPRINTF("[KZ::DB] Player %s (%llu) has been banned.\n", name.c_str(), steamID64);
		},
		OnGenericTxnFailure
	);
	// clang-format on
}

void KZDatabaseService::Ban(const char *reason, const char *endTime, const UUID_t banId, const UUID_t replayUuid)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	if (!this->player || this->player->IsFakeClient() || !this->player->IsAuthenticated())
	{
		return;
	}

	char query[1024];
	u64 steamID64 = this->player->GetSteamId64();

	Transaction txn;

	// Insert a new ban record with specific end time
	const UUID_t &useId = banId == UUID_t(false) ? banId : UUID_t();
	std::string banIdStr = useId.ToString();
	std::string escapedReason = GetDatabaseConnection()->Escape(reason ? reason : "No reason provided");
	std::string replayUuidStr = replayUuid == UUID_t(false) ? replayUuid.ToString() : "NULL";
	std::string escapedEndTime = GetDatabaseConnection()->Escape(endTime);

	const char *insertQuery = GetDatabaseType() == KZ::Database::DatabaseType::MySQL ? mysql_bans_insert : sqlite_bans_insert;
	V_snprintf(query, sizeof(query), insertQuery, banIdStr.c_str(), steamID64, escapedReason.c_str(), replayUuidStr.c_str(), escapedEndTime.c_str());
	txn.queries.push_back(query);

	std::string name = this->player->GetName();

	// clang-format off
	GetDatabaseConnection()->ExecuteTransaction(
		txn,
		[name, steamID64](std::vector<ISQLQuery *> queries)
		{
			META_CONPRINTF("[KZ::DB] Player %s (%llu) has been banned.\n", name.c_str(), steamID64);
		},
		OnGenericTxnFailure
	);
	// clang-format on
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
