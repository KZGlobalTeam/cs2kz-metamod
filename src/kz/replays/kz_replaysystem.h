#pragma once
#include "sdk/datatypes.h"

class KZPlayer;
class CCSPlayerPawnBase;
class PlayerCommand;
class CUserCmd;
class CBasePlayerWeapon;
struct EconInfo;
class CBaseTrigger;
struct KZCourseDescriptor;

namespace KZ::replaysystem
{
	void Init();
	void Cleanup();
	void OnRoundStart();
	void OnGameFrame();
	// A replay bot runs its per-tick logic once per replay tick, which can be zero or several times in a server tick.
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
	bool OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command);
	bool IsReplayBot(KZPlayer *player);
	bool CanTouchTrigger(KZPlayer *player, CBaseTrigger *trigger);
	// The playback controls shown to players watching the replay bot.
	void UpdateControls(KZPlayer *player);
	void OnProcessUsercmds(KZPlayer *player, PlayerCommand *cmds, i32 numCmds);

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
		// The course of the last timer start, null before the first one.
		const KZCourseDescriptor *course;
		bool running;
		i32 stages;
		i32 checkpoints;
		i32 splits;
		// The recorded run's time when this is a run replay of that course, 0 otherwise.
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
} // namespace KZ::replaysystem
