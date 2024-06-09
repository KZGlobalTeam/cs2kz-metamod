#pragma once
#include "../kz.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include <type_traits>

class ISQLConnection;
class ISQLQuery;

namespace KZ
{
	namespace Database
	{
		enum DatabaseType
		{
			DatabaseType_None = -1,
			DatabaseType_SQLite,
			DatabaseType_MySQL
		};
	} // namespace Database
} // namespace KZ

class KZDatabaseServiceEventListener
{
public:
	virtual void OnDatabaseConnect() {}

	virtual void OnClientSetup(Player *player, u64 steamID64, bool isCheater) {}

	virtual void OnMapSetup() {}

	virtual void OnTimeInserted(Player *player, u64 steamID64, u32 mapID, u32 course, u64 mode, u64 styles, u64 runtimeMS, u64 teleportsUsed) {}

	virtual void OnJumpstatPB(Player *player, JumpType jumpType, u64 mode, f64 distance, u32 block, u32 strafes, f32 sync, f32 pre, f32 max,
							  f32 airTime)
	{
	}

	virtual void OnTimeProcessed(Player *player, u64 steamID64, u32 mapID, u32 course, u64 mode, u64 styles, u64 runtimeMS, u64 teleportsUsed,
								 bool firstTime, f64 pbDiff, u32 rank, u32 maxRank, bool firstTimePro, f64 pbDiffPro, u32 rankPro, u32 maxRankPro)
	{
	}

	virtual void OnNewRecord(Player *player, u64 steamID64, u32 mapID, u32 course, u64 mode, u64 styles, u64 teleportsUsed) {}

	virtual void OnRecordMissed(Player *player, f64 recordTime, u32 course, u64 mode, u64 styles, bool pro) {}

	virtual void OnPBMissed(Player *player, f64 pbTime, u32 course, u64 mode, u64 styles, bool pro) {}
};

class KZDatabaseService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static_global bool RegisterEventListener(KZDatabaseServiceEventListener *eventListener);
	static_global bool UnregisterEventListener(KZDatabaseServiceEventListener *eventListener);

	static_global void Init();
	static_global void Cleanup();
	static_global bool IsReady();
	static_global void RegisterCommands();

private:
	internal KZ::Database::DatabaseType databaseType;
	internal ISQLConnection *databaseConnection;

	internal CUtlVector<KZDatabaseServiceEventListener *> eventListeners;

public:
	static_global KZ::Database::DatabaseType GetDatabaseType()
	{
		return databaseType;
	}

	static_global ISQLConnection *GetDatabaseConnection()
	{
		return databaseConnection;
	}

	static_global void OnGenericTxnSuccess(std::vector<ISQLQuery *> queries)
	{
		ConMsg("[KZDB] Transaction successful.\n");
	}

	static_global void OnGenericTxnFailure(std::string error, int failIndex)
	{
		ConMsg("[KZDB] Transaction failed at %i (%s).\n", failIndex, error.c_str());
	}

	static_global void SetupDatabase();
	static_global void OnDatabaseConnected(bool connect);

	static_global void CreateTables();
	static_global bool IsMapSetUp();
	static_global void SetupMap();
	static_global void SetupCourses();
	static_global void SetupClient(KZPlayer *player);

	bool isCheater;
	bool isSetUp;
};
