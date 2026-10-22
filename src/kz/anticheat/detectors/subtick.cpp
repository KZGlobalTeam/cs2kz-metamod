/*
	Detect cheats related to subtick inputs.
*/
#include "kz/anticheat/kz_anticheat.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "sdk/usercmd.h"
#include "kz/telemetry/kz_telemetry.h"

// Tunable thresholds for subtick command checks
#define SUBTICK_INITIAL_IGNORE_TIME        10.0f
#define SUBTICK_INVALID_COMMAND_WINDOW     5.0f
#define SUBTICK_INVALID_COMMAND_THRESHOLD  5
#define SUBTICK_SUSPICIOUS_MOVES_WINDOW    0.5f
#define SUBTICK_SUSPICIOUS_MOVES_THRESHOLD 20
#define SUBTICK_YAW_DELTA_THRESHOLD        2
#define SUBTICK_MOVE_YAW_TOLERANCE         2.0f
#define SUBTICK_SUBTICK_INPUTS_WINDOW      20.0f
#define SUBTICK_SUBTICK_INPUTS_THRESHOLD   30
#define SUBTICK_ZERO_WHEN_RATIO_THRESHOLD  0.9f

// Scrolling into and out of a jump is normal, so about 0.1s at each end of a jump is left out.
#define SUBTICK_SPLIT_EDGE_TICKS    7
#define SUBTICK_SPLIT_HISTORY_TICKS 256
// Scrolling in place gains nothing, so splits only count above this speed.
#define SUBTICK_SPLIT_MIN_SPEED 100.0f
// Drilling keeps up its rate for the whole jump while a scroll lasts a fraction of it, so the rate is taken over the busiest 0.5s.
#define SUBTICK_SPLIT_SUSTAIN_TICKS 32
// Splits per second. It takes five jumps at the drill rate to mark an infraction, and one at the instant rate.
#define SUBTICK_SPLIT_DRILL_RATE    48.0f
#define SUBTICK_SPLIT_DRILL_JUMPS   5
#define SUBTICK_SPLIT_INSTANT_RATE  96.0f
#define SUBTICK_SPLIT_DRILL_HISTORY 10
#define SUBTICK_SPLIT_BUTTON_MASK   (IN_ATTACK | IN_ATTACK2 | IN_JUMP | IN_DUCK)

// The engine's own limits on a subtick move list, from CCSPlayer_MovementServices.
// Not to be confused with MAX_SUBTICK_MOVES, which is how many the replay format stores.
#define ENGINE_MAX_SUBTICK_MOVES 32
#define MAX_SUBTICK_MOVE_IMPULSE 2.0f
// One button per subtick move, and only the ones the engine knows how to apply.
#define SUBTICK_MOVE_BUTTON_MASK (IN_ATTACK | IN_ATTACK2 | IN_JUMP | IN_DUCK | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)

CConVar<bool> kz_ac_subtick_debug("kz_ac_subtick_debug", FCVAR_NONE, "Enable subtick abuse detector debug messages", false);

// Every command should have all button presses/releases accounted for in subtick moves.
// Only cheats that modify buttons without updating subtick moves would fail this.
static_global bool VerifyCommand(const CUserCmd &cmd)
{
	// Expect a press/release this tick
	u64 expectedButtons = cmd.base().buttons_pb().buttonstate2() | cmd.base().buttons_pb().buttonstate3();
	// We only care about certain movement buttons.
	expectedButtons &= (/*IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT |*/ IN_JUMP | IN_DUCK);
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		if (cmd.base().subtick_moves(i).has_button())
		{
			expectedButtons &= ~cmd.base().subtick_moves(i).button();
		}
		// This is commented out for now because `forwardback 1 0 0` in console will produce moves
		// with no subtick presses/releases every time the user alt-tabs out and back in.
		/*
		else
		{
			// analog input?
			if (cmd.base().subtick_moves(i).has_analog_forward_delta() && cmd.base().subtick_moves(i).analog_forward_delta() != 0)
			{
				expectedButtons &= ~(IN_FORWARD | IN_BACK);
			}
			if (cmd.base().subtick_moves(i).has_analog_left_delta() && cmd.base().subtick_moves(i).analog_left_delta() != 0)
			{
				expectedButtons &= ~(IN_MOVELEFT | IN_MOVERIGHT);
			}
		}
		*/
	}
	if (expectedButtons != 0)
	{
		KZ_LOG_DEBUG(LogChannel::AC, "Unaccounted buttons in subtick moves detected in command %d: %s\n", cmd.cmdNum, cmd.DebugString().c_str());
	}
	return expectedButtons == 0;
}

