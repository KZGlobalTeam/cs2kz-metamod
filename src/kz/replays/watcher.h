#include <optional>
#include <thread>
#include "kz_replay.h"
#include "utils/utils.h"
#include "utils/uuid.h"
#include "utils/argparse.h"

class KZPlayer;

// (Legacy comparators removed; ordering handled during filtering when needed.)

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
	// Used by run/jump replays. Current mode by default.
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

	bool PassGeneralFilters(const ReplayHeader &header) const;
	bool PassCheaterFilters(const ReplayHeader &header) const;
	bool PassRunFilters(const ReplayHeader &header) const;
	bool PassJumpFilters(const ReplayHeader &header) const;
	bool PassManualFilters(const ReplayHeader &header) const;

	bool PassFilters(const ReplayHeader &header) const;
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
	// External archival index: uuid -> archived unix timestamp
	std::unordered_map<UUID_t, u64> archivedIndex;
	bool archiveDirty = false;

	void WatchLoop();

	void ScanReplays();

	void MarkArchived(const UUID_t &uuid, u64 archiveTimestamp);
	void LoadArchiveIndex();
	void SaveArchiveIndex();
	void ProcessCheaterReplays(std::unordered_map<UUID_t, ReplayHeader> &cheaterReplays, u64 currentTime);
	void ProcessRunReplays(std::vector<std::tuple<UUID_t, ReplayHeader>> &tempRunReplays, std::map<UUID_t, ReplayHeader> &runReplays,
						   u64 currentTime);
	void ProcessJumpReplays(std::vector<std::tuple<UUID_t, ReplayHeader>> &tempJumpReplays, std::map<UUID_t, ReplayHeader> &jumpReplays,
							u64 currentTime);
	void CleanupManualReplays(std::unordered_map<UUID_t, ReplayHeader> &manualReplays,
							  std::unordered_map<u64, std::vector<std::pair<UUID_t, u64>>> &manualReplaysBySteamID);

public:
	void Start()
	{
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
};
