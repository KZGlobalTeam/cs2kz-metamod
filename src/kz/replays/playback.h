#pragma once
#include "common.h"
#include "sdk/datatypes.h"

class KZPlayer;
class PlayerCommand;
class CMoveData;
struct TickData;
struct SubtickData;
class CBasePlayerWeapon;
struct EconInfo;

/*
	Replay playback

	Every replay tick gets fully simulated with the recorded state forced in before and after the movement.
	The timescale decides how many replay ticks one server tick plays.
	Each server tick adds the timescale to tickProgress; the whole part of it is the number of replay ticks to play, for instance:
	- 0.25x: 0, 0, 0, 1, 0, 0, 0, 1, ... (1 replay tick every 4 server ticks)
	- 2.5x: 2, 3, 2, 3, ...

	A server tick that plays no replay tick is called a hold tick. Nothing is simulated on it:
	- During a hold tick, the bot's per-tick logic and command do not run on it,
	so its buttons, timers, sounds and HUD stay as the last played tick left them.
	- FinishMove only moves the bot part of the way	towards the next replay tick via interpolation.

	The bot runs on its own clock (data::GetBotTickcount), which is the server tick the replay tick was recorded on, not the server's.
	ResetBotClock is called when the clock jumps.

	The call flow during each server tick is as follows:
	1. PhysicsSimulate pre hook -> KZPlayer::OnPhysicsSimulate -> BeginServerTick
		- Works out how many replay ticks to play (pendingTicks).
			- None: marks a hold tick and returns false, KZPlayer skips SimulateTick.
			- Otherwise: BeginReplayTick, then KZPlayer::SimulateTick, which ends in OnPhysicsSimulate (pre state of currentTick).

	2. PlayerRunCommand pre hook -> KZPlayer::OnPlayerRunCommand -> OnPlayerRunCommandPre
		- Hold tick: moves the bot with FinishMove (OnFinishMovePre fills in the held state) and returns false, so the hook
			supersedes the command.
		- Otherwise: fills the command with currentTick's input.
			While more than one replay tick is pending, KZPlayer runs the command itself (hooks::CallOriginalPlayerRunCommand),
			then SimulateTickPost, then run the next tick again with BeginReplayTick, SimulateTick and OnPlayerRunCommandPre
			for the next replay tick.
			The last pending tick's command is left to the engine.
		For each command, the engine calls OnProcessMovement, OnProcessMovementPost and OnFinishMovePre, which puts the
		recorded post state into the move data.

	3. PhysicsSimulate post hook -> KZPlayer::OnPhysicsSimulatePost
	   - If it is not a hold tick, KZPlayer::SimulateTickPost.
	   - EndServerTick is always called.
*/
namespace KZ::replaysystem::playback
{
	// Starts the bot's server tick and decides how many replay ticks it plays.
	// Returns false on a hold tick, and true for everyone that is a normal player.
	bool BeginServerTick(KZPlayer *player);
	// Starts one replay tick: advances the bot's clock and puts it in the plugin's server globals.
	void BeginReplayTick(KZPlayer *player);
	// True while more than one replay tick is left this server tick, so KZPlayer has to run a command itself.
	bool HasQueuedReplayTicks(KZPlayer *player);
	bool IsHoldingTick(KZPlayer *player);
	// Puts the server's time back in the plugin's server globals and clears the state of this server tick.
	void EndServerTick(KZPlayer *player);

	// Applies currentTick's recorded pre state, at the end of SimulateTick.
	void OnPhysicsSimulate(KZPlayer *player);
	void OnProcessMovement(KZPlayer *player);
	void OnProcessMovementPost(KZPlayer *player);
	// Overrides the result of the movement with currentTick's post state, or with the held state on a hold tick.
	void OnFinishMovePre(KZPlayer *player, CMoveData *mv);
	// Applies currentTick's post state, runs its events and jumps, and moves on to the next replay tick.
	void OnPhysicsSimulatePost(KZPlayer *player);
	// Fills the command with currentTick's input, or moves the held bot. Returns false when the command must not run.
	bool OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command);

	// Weapon management during playback
	void CheckWeapon(KZPlayer &player, PlayerCommand &cmd);
	void InitializeWeapons();

	// Playback state management
	void StartReplay();
	// Sets the bot's clock, which jumps when a replay starts or is seeked.
	// Clears the times the bot stamped on the old clock, which can be in the future of the new one.
	void ResetBotClock(KZPlayer *player, u32 tick);
	// Pauses the replay and plays one more replay tick on the next server tick.
	void StepTick();

	// Navigation support
	void ApplyTickState(KZPlayer *player, const TickData *tickData);
} // namespace KZ::replaysystem::playback
