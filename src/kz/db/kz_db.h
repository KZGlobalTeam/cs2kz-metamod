#pragma once
#include "../kz.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/timer/kz_timer.h"

class ISQLConnection;
class ISQLQuery;

namespace KZ
{
	namespace Database
	{
		enum class DatabaseType
		{
			None = -1,
			SQLite,
			MySQL
		};
	} // namespace Database
} // namespace KZ

class KZDatabaseServiceEventListener
{
public:
	virtual void OnDatabaseConnect() {}

	virtual void OnClientSetup(Player *player, u64 steamID64, bool isCheater) {}

	virtual void OnMapSetup() {}

	virtual void OnTimeInserted(Player *player, u64 steamID64, u32 mapID, u32 course, u64 mode, u64 styles, u64 runtimeMS, u32 teleportsUsed) {}

	virtual void OnJumpstatPB(Player *player, JumpType jumpType, u64 mode, f64 distance, u32 block, u32 strafes, f32 sync, f32 pre, f32 max,
							  f32 airTime)
	{
	}

	virtual void OnTimeProcessed(Player *player, u64 steamID64, u32 mapID, u32 course, u64 mode, u64 styles, u64 runtimeMS, u32 teleportsUsed,
								 bool firstTime, f64 pbDiff, u32 rank, u32 maxRank, bool firstTimePro, f64 pbDiffPro, u32 rankPro, u32 maxRankPro)
	{
	}

	virtual void OnNewRecord(Player *player, u64 steamID64, u32 mapID, u32 course, u64 mode, u64 styles, u32 teleportsUsed) {}

	virtual void OnRecordMissed(Player *player, f64 recordTime, u32 course, u64 mode, u64 styles, bool pro) {}

	virtual void OnPBMissed(Player *player, f64 pbTime, u32 course, u64 mode, u64 styles, bool pro) {}
};

class KZDatabaseService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static bool RegisterEventListener(KZDatabaseServiceEventListener *eventListener);
	static bool UnregisterEventListener(KZDatabaseServiceEventListener *eventListener);

	static void Init();
	static void Cleanup();
	static bool IsReady();

	static void RegisterCommands();
	static void RegisterPBCommand();

private:
	static KZ::Database::DatabaseType databaseType;
	static ISQLConnection *databaseConnection;

	static i32 currentMapID;

private:
	static CUtlVector<KZDatabaseServiceEventListener *> eventListeners;

public:
	static KZ::Database::DatabaseType GetDatabaseType()
	{
		return databaseType;
	}

	static ISQLConnection *GetDatabaseConnection()
	{
		return databaseConnection;
	}

	static void OnGenericTxnSuccess(std::vector<ISQLQuery *> queries)
	{
		ConMsg("[KZDB] Transaction successful.\n");
	}

	static void OnGenericTxnFailure(std::string error, int failIndex)
	{
		ConMsg("[KZDB] Transaction failed at %i (%s).\n", failIndex, error.c_str());
	}

	static void OnGenericQuerySuccess(ISQLQuery *query) {}

	static void SetupDatabase();
	static void OnDatabaseConnected(bool connect);

	static void CreateTables();

	static bool IsMapSetUp();
	static void SetupMap();

	static i32 GetMapID()
	{
		return currentMapID;
	}

	// Course
	static bool AreCoursesSetUp();
	static void SetupCourses(CUtlVector<KZ::timer::CourseInfo> &courseInfos);

	// Client
	static void SetupClient(KZPlayer *player);
	bool isCheater {};
	bool isSetUp {};

	// Mode
	static void UpdateModeIDs();
	static void InsertAndUpdateModeIDs(CUtlString modeName, CUtlString shortName);

	// Styles
	static void UpdateStyleIDs();
	static void InsertAndUpdateStyleIDs(CUtlString styleName, CUtlString shortName);

	// Times
	static void SaveTime(u32 id, KZPlayer *player, CUtlString courseName, f64 time, u64 teleportsUsed);
};
