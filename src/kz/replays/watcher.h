#pragma once
#include <atomic>
#include <optional>
#include <thread>
#include "kz_replay.h"
#include "utils/utils.h"
#include "utils/uuid.h"
#include "utils/argparse.h"

class KZPlayer;

struct ReplayFilterCriteria
{
	// General filters
	ReplayType type = RP_RUN;

	struct
	{
		std::optional<std::string> nameSubString;
		std::optional<u64> steamID;
	} player, savedBy; // savedBy is only for manual replays.

	// Current map name by default. * means any map.
	std::string mapName;

	i32 offset = 0;
	i32 limit = 10;

	// Used by cheater replays
	std::optional<std::string> reasonSubString;
	// Used by run and jump replays.
	// Current mode by default.
	std::string modeNameSubString;

	// Run-specific filters
	f32 maxTime = -1.0f;
	i32 maxTeleports = -1;
	i32 numStyles = -1;
	// Try to get current course by default.
	std::optional<std::string> courseName;

	// Jump-specific filters
	u8 jumpType = 0; // LJ by default
	f32 minDistance = 0.0f;

	bool PassGeneralFilters(const ReplayHeader &header, bool exactMapMatch) const;
	bool PassCheaterFilters(const ReplayHeader &header) const;
	bool PassRunFilters(const ReplayHeader &header) const;
	bool PassJumpFilters(const ReplayHeader &header) const;
	bool PassManualFilters(const ReplayHeader &header) const;

	bool PassFilters(const ReplayHeader &header, bool exactMapMatch) const;
};

struct CachedReplayEntry
{
	ReplayHeader header;
	long fileTime = 0;
};

// Keep track of replays on disk and their headers.
class ReplayWatcher
{
	std::mutex replayMapsMutex;
	std::thread watcherThread;
	std::atomic<bool> running;
	std::unordered_map<UUID_t, ReplayHeader> cheaterReplays;
	std::unordered_map<UUID_t, ReplayHeader> manualReplays;
	std::map<UUID_t, ReplayHeader> runReplays;
	std::map<UUID_t, ReplayHeader> jumpReplays;
	std::unordered_map<UUID_t, ReplayHeader> downloadedReplays;
	std::unordered_map<UUID_t, CachedReplayEntry> replayCache;
	std::unordered_map<UUID_t, CachedReplayEntry> downloadedReplayCache;

	std::mutex databaseMutex;
	// Whether the database keeps each replay it has answered for.
	std::unordered_map<UUID_t, bool> databaseStates;
	std::map<ReplayType, std::vector<UUID_t>> databaseChecks;
	std::atomic<bool> recheckRequested;
	// Whether the local database decides which replays are kept.
	bool databaseMode {};

	void WatchLoop();

	void ScanReplays();
	void SweepOrphanedChunks(u64 currentTime);
	void ScanDownloadedReplays(u64 currentTime);
	void QueueDatabaseChecks();

public:
	void Start(bool useDatabase)
	{
		databaseMode = useDatabase;
		running = true;
		watcherThread = std::thread(&ReplayWatcher::WatchLoop, this);
	}

	void Stop()
	{
		if (!running)
		{
			return;
		}
		running = false;
		if (watcherThread.joinable())
		{
			watcherThread.join();
		}
	}

private:
	static inline constexpr const char *generalParameters[] = {"type", "t", "player", "p", "steamid", "sid", "map", "m", "offset", "o", "limit", "l"};
	static inline constexpr const char *cheaterParameters[] = {"reason", "r"};
	static inline constexpr const char *runParameters[] = {"maxtime", "maxteleports", "numstyles", "mode", "course", "c"};
	static inline constexpr const char *jumpParameters[] = {"mode", "mindistance", "mindist", "jumptype", "jt"};
	static inline constexpr const char *manualParameters[] = {"saver", "s", "ssid", "saversteamid"};

public:
	static void PrintUsage(KZPlayer *player);

	void FilterAndPrintMatchingCheaterReplays(ReplayFilterCriteria &criteria, KZPlayer *player);
	void FilterAndPrintMatchingRunReplays(ReplayFilterCriteria &criteria, KZPlayer *player);
	void FilterAndPrintMatchingJumpReplays(ReplayFilterCriteria &criteria, KZPlayer *player);
	void FilterAndPrintMatchingManualReplays(ReplayFilterCriteria &criteria, KZPlayer *player);

	void FindReplaysMatchingCriteria(const char *inputs, KZPlayer *player);

	// Parses !replays arguments and fills in the caller's map, course and mode where they are missing.
	// `type` overrides the type argument if given.
	static ReplayFilterCriteria ParseCriteria(const char *inputs, KZPlayer *player, std::optional<ReplayType> type = std::nullopt);
	// Collects the replays of the criteria's type that pass its filters in console table order.
	std::vector<std::pair<UUID_t, ReplayHeader>> CollectMatches(const ReplayFilterCriteria &criteria);

	// The map of the jump replay with this UUID or an empty string if there is none.
	std::string GetJumpReplayMap(const UUID_t &uuid);
	// The header of a replay on disk or in the download folder.
	std::optional<ReplayHeader> GetReplayHeader(const UUID_t &uuid);

	// Asks the database again about the replays on disk.
	void RequestDatabaseChecks();
	// Hands out up to `count` replays of a type to ask the database about.
	std::vector<UUID_t> TakeDatabaseChecks(ReplayType type, u32 count);
	void SetDatabaseStates(const std::vector<std::pair<UUID_t, bool>> &states);

	// Find replays by UUID substring.
	// Returns matching UUIDs.
	std::vector<UUID_t> FindReplaysByUUIDSubstring(const char *uuidSubstring);
};