// Yaw alias abuse detection: A legitimate client only produces a handful of angle changes per command, unless they are scrolling to jump.
// If there's more than SUBTICK_YAW_DELTA_THRESHOLD subtick moves carrying an actual yaw change in a single tick, flag it.
static_global bool HasExcessiveSubtickMovesWithAngles(const CUserCmd &cmd)
{
	i32 numYawDeltas = 0;
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = cmd.base().subtick_moves(i);
		// Only count moves that actually turn the player.
		if (!step.has_yaw_delta() || step.yaw_delta() == 0.0f)
		{
			continue;
		}
		numYawDeltas++;
		if (numYawDeltas > SUBTICK_YAW_DELTA_THRESHOLD)
		{
			if (kz_ac_subtick_debug.GetBool())
			{
				KZ_LOG_INFO(LogChannel::AC, "Suspicious subtick moves with angles detected in command %d: %s\n", cmd.cmdNum,
							cmd.DebugString().c_str());
			}
			return true;
		}
	}
	return false;
}

// The client adds at most one move without a button or analog input to a command, to carry the view angles of a single frame.
static_function bool HasMultipleAngleOnlyMoves(const CUserCmd &cmd)
{
	i32 numAngleOnlyMoves = 0;
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = cmd.base().subtick_moves(i);
		if (step.button() == 0 && step.analog_forward_delta() == 0.0f && step.analog_left_delta() == 0.0f)
		{
			numAngleOnlyMoves++;
		}
	}
	return numAngleOnlyMoves > 1;
}

// The client keeps any input that lands at the end of the tick for the next command, where it happens at the start.
static_function bool HasEndOfTickMove(const CUserCmd &cmd)
{
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		if (cmd.base().subtick_moves(i).when() >= 1.0f)
		{
			return true;
		}
	}
	return false;
}

// A subtick move steers with the view angles of the frame it happened in, and the input history holds every frame the client rendered.
// Returns how many degrees the furthest move turned past all of those frames.
static_function f32 GetMoveYawOutsideFrames(const CUserCmd &cmd, f32 previousFrameYaw, bool hasPreviousFrame)
{
	f32 yaw = cmd.base().viewangles().y();
	f32 minYaw = 0.0f;
	f32 maxYaw = 0.0f;
	if (hasPreviousFrame)
	{
		f32 offset = utils::GetAngleDifference(yaw, previousFrameYaw, 180.0f, true);
		minYaw = MIN(minYaw, offset);
		maxYaw = MAX(maxYaw, offset);
	}
	for (i32 i = 0; i < cmd.input_history_size(); i++)
	{
		f32 offset = utils::GetAngleDifference(yaw, cmd.input_history(i).view_angles().y(), 180.0f, true);
		minYaw = MIN(minYaw, offset);
		maxYaw = MAX(maxYaw, offset);
	}
	f32 moveYaw = 0.0f;
	f32 outside = 0.0f;
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		moveYaw += cmd.base().subtick_moves(i).yaw_delta();
		f32 offset = utils::GetAngleDifference(0.0f, moveYaw, 180.0f, true);
		outside = MAX(outside, MAX(minYaw - offset, offset - maxYaw));
	}
	return outside;
}

// The engine validates the whole subtick move list before it applies any of it, and throws all of it away if
// any move fails. A command that fails this moves the player from its final button state for the whole tick,
// the same as a command that carried no subtick moves at all.
KZAnticheatService::SubtickRejection KZAnticheatService::VerifySubtickMoves(const CUserCmd *cmd, f32 forwardAxis, f32 sideAxis)
{
	if (cmd->base().subtick_moves_size() > ENGINE_MAX_SUBTICK_MOVES)
	{
		return SubtickRejection::MoveCount;
	}
	f32 lastWhen = 0.0f;
	for (i32 i = 0; i < cmd->base().subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = cmd->base().subtick_moves(i);
		// The moves have to be ordered and land inside the tick.
		if (step.when() < lastWhen || step.when() > 1.0f)
		{
			return SubtickRejection::When;
		}
		lastWhen = step.when();
		u64 button = step.button();
		if (button)
		{
			// Only one button can be pressed at a time, and it must be a valid button.
			if ((button & (button - 1)) != 0 || (button & SUBTICK_MOVE_BUTTON_MASK) == 0)
			{
				return SubtickRejection::Button;
			}
			f32 delta = step.pressed() ? 1.0f : -1.0f;
			if (button == IN_FORWARD)
			{
				forwardAxis += delta;
			}
			else if (button == IN_BACK)
			{
				forwardAxis -= delta;
			}
			else if (button == IN_MOVELEFT)
			{
				sideAxis += delta;
			}
			else if (button == IN_MOVERIGHT)
			{
				sideAxis -= delta;
			}
		}
		else
		{
			forwardAxis += step.analog_forward_delta();
			sideAxis += step.analog_left_delta();
		}
		// Neither a stick nor a keyboard can push an axis past this.
		if (fabsf(forwardAxis) > MAX_SUBTICK_MOVE_IMPULSE || fabsf(sideAxis) > MAX_SUBTICK_MOVE_IMPULSE)
		{
			return SubtickRejection::ImpulseLimit;
		}
	}
	return SubtickRejection::None;
}

