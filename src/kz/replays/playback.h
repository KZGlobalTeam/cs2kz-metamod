#pragma once
#include "common.h"
#include "sdk/datatypes.h"

class KZPlayer;
class CUserCmd;
class CMoveData;
struct TickData;
struct SubtickData;
class CBasePlayerWeapon;
struct EconInfo;

/*
	Replay playback

	Every replay tick gets fully simulated with the recorded state forced in before and after the movement.
	The timescale decides how many replay ticks one server tick plays.
	Each server tick adds the timescale to tickProgress.
	The whole part of it is the number of replay ticks to play as in these examples:
	- 0.25x: 0, 0, 0, 1, 0, 0, 0, 1, ... (1 replay tick every 4 server ticks)
	- 2.5x: 2, 3, 2, 3, ...

	A server tick that plays no replay tick is called a hold tick.
	Nothing is simulated on it:
	- The bot's per-tick logic and command do not run during a hold tick.
	Its buttons, timers, sounds and HUD stay as the last played tick left them.
	- FinishMove only moves the bot part of the way	towards the next replay tick via interpolation.

	The bot runs on its own clock (data::GetBotTickcount).
	That is the server tick the replay tick was recorded on rather than the server's.
	ResetBotClock is called when the clock jumps.

	The call flow during each server tick is as follows:
	1. PhysicsSimulate pre hook -> KZPlayer::OnPhysicsSimulate -> BeginServerTick
		- Works out how many replay ticks to play (pendingTicks).
			- None: marks a hold tick and returns false so KZPlayer skips SimulateTick.
			- Otherwise: BeginReplayTick and then KZPlayer::SimulateTick that ends in OnPhysicsSimulate (pre state of currentTick).

	2. PlayerRunCommand pre hook -> KZPlayer::OnPlayerRunCommand -> OnPlayerRunCommandPre
		- Hold tick: moves the bot with FinishMove (OnFinishMovePre fills in the held state) and returns false so the hook supersedes the command.
		- Otherwise: fills the command with currentTick's input.
			KZPlayer runs the command itself (hooks::CallOriginalPlayerRunCommand) while more than one replay tick is pending.
			It then runs SimulateTickPost and the next replay tick again with BeginReplayTick, SimulateTick and OnPlayerRunCommandPre.
			The last pending tick's command is left to the engine.
		The engine calls OnProcessMovement, OnProcessMovementPost and OnFinishMovePre for each command.
		OnFinishMovePre puts the recorded post state into the move data.

	3. PhysicsSimulate post hook -> KZPlayer::OnPhysicsSimulatePost
	   - KZPlayer::SimulateTickPost if it is not a hold tick.
	   - EndServerTick is always called.
*/
namespace KZ::replaysystem::playback
{
	// Starts the bot's server tick and decides how many replay ticks it plays.
	// Returns false on a hold tick and true for everyone that is a normal player.
	bool BeginServerTick(KZPlayer *player);
	// Starts one replay tick by advancing the bot's clock and putting it in the plugin's server globals.
	void BeginReplayTick(KZPlayer *player);
	// True while more than one replay tick is left this server tick and KZPlayer has to run a command itself.
	bool HasQueuedReplayTicks(KZPlayer *player);
	bool IsHoldingTick(KZPlayer *player);
	// Puts the server's time back in the plugin's server globals and clears the state of this server tick.
	void EndServerTick(KZPlayer *player);

	// Applies currentTick's recorded pre state at the end of SimulateTick.
	void OnPhysicsSimulate(KZPlayer *player);
	void OnProcessMovement(KZPlayer *player);
	void OnProcessMovementPost(KZPlayer *player);
	// Overrides the result of the movement with currentTick's post state or with the held state on a hold tick.
	void OnFinishMovePre(KZPlayer *player, CMoveData *mv);
	// Applies currentTick's post state, runs its events and jumps, and moves on to the next replay tick.
	void OnPhysicsSimulatePost(KZPlayer *player);
	// Fills the command with currentTick's input or moves the held bot.
	// Returns false if the command must not run.
	bool OnPlayerRunCommandPre(KZPlayer *player, CUserCmd *command);

	// Weapon management during playback
	void CheckWeapon(KZPlayer &player, CUserCmd &cmd);
	void InitializeWeapons();

	// Playback state management
	void StartReplay();
	// Sets the bot's clock (it jumps when a replay starts or is seeked).
	// Clears the times the bot stamped on the old clock because they can be in the future of the new one.
	void ResetBotClock(KZPlayer *player, u32 tick);
	// Pauses the replay and plays one more replay tick on the next server tick.
	void StepTick();

	// Navigation support
	void ApplyTickState(KZPlayer *player, const TickData *tickData);
} // namespace KZ::replaysystem::playback
