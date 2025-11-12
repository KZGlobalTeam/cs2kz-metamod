#include <optional>
#include <thread>
#include "kz_replay.h"
#include "utils/utils.h"
#include "utils/uuid.h"
#include "utils/argparse.h"

class KZPlayer;

// Comparator for run replays: mode -> course -> time (ascending)
struct RunReplayComparator
{
	bool operator()(const std::pair<GeneralReplayHeader, RunReplayHeader> &a, const std::pair<GeneralReplayHeader, RunReplayHeader> &b) const
	{
		// Compare map name
		i32 mapCompare = V_stricmp(a.first.map.name, b.first.map.name);
		if (mapCompare != 0)
		{
			return mapCompare < 0;
		}
		// Compare mode name
		i32 modeCompare = V_stricmp(a.second.mode.name, b.second.mode.name);
		if (modeCompare != 0)
		{
			return modeCompare < 0;
		}

		// Compare course name
		i32 courseCompare = V_stricmp(a.second.courseName, b.second.courseName);
		if (courseCompare != 0)
		{
			return courseCompare < 0;
		}

		// Compare time (faster times first)
		return a.second.time < b.second.time;
	}
};

// Comparator for jump replays: type -> distance (descending) -> block (descending)
struct JumpReplayComparator
{
	bool operator()(const std::pair<GeneralReplayHeader, JumpReplayHeader> &a, const std::pair<GeneralReplayHeader, JumpReplayHeader> &b) const
	{
		// Compare jump type
		if (a.second.jumpType != b.second.jumpType)
		{
			return a.second.jumpType < b.second.jumpType;
		}

		// Compare distance (larger distances first)
		if (a.second.distance != b.second.distance)
		{
			return a.second.distance > b.second.distance;
		}

		// Compare block distance (larger blocks first)
		return a.second.blockDistance > b.second.blockDistance;
	}
};

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

	bool PassGeneralFilters(const GeneralReplayHeader &header) const;
	bool PassCheaterFilters(const CheaterReplayHeader &header) const;
	bool PassRunFilters(const RunReplayHeader &header) const;
	bool PassJumpFilters(const JumpReplayHeader &header) const;
	bool PassManualFilters(const ManualReplayHeader &header) const;

	template<typename T>
	bool PassFilters(const GeneralReplayHeader &header, const T &specificHeader) const
	{
		if (!PassGeneralFilters(header))
		{
			return false;
		}
		if constexpr (std::is_same_v<T, CheaterReplayHeader>)
		{
			return PassCheaterFilters(specificHeader);
		}
		else if constexpr (std::is_same_v<T, RunReplayHeader>)
		{
			return PassRunFilters(specificHeader);
		}
		else if constexpr (std::is_same_v<T, JumpReplayHeader>)
		{
			return PassJumpFilters(specificHeader);
		}
		else if constexpr (std::is_same_v<T, ManualReplayHeader>)
		{
			return PassManualFilters(specificHeader);
		}
		else
		{
			static_assert(std::is_same_v<T, void>, "Unsupported header type for PassFilters");
			return false;
		}
	}
};

// Keep track of replays on disk and their headers.
class ReplayWatcher
{
	std::mutex replayMapsMutex;
	std::thread watcherThread;
	std::atomic<bool> running;
	std::unordered_map<UUID_t, std::pair<GeneralReplayHeader, CheaterReplayHeader>> cheaterReplays;
	std::unordered_map<UUID_t, std::pair<GeneralReplayHeader, ManualReplayHeader>> manualReplays;

	// Sorted containers for run and jump replays
	std::map<UUID_t, std::pair<GeneralReplayHeader, RunReplayHeader>> runReplays;
	std::map<UUID_t, std::pair<GeneralReplayHeader, JumpReplayHeader>> jumpReplays;

	void WatchLoop();

	void ScanReplays();

	void UpdateArchivedReplayOnDisk(const UUID_t &uuid, GeneralReplayHeader &header, u64 archiveTimestamp);
	void ProcessCheaterReplays(std::unordered_map<UUID_t, std::pair<GeneralReplayHeader, CheaterReplayHeader>> &cheaterReplays, u64 currentTime);
	void ProcessRunReplays(std::vector<std::tuple<UUID_t, GeneralReplayHeader, RunReplayHeader>> &tempRunReplays,
						   std::map<UUID_t, std::pair<GeneralReplayHeader, RunReplayHeader>> &runReplays, u64 currentTime);
	void ProcessJumpReplays(std::vector<std::tuple<UUID_t, GeneralReplayHeader, JumpReplayHeader>> &tempJumpReplays,
							std::map<UUID_t, std::pair<GeneralReplayHeader, JumpReplayHeader>> &jumpReplays, u64 currentTime);
	void CleanupManualReplays(std::unordered_map<UUID_t, std::pair<GeneralReplayHeader, ManualReplayHeader>> &manualReplays,
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
