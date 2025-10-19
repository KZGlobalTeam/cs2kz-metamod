#ifndef KZ_REPLAYPLAYBACK_H
#define KZ_REPLAYPLAYBACK_H
#include "common.h"
#include "sdk/datatypes.h"

class KZPlayer;
class PlayerCommand;
class CMoveData;
struct TickData;
struct SubtickData;
class CBasePlayerWeapon;
struct EconInfo;

namespace KZ::replaysystem::playback
{
	// Core playback functions
	void OnPhysicsSimulate(KZPlayer *player);
	void OnProcessMovement(KZPlayer *player);
	void OnProcessMovementPost(KZPlayer *player);
	void OnFinishMovePre(KZPlayer *player, CMoveData *mv);
	void OnPhysicsSimulatePost(KZPlayer *player);
	void OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command);

	// Weapon management during playback
	void CheckWeapon(KZPlayer &player, PlayerCommand &cmd);
	void InitializeWeapons();

	// Playback state management
	void StartReplay();

	// Navigation support
	void NavigateToTick(u32 targetTick);
	void ApplyTickState(KZPlayer *player, const TickData *tickData);
} // namespace KZ::replaysystem::playback

#endif // KZ_REPLAYPLAYBACK_H
