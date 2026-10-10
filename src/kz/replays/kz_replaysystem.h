
#ifndef KZ_REPLAYSYSTEM_H
#define KZ_REPLAYSYSTEM_H
#include "sdk/datatypes.h"

class KZPlayer;
class CCSPlayerPawnBase;
class PlayerCommand;
class CUserCmd;
class CBasePlayerWeapon;
struct EconInfo;
class CBaseTrigger;

namespace KZ::replaysystem
{
	// Receives completed replay writes and renames on the main thread.
	class ReplayEventListener
	{
	public:
		virtual ~ReplayEventListener() = default;

		virtual void OnReplayFileChanged(const char *uuid) {}
	};

	// Register and unregister listeners on the main thread; duplicate registration returns false.
	bool RegisterReplayEventListener(ReplayEventListener *listener);
	bool UnregisterReplayEventListener(ReplayEventListener *listener);
	// Synchronously notify listeners after a replay file changes. Main thread only.
	void NotifyReplayFileChanged(const char *uuid);
	// Find a valid UUID at the existing local/downloaded replay paths. Main thread only.
	bool FindReplayPath(const char *uuid, std::string &path);

	void Init();
	void Cleanup();
	void OnRoundStart();
	void OnGameFrame();
	void OnPhysicsSimulate(KZPlayer *player);
	void OnProcessMovement(KZPlayer *player);
	void OnProcessMovementPost(KZPlayer *player);
	void OnFinishMovePre(KZPlayer *player, CMoveData *pMoveData);
	void OnPhysicsSimulatePost(KZPlayer *player);
	void OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command);
	bool IsReplayBot(KZPlayer *player);
	bool CanTouchTrigger(KZPlayer *player, CBaseTrigger *trigger);

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

#endif // KZ_REPLAYSYSTEM_H
