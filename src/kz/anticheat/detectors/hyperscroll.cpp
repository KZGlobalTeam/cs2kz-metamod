/*
	Detector for clients using hyperscrolls.
	Track the amount of jump attempts per command, and every time the player jumps, log the scroll amount before and after the jump.
	If the player consistently scrolls a large amount immediately around the jump, it's likely they are hyperscrolling.
*/

#include "kz/anticheat/kz_anticheat.h"
#include "sdk/usercmd.h"

#define NUM_COMMANDS_TO_TRACK          20   // How many recent commands to track jump attempts for
#define NUM_TICKS_TO_TRACK             10   // How many ticks around a jump should we track scroll data for
#define NUM_JUMPS_WINDOW_SIZE          20   // How many recent jumps to consider for hyperscroll detection
#define NUM_DETECTIONS_THRESHOLD       17   // How many jumps within the window size must be flagged to consider it a detection (85%)
#define HYPERSCROLL_THRESHOLD_PER_TICK 2.0f // Minimum average scroll amount per tick around jumps to consider as hyperscroll

// void KZAnticheatService::OnProcessUsercmds(PlayerCommand *cmds, int numcmds)
// {
// 	for (i32 i = 0; i < numcmds; i++)
// 	{
// 		if (cmds[i].cmdNum <= this->lastCmdNumReceived)
// 		{
// 			continue;
// 		}
// 		this->lastCmdNumReceived = cmds[i].cmdNum;
// 		RecordNumJumpForCommand(&cmds[i]);
// 	}
// }

void KZAnticheatService::RecordNumJumpForCommand(PlayerCommand *cmd)
{
	// JumpCommand jt;
	// jt.numCmd = cmd->cmdNum;
	// i32 jumpAmount = 0;
	// i32 numSubtickJumpMove = 0;
	// for (i32 i = 0; i < cmd->base().subtick_moves_size(); i++)
	// {
	// 	const CSubtickMoveStep &step = cmd->base().subtick_moves(i);
	// 	if (step.button() == IN_JUMP)
	// 	{
	// 		numSubtickJumpMove++;
	// 	}
	// }
	// if (cmd->buttonstates.IsButtonPressed(IN_JUMP, false) && numSubtickJumpMove == 0)
	// {
	// 	// Consider a new jump if the last command had no jump input
	// 	if (this->recentJumpCommands.size() == 0 || !this->recentJumpCommands.back().heldJump)
	// 	{
	// 		jumpAmount = 1;
	// 	}
	// 	i32 finalState = cmd->buttonstates.GetButtonState(IN_JUMP);
	// 	jt.numJumpAttempts = jumpAmount;
	// 	jt.heldJump = finalState & 1;
	// }
	// else if (numSubtickJumpMove > 0)
	// {
	// 	bool waitingForRelease = false;
	// 	for (i32 i = 0; i < cmd->base().subtick_moves_size(); i++)
	// 	{
	// 		const CSubtickMoveStep &step = cmd->base().subtick_moves(i);
	// 		if (step.button() == IN_JUMP)
	// 		{
	// 			if (step.pressed() && !waitingForRelease)
	// 			{
	// 				jumpAmount++;
	// 				waitingForRelease = true;
	// 			}
	// 			else if (!step.pressed())
	// 			{
	// 				waitingForRelease = false;
	// 			}
	// 		}
	// 	}
	// 	jt.heldJump = waitingForRelease;
	// }
	// else
	// {
	// 	jt.numJumpAttempts = 0;
	// 	jt.heldJump = false;
	// }
	// META_CONPRINTF("Recorded jump tick %d: %d jumps, heldJump=%d\n", jt.numCmd, jt.numJumpAttempts, jt.heldJump);
	// this->recentJumpCommands.push_back(jt);
}
