#pragma once
#include "sdk/datatypes.h"
#include "utils/uuid.h"

#include <functional>
#include <unordered_map>

class KZPlayer;
class CCSPlayerPawnBase;
class CUserCmd;
class CBasePlayerWeapon;
struct EconInfo;
class CBaseTrigger;
struct KZCourseDescriptor;

namespace KZ::replaysystem
{
	// Receives completed replay writes and renames on the main thread.
	class ReplayEventListener
	{
	public:
		virtual ~ReplayEventListener() = default;

		virtual void OnReplayFileChanged(const char *uuid) {}
	};

	// Registering a listener twice returns false.
	bool RegisterReplayEventListener(ReplayEventListener *listener);
	bool UnregisterReplayEventListener(ReplayEventListener *listener);
	// Called on the main thread once a replay file is written or renamed.
	void NotifyReplayFileChanged(const char *uuid);
	// Finds the replay file of a UUID among the local and downloaded replays.
	bool FindReplayPath(const char *uuid, std::string &path);

	void Init();
	void Cleanup();
	void OnRoundStart();
	void OnGameFrame();
	// A replay bot runs its per-tick logic once per replay tick (zero or several times in a server tick).
	// playback.h explains the order these are called in.
	bool BeginServerTick(KZPlayer *player);
	void BeginReplayTick(KZPlayer *player);
	bool HasQueuedReplayTicks(KZPlayer *player);
	bool IsHoldingTick(KZPlayer *player);
	void EndServerTick(KZPlayer *player);
	void OnPhysicsSimulate(KZPlayer *player);
	void OnProcessMovement(KZPlayer *player);
	void OnProcessMovementPost(KZPlayer *player);
	void OnFinishMovePre(KZPlayer *player, CMoveData *pMoveData);
	void OnPhysicsSimulatePost(KZPlayer *player);
	// Returns false when the command must not run.
	bool OnPlayerRunCommandPre(KZPlayer *player, CUserCmd *command);
	bool IsReplayBot(KZPlayer *player);
	bool CanTouchTrigger(KZPlayer *player, CBaseTrigger *trigger);
	// The playback controls shown to players watching the replay bot.
	void UpdateControls(KZPlayer *player);
	void OnProcessUsercmds(KZPlayer *player, CUserCmd *cmds, i32 numCmds);

	namespace item
	{
		void InitItemAttributes();
		std::string GetItemAttributeName(u16 id);
		std::string GetWeaponName(u16 id);
		gear_slot_t GetWeaponGearSlot(u16 id);
		bool DoesPaintKitUseLegacyModel(float paintKit);
		void ApplyItemAttributesToWeapon(CBasePlayerWeapon &weapon, const EconInfo &info);
		void ApplyModelAttributesToPawn(CCSPlayerPawn *pawn, const EconInfo &info, const char *modelName);
	} // namespace item

	struct RunStatus
	{
		// False for replays that never have a timer.
		bool timed;
		// The course of the last timer start (null before the first one).
		const KZCourseDescriptor *course;
		bool running;
		i32 stages;
		i32 checkpoints;
		i32 splits;
		// The recorded run's time when this is a run replay of that course and 0 otherwise.
		f64 runTime;
		bool runPro;
	};

	RunStatus GetRunStatus();
	// The name of the player the replay was recorded from.
	const char *GetPlayerName();

	i32 GetCurrentCpIndex();
	i32 GetCheckpointCount();
	i32 GetTeleportCount();
	f32 GetTime();
	f32 GetEndTime();
	bool GetPaused();
	const char *GetCourseName();

	void InitWatcher();
	void CleanupWatcher();
	// Asks the database again which replays on disk it still references.
	void RequestDatabaseChecks();
	void ProcessDatabaseChecks();

	void OnAPIUploadFinished(const UUID_t &uuid, bool uploaded);

	struct ReplayAvailability
	{
		bool found {};
		// Empty if the store only said that the replay exists.
		std::string map;
	};

	using AvailabilityCallback = std::function<void(std::unordered_map<UUID_t, ReplayAvailability> &&replays)>;
	// Looks for each replay on disk and then in the store.
	// Each replay comes with the map it was played on if known which fills the {map} of the store's URL.
	// The callback may run before this returns.
	void LocateReplays(const std::vector<std::pair<UUID_t, std::string>> &replays, AvailabilityCallback callback);
	// Finds a replay on disk or downloads it from the store or the API into the download folder.
	// The path is empty if the replay wasn't found.
	void FetchReplay(const UUID_t &uuid, std::function<void(const std::string &path)> callback);
} // namespace KZ::replaysystem
