/*
	Detect cheats related to subtick inputs.
*/
#include "kz/anticheat/kz_anticheat.h"
#include "sdk/usercmd.h"
#include "kz/telemetry/kz_telemetry.h"

// Every command should have all button presses/releases accounted for in subtick moves.
// Only cheats that modify buttons without updating subtick moves would fail this.
static_global bool VerifyCommand(const PlayerCommand &cmd)
{
	// Expect a press/release this tick
	u64 expectedButtons = cmd.base().buttons_pb().buttonstate2() | cmd.base().buttons_pb().buttonstate3();
	// We only care about certain movement buttons.
	expectedButtons &= (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP | IN_DUCK);
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		if (cmd.base().subtick_moves(i).has_button())
		{
			expectedButtons &= ~cmd.base().subtick_moves(i).button();
		}
		else
		{
			// analog input?
			if (cmd.base().subtick_moves(i).has_analog_forward_delta())
			{
				expectedButtons &= ~(IN_FORWARD | IN_BACK);
			}
			else if (cmd.base().subtick_moves(i).has_analog_left_delta())
			{
				expectedButtons &= ~(IN_MOVELEFT | IN_MOVERIGHT);
			}
		}
	}
	return expectedButtons == 0;
}

// Yaw/pitch alias abuse detection: If there's too many subtick moves with the same button + timing and non-zero angles, flag it.
static_global bool HasExcessiveSubtickMovesWithAngles(const PlayerCommand &cmd)
{
	if (cmd.base().subtick_moves_size() < 2)
	{
		return false;
	}
	std::set<std::pair<u64, f32>> buttonTimes;
	// Get a list of all button presses with timings
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = cmd.base().subtick_moves(i);
		if (!step.pressed() || !step.has_button())
		{
			continue;
		}
		// Ignore attack buttons and turn binds (since they wouldn't work anyway)
		if (step.button() == IN_ATTACK || step.button() == IN_ATTACK2 || step.button() == IN_USE || step.button() == IN_RELOAD
			|| step.button() == IN_TURNLEFT || step.button() == IN_TURNRIGHT)
		{
			continue;
		}
		if (step.button() == IN_JUMP)
		{
			// They will get hit by the hyperscroll detector instead...
			continue;
		}
		if (step.has_pitch_delta() || step.has_yaw_delta())
		{
			buttonTimes.insert({step.button(), step.when()});
		}
	}
	// Go through the list again. If we find the same button + timing again on release, it's suspicious.
	i32 numSuspicious = 0;
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = cmd.base().subtick_moves(i);
		if (step.pressed() || !step.has_button())
		{
			continue;
		}
		auto it = buttonTimes.find({step.button(), step.when()});
		if (it != buttonTimes.end())
		{
			numSuspicious++;
			if (numSuspicious >= 2)
			{
				META_CONPRINTF("Suspicious subtick moves with angles detected in command %d: %s\n", cmd.cmdNum, cmd.DebugString().c_str());
				return true;
			}
		}
	}
	return false;
}

static_global bool PossibleDesubtickedCommand(const PlayerCommand &cmd)
{
	// Can't tell if there are no subtick moves.
	if (cmd.base().subtick_moves_size() == 0)
	{
		return false;
	}

	// If there are subtick moves but the timing is at exactly 0, it's suspicious... unless it's controller-issued commands.
	bool hasZeroWhen = true;
	for (i32 i = 0; i < cmd.base().subtick_moves_size(); i++)
	{
		const CSubtickMoveStep &step = cmd.base().subtick_moves(i);
		if (step.has_analog_forward_delta() || step.has_analog_left_delta())
		{
			continue;
		}
		hasZeroWhen &= (step.when() == 0.0f);
	}
	return hasZeroWhen;
}

void KZAnticheatService::CheckSubtickAbuse(PlayerCommand *cmd)
{
	// Verify all button presses/releases are accounted for in subtick moves
	if (!VerifyCommand(*cmd))
	{
		this->invalidCommandTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
	// Check for excessive subtick moves with angles
	if (HasExcessiveSubtickMovesWithAngles(*cmd))
	{
		this->suspiciousSubtickMoveTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
	// Check for possible desubticked commands
	if (cmd->base().subtick_moves_size() > 0)
	{
		this->numCommandsWithSubtickInputs.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
	if (PossibleDesubtickedCommand(*cmd))
	{
		this->zeroWhenCommandTimes.push_back(g_pKZUtils->GetServerGlobals()->curtime);
	}
}

void KZAnticheatService::CheckSuspiciousSubtickCommands()
{
	if (this->player->telemetryService->GetTimeInServer() < 10.0f)
	{
		// Don't check for the first 10 seconds to avoid false positives on connect... just in case.
		return;
	}
	f32 currentTime = g_pKZUtils->GetServerGlobals()->curtime;

	// Clear out invalid commands within 5 seconds
	while (!this->invalidCommandTimes.empty() && currentTime - this->invalidCommandTimes.front() > 5.0f)
	{
		this->invalidCommandTimes.pop_front();
	}
	if (this->invalidCommandTimes.size() >= 5)
	{
		this->MarkInfraction(Infraction::Type::InvalidInput, "Excessive invalid commands detected");
		this->invalidCommandTimes.clear();
	}

	// Clear out old entries beyond 500ms
	while (!this->suspiciousSubtickMoveTimes.empty() && currentTime - this->suspiciousSubtickMoveTimes.front() > 0.5f)
	{
		this->suspiciousSubtickMoveTimes.pop_front();
	}
	// If there are too many within the timeframe, ban
	if (this->suspiciousSubtickMoveTimes.size() >= 16)
	{
		this->MarkInfraction(Infraction::Type::SubtickSpam, "Excessive subtick moves detected");
		this->suspiciousSubtickMoveTimes.clear();
	}

	// Clear out old entries beyond 20s
	while (!this->numCommandsWithSubtickInputs.empty() && currentTime - this->numCommandsWithSubtickInputs.front() > 20.0f)
	{
		this->numCommandsWithSubtickInputs.pop_front();
	}
	while (!this->zeroWhenCommandTimes.empty() && currentTime - this->zeroWhenCommandTimes.front() > 20.0f)
	{
		this->zeroWhenCommandTimes.pop_front();
	}
	// If the vast majority of commands in the timeframe have subtick inputs with zero 'when', kick.
	if (this->numCommandsWithSubtickInputs.size() >= 30)
	{
		f32 ratio = (f32)this->zeroWhenCommandTimes.size() / (f32)this->numCommandsWithSubtickInputs.size();
		if (ratio >= 0.9f)
		{
			this->MarkInfraction(Infraction::Type::Desubtick, "Desubticking detected");
			this->zeroWhenCommandTimes.clear();
			this->numCommandsWithSubtickInputs.clear();
		}
	}
}
