#include "kz_db.h"
#include "kz/option/kz_option.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

#include "queries/jumpstats.h"

using namespace KZ::Database;

static_function i32 Scale(f64 value, f64 scale)
{
	return (i32)(value * scale + 0.5);
}

void KZDatabaseService::SaveJumpstat(u64 steamID64, i32 jumpType, i32 modeID, f64 distance, f32 block, u32 strafes, f32 sync, f32 pre, f32 max,
									 f32 airtime, const UUID_t &uuid, TransactionSuccessCallbackFunc onSuccess)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	const i32 blockUnits = (i32)(block + 0.5f);
	const i32 keep = (i32)MAX(KZOptionService::GetOptionInt("maxJumpstatsPerCategory", 3), 1);
	Transaction txn;
	char query[2048];
	V_snprintf(query, sizeof(query), sql_jumpstats_getpb, steamID64, jumpType, modeID);
	txn.queries.push_back(query);
	V_snprintf(query, sizeof(query), sql_jumpstats_getblockpb, steamID64, jumpType, modeID);
	txn.queries.push_back(query);
	V_snprintf(query, sizeof(query), sql_jumpstats_insert_uuid, steamID64, jumpType, modeID, Scale(distance, KZ_JUMPSTATS_DISTANCE_SCALE),
			   blockUnits > 0, blockUnits, strafes, Scale(sync, KZ_JUMPSTATS_SYNC_SCALE), Scale(pre, KZ_JUMPSTATS_SPEED_SCALE),
			   Scale(max, KZ_JUMPSTATS_SPEED_SCALE), Scale(airtime, KZ_JUMPSTATS_AIRTIME_SCALE), uuid.ToString().c_str());
	txn.queries.push_back(query);
	V_snprintf(query, sizeof(query), sql_jumpstats_prune, steamID64, jumpType, modeID, steamID64, jumpType, modeID, keep, steamID64, jumpType, modeID,
			   keep);
	txn.queries.push_back(query);
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, OnGenericTxnFailure);
}

void KZDatabaseService::QueryJumpTop(i32 jumpType, i32 modeID, bool block, u32 count, u64 offset, TransactionSuccessCallbackFunc onSuccess,
									 TransactionFailureCallbackFunc onFailure)
{
	const char *filter = block ? " AND j.IsBlockJump=1" : "";
	const char *innerFilter = block ? " AND j2.IsBlockJump=1" : "";
	const char *innerOrder = block ? "j2.Block DESC, j2.Distance DESC, j2.ID" : "j2.Distance DESC, j2.ID";
	const char *outerOrder = block ? "j.Block DESC, j.Distance DESC, j.ID" : "j.Distance DESC, j.ID";
	Transaction txn;
	char query[4096];
	V_snprintf(query, sizeof(query), sql_jumpstats_gettop, jumpType, modeID, filter, innerFilter, innerOrder, outerOrder, count, offset);
	txn.queries.push_back(query);
	V_snprintf(query, sizeof(query), sql_jumpstats_gettopcount, jumpType, modeID, filter);
	txn.queries.push_back(query);
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::FindPlayerJumps(u64 steamID64, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	Transaction txn;
	char query[1024];
	V_snprintf(query, sizeof(query), sql_jumpstats_getplayerjumps, steamID64);
	txn.queries.push_back(query);
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::DeleteJumpstat(u32 id, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	Transaction txn;
	char query[1024];
	V_snprintf(query, sizeof(query), sql_jumpstats_getuuid, id);
	txn.queries.push_back(query);
	V_snprintf(query, sizeof(query), sql_jumpstats_deletejump, (i32)id);
	txn.queries.push_back(query);
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}

void KZDatabaseService::DeleteAllJumpstats(u64 steamID64, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure)
{
	Transaction txn;
	char query[1024];
	V_snprintf(query, sizeof(query), sql_jumpstats_getplayeruuids, steamID64);
	txn.queries.push_back(query);
	V_snprintf(query, sizeof(query), sql_jumpstats_deleteallrecords, steamID64);
	txn.queries.push_back(query);
	GetDatabaseConnection()->ExecuteTransaction(txn, onSuccess, onFailure);
}
