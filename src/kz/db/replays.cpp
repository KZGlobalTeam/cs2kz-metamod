#include "kz_db.h"
#include "kz/option/kz_option.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "queries/bans.h"
#include "queries/jumpstats.h"
#include "queries/manualreplays.h"
#include "queries/times.h"

// The UUIDs come from parsed UUID_t values so they only hold hex digits and dashes.
static_function std::string JoinQuoted(const std::vector<std::string> &uuids)
{
	std::string list;
	for (const std::string &uuid : uuids)
	{
		list += list.empty() ? "'" : ",'";
		list += uuid;
		list += "'";
	}
	return list;
}

void KZDatabaseService::FindJumpReplays(const std::vector<std::string> &uuids, TransactionSuccessCallbackFunc onSuccess,
										TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady() || uuids.empty())
	{
		return;
	}
	Transaction txn;
	txn.queries.push_back(tfm::format(sql_jumpstats_finduuids, JoinQuoted(uuids)));
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::FindRunReplays(const std::vector<std::string> &uuids, i32 keep, TransactionSuccessCallbackFunc onSuccess,
									   TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady() || uuids.empty())
	{
		return;
	}
	Transaction txn;
	txn.queries.push_back(tfm::format(sql_times_findkeptruns, keep, keep, JoinQuoted(uuids)));
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::FindManualReplays(const std::vector<std::string> &uuids, TransactionSuccessCallbackFunc onSuccess,
										  TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady() || uuids.empty())
	{
		return;
	}
	Transaction txn;
	txn.queries.push_back(tfm::format(sql_manualreplays_finduuids, JoinQuoted(uuids)));
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::FindCheaterReplays(const std::vector<std::string> &uuids, TransactionSuccessCallbackFunc onSuccess,
										   TransactionFailureCallbackFunc onFailure)
{
	if (!KZDatabaseService::IsReady() || uuids.empty())
	{
		return;
	}
	Transaction txn;
	txn.queries.push_back(tfm::format(sql_bans_findreplayuuids, JoinQuoted(uuids)));
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::SaveManualReplay(const UUID_t &uuid, u64 steamID64)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	Transaction txn;
	txn.queries.push_back(tfm::format(sql_manualreplays_insert, uuid.ToString(), steamID64));
	const i64 keep = MAX(KZOptionService::GetOptionInt("replays/maxManualSaves", 2), 0);
	txn.queries.push_back(tfm::format(sql_manualreplays_prune, steamID64, steamID64, keep));
	GetDatabaseConnection()->ExecuteTransaction(txn, OnGenericTxnSuccess, OnGenericTxnFailure);
}