static_global bool PossibleDesubtickedCommand(const CUserCmd &cmd, const Vector &impulses)
{
	// Can't tell if there are no subtick moves.
	if (cmd.base().subtick_moves_size() == 0)
	{
		return false;
	}

	// An oversized analog delta makes the engine discard the whole list, and falls back to the buttonstates alone.
	// This can be achieved by using `forwardback 5 0 0` for instance.
	// See KZAnticheatService::CreateInputEvents for more details.
	if (KZAnticheatService::VerifySubtickMoves(&cmd, impulses.x, impulses.y) == KZAnticheatService::SubtickRejection::ImpulseLimit)
	{
		return true;
	}

	// If there are subtick moves but the timing is at exactly 0, it's suspicious... unless it's controller-issued commands.
	bool hasZeroWhen = true;
	bool hasNonAnalogMove = false;
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = cmd.base().subtick_moves(i);
		if (step.has_analog_forward_delta() || step.has_analog_left_delta())
		{
			continue;
		}
		hasNonAnalogMove = true;
		hasZeroWhen &= (step.when() == 0.0f);
	}
	// If every move was analog, we have no non-analog evidence to call this suspicious.
	return hasNonAnalogMove && hasZeroWhen;
}

// Every subtick move splits the tick and runs air acceleration again with the newest view angles.
// Strafing splits on movement keys, so only presses that don't steer and moves with no input at all are counted.
static_function u32 CountSubtickSplits(const CUserCmd &cmd)
{
	u32 numSplits = 0;
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = cmd.base().subtick_moves(i);
		if (step.button() == 0)
		{
			if (step.analog_forward_delta() == 0.0f && step.analog_left_delta() == 0.0f)
			{
				numSplits++;
			}
		}
		else if (step.pressed() && (step.button() & SUBTICK_SPLIT_BUTTON_MASK))
		{
			numSplits++;
		}
	}
	return numSplits;
}

void KZAnticheatService::RecordSubtickSplits(CUserCmd *cmd)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	// Late commands run without moving the player, and the substitute that ran in place of the last one already carries its moves.
	if (cmd->IsLate())
	{
		return;
	}
	u32 currentTick = g_pKZUtils->GetServerGlobals()->tickcount;
	u32 numSplits = 0;
	// Without subtick view angles every part of the tick moves with the same angles, so splitting it changes nothing.
	if (this->player->GetCvarValueFromModeStyles(MODECVAR_SV_SUBTICK_MOVEMENT_VIEW_ANGLES)->m_bValue
		&& this->player->moveDataPost.m_vecVelocity.Length2D() >= SUBTICK_SPLIT_MIN_SPEED)
	{
		numSplits = CountSubtickSplits(*cmd);
	}
	this->recentSubtickSplits.push_back({currentTick, numSplits});
	while (currentTick - this->recentSubtickSplits.front().serverTick > SUBTICK_SPLIT_HISTORY_TICKS)
	{
		this->recentSubtickSplits.pop_front();
	}
}

