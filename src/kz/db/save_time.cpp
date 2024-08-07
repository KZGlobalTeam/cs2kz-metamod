#include "kz_db.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"
#include "queries/save_time.h"
#include "queries/times.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

using namespace KZ::Database;

void KZDatabaseService::SaveTime(u32 id, KZPlayer *player, CUtlString courseName, f64 time, u64 teleportsUsed)
{
	if (!KZDatabaseService::IsReady())
	{
		return;
	}
	char query[1024];
	CPlayerUserId userID = player->GetClient()->GetUserID();
	u64 steamID = player->GetSteamId64();
	u32 modeID = KZ::mode::GetModeInfo(player->modeService).databaseID;
	u64 styleIDs = 0;
	FOR_EACH_VEC(player->styleServices, i)
	{
		i32 styleDatabaseID = KZ::style::GetStyleInfo(player->styleServices[i]).databaseID;
		if (styleDatabaseID < 0)
		{
			styleIDs = -1;
			break;
		}
		styleIDs |= (1ull << styleDatabaseID);
	}
	std::string cleanedCourseName = KZDatabaseService::GetDatabaseConnection()->Escape(courseName);
	Transaction txn;
	V_snprintf(query, sizeof(query), sql_times_insert, steamID, modeID, styleIDs, time, teleportsUsed, GetMapID(), cleanedCourseName.c_str());
	txn.queries.push_back(query);
	if (styleIDs != 0)
	{
		KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(txn, OnGenericTxnSuccess, OnGenericTxnFailure);
	}
	else
	{
		// Get Top 2 PRO PBs
		V_snprintf(query, sizeof(query), sql_getpb, steamID, GetMapID(), cleanedCourseName.c_str(), modeID, styleIDs, 2);
		txn.queries.push_back(query);
		// Get Rank
		V_snprintf(query, sizeof(query), sql_getmaprank, GetMapID(), cleanedCourseName.c_str(), modeID, steamID, GetMapID(),
				   cleanedCourseName.c_str(), modeID);
		txn.queries.push_back(query);
		// Get Number of Players with Times
		V_snprintf(query, sizeof(query), sql_getlowestmaprank, GetMapID(), cleanedCourseName.c_str(), modeID);
		txn.queries.push_back(query);
		if (teleportsUsed == 0)
		{
			// Get Top 2 PRO PBs
			V_snprintf(query, sizeof(query), sql_getpbpro, steamID, GetMapID(), cleanedCourseName.c_str(), modeID, styleIDs, 2);
			txn.queries.push_back(query);
			// Get PRO Rank
			V_snprintf(query, sizeof(query), sql_getmaprankpro, GetMapID(), cleanedCourseName.c_str(), modeID, steamID, GetMapID(),
					   cleanedCourseName.c_str(), modeID);
			txn.queries.push_back(query);
			// Get Number of Players with Times
			V_snprintf(query, sizeof(query), sql_getlowestmaprankpro, GetMapID(), cleanedCourseName.c_str(), modeID);
			txn.queries.push_back(query);
		}
		KZDatabaseService::GetDatabaseConnection()->ExecuteTransaction(
			txn,
			[=](std::vector<ISQLQuery *> queries)
			{
				KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
				KZ::timer::LocalRankData data;
				ISQLResult *result = queries[1]->GetResultSet();
				data.firstTime = result->GetRowCount() == 1;
				if (!data.firstTime)
				{
					result->FetchRow();
					f32 pb = result->GetFloat(0);
					// Close enough. New time is new PB.
					if (fabs(pb - time) < EPSILON)
					{
						result->FetchRow();
						f32 oldPB = result->GetFloat(0);
						data.pbDiff = time - oldPB;
					}
					else // Didn't beat PB
					{
						data.pbDiff = time - pb;
					}
				}
				// Get NUB Rank
				result = queries[2]->GetResultSet();
				result->FetchRow();
				data.rank = result->GetInt(0);
				result = queries[3]->GetResultSet();
				result->FetchRow();
				data.maxRank = result->GetInt(0);

				if (teleportsUsed == 0)
				{
					ISQLResult *result = queries[4]->GetResultSet();
					data.firstTimePro = result->GetRowCount() == 1;
					if (!data.firstTimePro)
					{
						result->FetchRow();
						f32 pb = result->GetFloat(0);
						// Close enough. New time is new PB.
						if (fabs(pb - time) < EPSILON)
						{
							result->FetchRow();
							f32 oldPB = result->GetFloat(0);
							data.pbDiffPro = time - oldPB;
						}
						else // Didn't beat PB
						{
							data.pbDiffPro = time - pb;
						}
					}
					// Get PRO rank
					result = queries[5]->GetResultSet();
					result->FetchRow();
					data.rankPro = result->GetInt(0);
					result = queries[6]->GetResultSet();
					result->FetchRow();
					data.maxRankPro = result->GetInt(0);
				}
				KZ::timer::UpdateLocalRankData(id, data);
			},
			OnGenericTxnFailure);
	}
}
