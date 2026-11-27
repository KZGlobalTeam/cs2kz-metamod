#pragma once
#include "kz/kz.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/timer/kz_timer.h"

class ISQLConnection;
class ISQLQuery;
typedef std::function<void(std::vector<ISQLQuery *>)> TransactionSuccessCallbackFunc;
typedef std::function<void(std::string, int)> TransactionFailureCallbackFunc;

// The Jumpstats table stores its values as integers scaled by these.
#define KZ_JUMPSTATS_DISTANCE_SCALE 10000.0
#define KZ_JUMPSTATS_SYNC_SCALE     100.0
#define KZ_JUMPSTATS_SPEED_SCALE    100.0
#define KZ_JUMPSTATS_AIRTIME_SCALE  10000.0

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
	virtual void OnDatabaseSetup() {}

	// `banReason` is "" when the player isn't banned.
	virtual void OnClientSetup(Player *player, u64 steamID64, bool isBanned, const char *banReason) {}

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

private:
	static KZ::Database::DatabaseType databaseType;
	static ISQLConnection *databaseConnection;

	static i32 currentMapID;

private:
	static std::vector<KZDatabaseServiceEventListener *> eventListeners;

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
		KZ_LOG_INFO(LogChannel::DB, "Transaction successful.\n");
	}

	static void OnGenericTxnFailure(std::string error, int failIndex)
	{
		KZ_LOG_WARN(LogChannel::DB, "Transaction failed at %i (%s).\n", failIndex, error.c_str());
	}

	static void OnGenericQuerySuccess(ISQLQuery *query) {}

	static void SetupDatabase();
	static void OnDatabaseConnected(bool connect);
	static void StartHealthCheck();

	static void RunMigrations();

private:
	static void CheckMigrations(std::vector<ISQLQuery *> queries);
	static void ScheduleMigrationRetry();

public:
	static bool IsMapSetUp();
	static void SetupMap();

	static i32 GetMapID()
	{
		return currentMapID;
	}

	// Course
	static bool AreCoursesSetUp();
	static void SetupCourses(std::vector<KZCourseDescriptor *> &courses);
	static void FindFirstCourseByMapName(const std::string &mapName, TransactionSuccessCallbackFunc onSuccess,
										 TransactionFailureCallbackFunc onFailure);
	// Finds the names of a map's courses known to this server in stage order.
	static void FindCoursesByMapName(const std::string &mapName, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);

	// Client/Player
	void SetupClient();
	void SavePrefs(const std::string &prefs);

private:
	bool isSetUp {};