void KZAnticheatService::CheckJumpSubtickSplits(Jump *jump)
{
	u32 airTicks = (u32)roundf(jump->airtime * ENGINE_FIXED_TICK_RATE);
	if (airTicks < 2 * SUBTICK_SPLIT_EDGE_TICKS + SUBTICK_SPLIT_SUSTAIN_TICKS)
	{
		return;
	}
	u32 windowStart = jump->serverTick - airTicks + SUBTICK_SPLIT_EDGE_TICKS;
	u32 windowEnd = jump->serverTick - SUBTICK_SPLIT_EDGE_TICKS;
	std::vector<u32> commandSplits;
	for (const SubtickSplitSample &sample : this->recentSubtickSplits)
	{
		if (sample.serverTick >= windowStart && sample.serverTick < windowEnd)
		{
			commandSplits.push_back(sample.numSplits);
		}
	}
	if (commandSplits.size() < SUBTICK_SPLIT_SUSTAIN_TICKS)
	{
		return;
	}
	u32 numSplits = 0;
	u32 maxSplits = 0;
	for (size_t i = 0; i < commandSplits.size(); i++)
	{
		numSplits += commandSplits[i];
		if (i >= SUBTICK_SPLIT_SUSTAIN_TICKS)
		{
			numSplits -= commandSplits[i - SUBTICK_SPLIT_SUSTAIN_TICKS];
		}
		maxSplits = Max(maxSplits, numSplits);
	}
	f32 splitRate = maxSplits * ENGINE_FIXED_TICK_RATE / SUBTICK_SPLIT_SUSTAIN_TICKS;
	f32 drillScore = 0.0f;
	if (splitRate > SUBTICK_SPLIT_DRILL_RATE)
	{
		f32 ratio = Clamp((splitRate - SUBTICK_SPLIT_DRILL_RATE) / (SUBTICK_SPLIT_INSTANT_RATE - SUBTICK_SPLIT_DRILL_RATE), 0.0f, 1.0f);
		drillScore = 1.0f / Lerp(ratio, (f32)SUBTICK_SPLIT_DRILL_JUMPS, 1.0f);
	}
	this->recentJumpDrillScores.push_back(drillScore);
	if (this->recentJumpDrillScores.size() > SUBTICK_SPLIT_DRILL_HISTORY)
	{
		this->recentJumpDrillScores.pop_front();
	}
	f32 totalDrillScore = 0.0f;
	u32 numDrilledJumps = 0;
	for (f32 score : this->recentJumpDrillScores)
	{
		totalDrillScore += score;
		numDrilledJumps += score > 0.0f;
	}
	// Five scores of 0.2 can add up to slightly less than 1.
	if (totalDrillScore >= 0.999f)
	{
		this->MarkInfraction(Infraction::Type::SubtickSpam,
							 tfm::format("%.1f subtick splits per second in the air for 0.5s, %d of the last %zu jumps over %.0f", splitRate,
										 numDrilledJumps, this->recentJumpDrillScores.size(), SUBTICK_SPLIT_DRILL_RATE));
		this->recentJumpDrillScores.clear();
	}
}

void KZAnticheatService::CheckSubtickAbuse(CUserCmd *cmd)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	// Verify all button presses/releases are accounted for in subtick moves
	if (!VerifyCommand(*cmd))
	{
		this->invalidCommandTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
		KZ_LOG_WARN(LogChannel::AC, "Invalid command detected from player %s(%llu) @%f, current: %zu\n", this->player->GetName(),
					this->player->GetSteamId64(), g_pKZUtils->GetServerGlobals()->curtime, this->invalidCommandTimes.size());
	}
	// Check for excessive subtick moves with angles
	if (HasExcessiveSubtickMovesWithAngles(*cmd))
	{
		this->suspiciousSubtickMoveTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
	// Late commands can get condensed together, so only on-time commands are checked.
	if (!cmd->IsLate() && HasMultipleAngleOnlyMoves(*cmd))
	{
		this->angleOnlyMoveCommandTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
	if (!cmd->IsLate() && HasEndOfTickMove(*cmd))
	{
		this->endOfTickMoveCommandTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
	if (cmd->input_history_size() > 0)
	{
		if (!cmd->IsLate() && GetMoveYawOutsideFrames(*cmd, this->lastFrameYaw, this->hasLastFrameYaw) > SUBTICK_MOVE_YAW_TOLERANCE)
		{
			this->offViewMoveCommandTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
		}
		this->lastFrameYaw = cmd->input_history(cmd->input_history_size() - 1).view_angles().y();
		this->hasLastFrameYaw = true;
	}
	// Check for possible desubticked commands
	if (cmd->base().subtick_moves_size() > 0)
	{
		this->numCommandsWithSubtickInputs.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
	if (PossibleDesubtickedCommand(*cmd, this->player->GetMoveServices()->m_vecLastMovementImpulses))
	{
		this->zeroWhenCommandTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
}

void KZAnticheatService::CheckSuspiciousSubtickCommands()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	if (this->player->telemetryService->GetTimeInServer() < SUBTICK_INITIAL_IGNORE_TIME)
	{
		// Don't check for the first 10 seconds to avoid false positives on connect... just in case.
		return;
	}
	f32 currentTime = g_pKZUtils->GetServerGlobals()->curtime;

	// Clear out invalid commands within configured window
	while (!this->invalidCommandTimes.empty() && currentTime - this->invalidCommandTimes.front() > SUBTICK_INVALID_COMMAND_WINDOW)
	{
		this->invalidCommandTimes.pop_front();
	}
	if (this->invalidCommandTimes.size() >= SUBTICK_INVALID_COMMAND_THRESHOLD)
	{
		this->MarkInfraction(Infraction::Type::InvalidInput, "Excessive invalid commands detected");
		this->invalidCommandTimes.clear();
	}

	while (!this->angleOnlyMoveCommandTimes.empty() && currentTime - this->angleOnlyMoveCommandTimes.front() > SUBTICK_INVALID_COMMAND_WINDOW)
	{
		this->angleOnlyMoveCommandTimes.pop_front();
	}
	if (this->angleOnlyMoveCommandTimes.size() >= SUBTICK_INVALID_COMMAND_THRESHOLD)
	{
		this->MarkInfraction(Infraction::Type::InvalidInput, "Multiple angle-only subtick moves in one command");
		this->angleOnlyMoveCommandTimes.clear();
	}

	while (!this->endOfTickMoveCommandTimes.empty() && currentTime - this->endOfTickMoveCommandTimes.front() > SUBTICK_INVALID_COMMAND_WINDOW)
	{
		this->endOfTickMoveCommandTimes.pop_front();
	}
	if (this->endOfTickMoveCommandTimes.size() >= SUBTICK_INVALID_COMMAND_THRESHOLD)
	{
		this->MarkInfraction(Infraction::Type::InvalidInput, "Subtick moves at the end of the tick");
		this->endOfTickMoveCommandTimes.clear();
	}

	while (!this->offViewMoveCommandTimes.empty() && currentTime - this->offViewMoveCommandTimes.front() > SUBTICK_INVALID_COMMAND_WINDOW)
	{
		this->offViewMoveCommandTimes.pop_front();
	}
	if (this->offViewMoveCommandTimes.size() >= SUBTICK_INVALID_COMMAND_THRESHOLD)
	{
		this->MarkInfraction(Infraction::Type::InvalidInput, "Subtick moves turned away from every rendered frame");
		this->offViewMoveCommandTimes.clear();
	}

	// Clear out old entries beyond configured window
	while (!this->suspiciousSubtickMoveTimes.empty() && currentTime - this->suspiciousSubtickMoveTimes.front() > SUBTICK_SUSPICIOUS_MOVES_WINDOW)
	{
		this->suspiciousSubtickMoveTimes.pop_front();
	}
	// If there are too many within the timeframe, ban
	if (this->suspiciousSubtickMoveTimes.size() >= SUBTICK_SUSPICIOUS_MOVES_THRESHOLD)
	{
		this->MarkInfraction(Infraction::Type::SubtickSpam, "Excessive subtick moves detected");
		this->suspiciousSubtickMoveTimes.clear();
	}

	// Clear out old entries beyond configured window
	while (!this->numCommandsWithSubtickInputs.empty() && currentTime - this->numCommandsWithSubtickInputs.front() > SUBTICK_SUBTICK_INPUTS_WINDOW)
	{
		this->numCommandsWithSubtickInputs.pop_front();
	}
	while (!this->zeroWhenCommandTimes.empty() && currentTime - this->zeroWhenCommandTimes.front() > SUBTICK_SUBTICK_INPUTS_WINDOW)
	{
		this->zeroWhenCommandTimes.pop_front();
	}
	// If the vast majority of commands in the timeframe have subtick inputs with zero 'when', kick.
	if (this->numCommandsWithSubtickInputs.size() >= SUBTICK_SUBTICK_INPUTS_THRESHOLD)
	{
		f32 ratio = (f32)this->zeroWhenCommandTimes.size() / (f32)this->numCommandsWithSubtickInputs.size();
		if (ratio >= SUBTICK_ZERO_WHEN_RATIO_THRESHOLD)
		{
			this->MarkInfraction(Infraction::Type::Desubtick, "Desubticking detected");
			this->zeroWhenCommandTimes.clear();
			this->numCommandsWithSubtickInputs.clear();
		}
	}
}