public:
	bool IsSetUp() const
	{
		return this->isSetUp;
	}

	static void FindPlayerByAlias(const std::string &playerName, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);
	// Finds the SteamID64, alias and first visit of a player by SteamID.
	// The alias that best matches the name is used instead if the SteamID is 0.
	static void FindServerProfile(u64 steamID64, const std::string &playerName, TransactionSuccessCallbackFunc onSuccess,
								  TransactionFailureCallbackFunc onFailure);

	// Mode
	static void UpdateModeIDs();
	static void InsertAndUpdateModeIDs(const std::string &modeName, const std::string &shortName);

	// Styles
	static void UpdateStyleIDs();
	static void InsertAndUpdateStyleIDs(const std::string &styleName, const std::string &shortName);

	// Times
	static void SaveTime(const char *runUUID, u64 steamID, u32 courseID, i32 modeID, f64 time, u64 teleportsUsed, u64 styleIDs,
						 std::string_view metadata, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);
	static void UpdateRunUUID(const char *oldUUID, const char *newUUID, TransactionSuccessCallbackFunc onSuccess,
							  TransactionFailureCallbackFunc onFailure);
	static void QueryAllPBs(u64 steamID64, const std::string &mapName, TransactionSuccessCallbackFunc onSuccess,
							TransactionFailureCallbackFunc onFailure);
	static void QueryPB(u64 steamID64, const std::string &mapName, const std::string &courseName, u32 modeID,
						TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);
	static void QueryPBRankless(u64 steamID64, const std::string &mapName, const std::string &courseName, u32 modeID, u64 styleIDFlags,
								TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);

	static void QueryAllRecords(const std::string &mapName, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);
	// Queries a page of the player's personal bests in a mode with server ranks from newest to oldest.
	// A second query counts the player's personal bests.
	static void QueryPlayerPBs(u64 steamID64, u32 modeID, bool pro, u32 count, u64 offset, TransactionSuccessCallbackFunc onSuccess,
							   TransactionFailureCallbackFunc onFailure);
	// Queries 2 and 3 count the players on the overall and pro leaderboards if withTotals is set.
	static void QueryRecords(const std::string &mapName, const std::string &courseName, u32 modeID, u32 count, u32 offset,
							 TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure, bool withTotals = false);

	// Jumpstats
	// Saves a jump and only keeps the player's best jumps of its type and mode.
	// The first two queries return the previous best distance and the previous best block jump.
	// Sync is a percentage.
	static void SaveJumpstat(u64 steamID64, i32 jumpType, i32 modeID, f64 distance, f32 block, u32 strafes, f32 sync, f32 pre, f32 max, f32 airtime,
							 const UUID_t &uuid, TransactionSuccessCallbackFunc onSuccess);
	// Queries a page of each player's best jump of a type and mode and then counts the players.
	static void QueryJumpTop(i32 jumpType, i32 modeID, bool block, u32 count, u64 offset, TransactionSuccessCallbackFunc onSuccess,
							 TransactionFailureCallbackFunc onFailure);
	static void FindPlayerJumps(u64 steamID64, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);
	// The first query of both returns the UUIDs of the jumps they delete.
	static void DeleteJumpstat(u32 id, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);
	static void DeleteAllJumpstats(u64 steamID64, TransactionSuccessCallbackFunc onSuccess, TransactionFailureCallbackFunc onFailure);

	// Replays
	// Returns the UUIDs among these that still have a jump in the jump top.
	static void FindJumpReplays(const std::vector<std::string> &uuids, TransactionSuccessCallbackFunc onSuccess,
								TransactionFailureCallbackFunc onFailure);
	// Returns the runs among these UUIDs and whether each is among its player's `keep` fastest or fastest without teleports.
	static void FindRunReplays(const std::vector<std::string> &uuids, i32 keep, TransactionSuccessCallbackFunc onSuccess,
							   TransactionFailureCallbackFunc onFailure);
	// Returns the UUIDs among these that are manual saves their player still keeps.
	static void FindManualReplays(const std::vector<std::string> &uuids, TransactionSuccessCallbackFunc onSuccess,
								  TransactionFailureCallbackFunc onFailure);
	// Returns the UUIDs among these that a ban points to.
	static void FindCheaterReplays(const std::vector<std::string> &uuids, TransactionSuccessCallbackFunc onSuccess,
								   TransactionFailureCallbackFunc onFailure);
	// Records a manual save and forgets the player's saves beyond the newest maxManualSaves.
	static void SaveManualReplay(const UUID_t &uuid, u64 steamID64);

	static void Ban(u64 steamID64, const char *reason = nullptr, f32 duration = 0.0f, const UUID_t banId = UUID_t(false),
					const UUID_t replayUuid = UUID_t(false), TransactionSuccessCallbackFunc onSuccess = OnGenericTxnSuccess,
					TransactionFailureCallbackFunc onFailure = OnGenericTxnFailure);
	static void AddOrUpdateBan(u64 steamID64, const char *reason, const char *endTime, const UUID_t banId = UUID_t(false),
							   const UUID_t replayUuid = UUID_t(false), TransactionSuccessCallbackFunc onSuccess = OnGenericTxnSuccess,
							   TransactionFailureCallbackFunc onFailure = OnGenericTxnFailure);
	static void Unban(u64 steamID64);
};
