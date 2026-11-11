/*
	Turn bind detection.
	Doesn't actually ban, this just tries to put a cooldown on external turn binds.
*/
#include "kz/kz.h"
#include "kz/anticheat/kz_anticheat.h"
#include "sdk/usercmd.h"
#include "utils/utils.h"

#define TURN_MIN_DEGREES 0.01f
// Anything bigger is a snap or a teleport.
#define TURN_MAX_DEGREES 45.0f
#define TURN_MAX_LENGTH  512
// Commands sent before the client received the new angles of a teleport still carry the old ones.
#define TURN_TELEPORT_IGNORE_DURATION 1.0f

// Turns held longer than a strafe are judged on their steadiest window of this many commands.
#define TURN_WINDOW_LENGTH 16
#define TURN_WINDOW_STEP   2
#define TURN_MAX_DRIFT     0.05f
// A resting hand or a drifting sensor turns slower than this, in counts per tick.
#define TURN_MIN_COUNTS 8.0f

// Shortest strafe worth measuring, partial first and last command included.
#define TURN_MIN_STRAFE_LENGTH     6
#define TURN_MIN_STRAFE_COUNTS     20.0f
#define TURN_MAX_STRAFE_RATE_ERROR 0.03f

// Below this, the turn is identical every tick, which mouse input sampled once per frame never is.
#define TURN_EXACT_RATIO                     0.1f
#define TURN_NUM_EXACT_WINDOWS_FOR_DETECTION 2
// A hard flick saturates the mouse report at 127 counts, which also turns by the same amount every tick.
#define TURN_SATURATED_COUNTS 127.0f

#define TURN_RATE_HISTORY 512
// Widths are in log space, so the peak is +-2% and its surroundings +-10% of the rate.
#define TURN_PEAK_WIDTH   0.04f
#define TURN_AROUND_WIDTH 0.2f
// Consistent players can reach 10 with normal turns early in a session.
#define TURN_MIN_PEAK      15
#define TURN_MIN_PEAKINESS 0.75f

// Frame timing makes each command of a turn bind turn a bit more or less than its rate.
#define TURN_EXTERNAL_MIN_RATIO    0.75f
#define TURN_EXTERNAL_MAX_RATIO    1.33f
#define TURN_EXTERNAL_MIN_COMMANDS 2
// Turning the same way again this soon is still the turn bind, eg. after a cooldown snap.
#define TURN_EXTERNAL_RESUME_COMMANDS 8
#define TURN_EXTERNAL_SAME_RATE       0.05f

CConVar<bool> kz_ac_turnbind_debug("kz_ac_turnbind_debug", FCVAR_NONE, "Enable turn bind detector debug messages", false);

struct TurnFit
{
	f32 rate;
	// Spread of the cumulative counts around a constant rate, relative to what frame timing alone can cause.
	f32 bandRatio;
	f32 drift;
	f32 meanFrametime;
};

static_function TurnFit FitTurn(const KZAnticheatService::TurnStep *steps, i32 count)
{
	f64 sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0, cumulative = 0.0, frametimeSum = 0.0;
	f32 maxFrametime = 0.0f;
	for (i32 i = 0; i < count; i++)
	{
		cumulative += steps[i].counts;
		sumX += i;
		sumY += cumulative;
		sumXX += (f64)i * i;
		sumXY += i * cumulative;
		frametimeSum += steps[i].frametime;
		maxFrametime = Max(maxFrametime, steps[i].frametime);
	}
	f64 slope = (count * sumXY - sumX * sumY) / (count * sumXX - sumX * sumX);
	f64 intercept = (sumY - slope * sumX) / count;
	f64 minResidual = DBL_MAX;
	f64 maxResidual = -DBL_MAX;
	f64 firstHalf = 0.0, secondHalf = 0.0;
	i32 half = count / 2;
	cumulative = 0.0;
	for (i32 i = 0; i < count; i++)
	{
		cumulative += steps[i].counts;
		f64 residual = cumulative - (intercept + slope * i);
		minResidual = Min(minResidual, residual);
		maxResidual = Max(maxResidual, residual);
		(i < half ? firstHalf : secondHalf) += steps[i].counts;
	}
	TurnFit fit;
	fit.rate = (f32)fabs(slope);
	fit.meanFrametime = (f32)(frametimeSum / count);
	fit.drift = (f32)(fabs(firstHalf / half - secondHalf / (count - half)) / (cumulative / count));
	// Each command may hold one frame more or less than the next, so a constant rate strays by up to one frame of motion.
	f32 allowance = fit.rate * fit.meanFrametime / ENGINE_FIXED_TICK_INTERVAL + 2.0f;
	// A frame spike moves a single command further than the mean frame accounts for.
	fit.bandRatio = maxFrametime > 3.0f * fit.meanFrametime ? FLT_MAX : (f32)(maxResidual - minResidual) / allowance;
	return fit;
}

void KZAnticheatService::TrackTurn(CUserCmd *cmd)
{
	if (cmd->cmdNum <= this->lastTurnCmdNum)
	{
		return;
	}
	this->externalTurnDirection = 0;
	bool consecutive = cmd->cmdNum == this->lastTurnCmdNum + 1;
	f32 yaw = cmd->base().viewangles().y();
	f32 delta = yaw - this->lastTurnYaw;
	if (delta > 180.0f)
	{
		delta -= 360.0f;
	}
	else if (delta < -180.0f)
	{
		delta += 360.0f;
	}
	this->lastTurnCmdNum = cmd->cmdNum;
	this->lastTurnYaw = yaw;
	if (g_pKZUtils->GetServerGlobals()->curtime < this->ignoreTurnsUntil)
	{
		this->ClearCurrentTurn();
		return;
	}

	// +turnleft and +turnright turn at cl_yawspeed, which the cvar check already pins.
	u64 buttons = cmd->base().buttons_pb().buttonstate1();
	if (!consecutive || (buttons & (IN_TURNLEFT | IN_TURNRIGHT)) || fabsf(delta) < TURN_MIN_DEGREES || fabsf(delta) >= TURN_MAX_DEGREES)
	{
		this->FinishTurn();
		return;
	}
	CPlayerSlot slot = this->player->GetPlayerSlot();
	f32 yawPerCount = (f32)(utils::StringToFloat(interfaces::pEngine->GetClientConVarValue(slot, "sensitivity"))
							* utils::StringToFloat(interfaces::pEngine->GetClientConVarValue(slot, "m_yaw")));
	INetChannelInfo *netchan = interfaces::pEngine->GetPlayerNetInfo(slot);
	if (yawPerCount <= 0.0f || !netchan)
	{
		this->FinishTurn();
		return;
	}
	f32 frametime = 0.0f;
	netchan->GetRemoteFramerate(&frametime, nullptr, nullptr);

	i32 sign = delta > 0.0f ? 1 : -1;
	if (sign != this->currentTurnSign)
	{
		this->FinishTurn();
	}
	this->currentTurnSign = sign;
	f32 counts = fabsf(delta) / yawPerCount;
	this->currentTurn.push_back({counts, frametime});
	this->ClassifyExternalTurn(counts, cmd->cmdNum);
	if (this->currentTurn.size() >= TURN_MAX_LENGTH)
	{
		this->FinishTurn();
	}
}

void KZAnticheatService::ClassifyExternalTurn(f32 counts, i32 cmdNum)
{
	if (this->externalTurnRates.empty())
	{
		return;
	}
	if (!this->currentTurnExternal)
	{
		bool nearRate = false;
		for (f32 rate : this->externalTurnRates)
		{
			nearRate |= counts >= rate * TURN_EXTERNAL_MIN_RATIO && counts <= rate * TURN_EXTERNAL_MAX_RATIO;
		}
		this->numExternalTurnCommands = nearRate ? this->numExternalTurnCommands + 1 : 0;
		bool resumed = this->currentTurnSign == this->lastExternalTurnSign && cmdNum - this->lastExternalTurnCmdNum <= TURN_EXTERNAL_RESUME_COMMANDS;
		this->currentTurnExternal = this->numExternalTurnCommands >= TURN_EXTERNAL_MIN_COMMANDS || (nearRate && resumed);
	}
	if (this->currentTurnExternal)
	{
		this->externalTurnDirection = this->currentTurnSign;
		this->lastExternalTurnSign = this->currentTurnSign;
		this->lastExternalTurnCmdNum = cmdNum;
	}
}

void KZAnticheatService::AddExternalTurnRate(f32 rate, const std::string &reason)
{
	for (f32 known : this->externalTurnRates)
	{
		if (fabsf(logf(rate / known)) < TURN_EXTERNAL_SAME_RATE)
		{
			return;
		}
	}
	this->externalTurnRates.push_back(rate);
	KZ_LOG_INFO(LogChannel::AC, "Turn bind found for player %s (%llu): %s\n", this->player->GetName(), this->player->GetSteamId64(), reason.c_str());
}

void KZAnticheatService::ClearCurrentTurn()
{
	this->currentTurn.clear();
	this->currentTurnExternal = false;
	this->numExternalTurnCommands = 0;
}

void KZAnticheatService::ClearTurnTracking()
{
	this->ClearCurrentTurn();
	this->currentTurnSign = 0;
	this->lastTurnCmdNum = -1;
	this->lastTurnYaw = 0.0f;
	this->ignoreTurnsUntil = 0.0;
	this->turnWindowRates.clear();
	this->strafeTurnRates.clear();
	this->numExactTurnWindows = 0;
	this->externalTurnRates.clear();
	this->lastExternalTurnSign = 0;
	this->lastExternalTurnCmdNum = 0;
	this->externalTurnDirection = 0;
}

void KZAnticheatService::OnTeleport(const QAngle *angles)
{
	if (!angles)
	{
		return;
	}
	QAngle currentAngles;
	this->player->GetAngles(&currentAngles);
	if (*angles == currentAngles)
	{
		return;
	}
	this->ClearCurrentTurn();
	this->ignoreTurnsUntil = g_pKZUtils->GetServerGlobals()->curtime + TURN_TELEPORT_IGNORE_DURATION;
}

void KZAnticheatService::FinishTurn()
{
	i32 count = (i32)this->currentTurn.size();
	const TurnStep *steps = this->currentTurn.data();

	if (count >= TURN_MIN_STRAFE_LENGTH)
	{
		// The first and last command only turn for part of the tick.
		i32 interior = count - 2;
		TurnFit fit = FitTurn(steps + 1, interior);
		f32 rateError = fit.meanFrametime / ENGINE_FIXED_TICK_INTERVAL / interior + 2.0f / (fit.rate * interior);
		if (fit.bandRatio < 1.0f && fit.bandRatio >= TURN_EXACT_RATIO && fit.rate >= TURN_MIN_STRAFE_COUNTS
			&& rateError <= TURN_MAX_STRAFE_RATE_ERROR)
		{
			this->strafeTurnRates.push_back(fit.rate);
			this->CheckTurnRates(this->strafeTurnRates, "strafes");
		}
	}

	if (count >= TURN_WINDOW_LENGTH)
	{
		TurnFit best = FitTurn(steps, TURN_WINDOW_LENGTH);
		for (i32 start = TURN_WINDOW_STEP; start + TURN_WINDOW_LENGTH <= count; start += TURN_WINDOW_STEP)
		{
			TurnFit fit = FitTurn(steps + start, TURN_WINDOW_LENGTH);
			if (fit.bandRatio < best.bandRatio)
			{
				best = fit;
			}
		}
		if (best.rate >= TURN_MIN_COUNTS)
		{
			f32 saturations = best.rate / TURN_SATURATED_COUNTS;
			bool saturated = roundf(saturations) >= 1.0f && fabsf(saturations - roundf(saturations)) < 0.01f;
			if (best.bandRatio < TURN_EXACT_RATIO)
			{
				if (!saturated && ++this->numExactTurnWindows >= TURN_NUM_EXACT_WINDOWS_FOR_DETECTION)
				{
					this->AddExternalTurnRate(best.rate,
											  tfm::format("Turned by exactly %.1f counts every tick %d times", best.rate, this->numExactTurnWindows));
				}
			}
			else if (best.bandRatio < 1.0f && best.drift < TURN_MAX_DRIFT)
			{
				this->turnWindowRates.push_back(best.rate);
				this->CheckTurnRates(this->turnWindowRates, "long turns");
			}
		}
	}
	this->ClearCurrentTurn();
}

void KZAnticheatService::CheckTurnRates(std::deque<f32> &rates, const char *kind)
{
	while (rates.size() > TURN_RATE_HISTORY)
	{
		rates.pop_front();
	}
	if ((i32)rates.size() < TURN_MIN_PEAK)
	{
		return;
	}
	std::vector<f32> &logRates = this->turnRateBuffer;
	logRates.clear();
	for (f32 rate : rates)
	{
		logRates.push_back(logf(rate));
	}
	std::sort(logRates.begin(), logRates.end());

	i32 peak = 0;
	i32 around = 0;
	f32 peakCentre = 0.0f;
	for (auto it = logRates.begin(); it != logRates.end(); ++it)
	{
		i32 count = (i32)(std::upper_bound(it, logRates.end(), *it + TURN_PEAK_WIDTH) - it);
		if (count <= peak)
		{
			continue;
		}
		peak = count;
		peakCentre = *it + TURN_PEAK_WIDTH / 2.0f;
		around = (i32)(std::upper_bound(logRates.begin(), logRates.end(), peakCentre + TURN_AROUND_WIDTH / 2.0f)
					   - std::lower_bound(logRates.begin(), logRates.end(), peakCentre - TURN_AROUND_WIDTH / 2.0f));
	}
	f32 peakRate = expf(peakCentre) * ENGINE_FIXED_TICK_RATE;
	if (kz_ac_turnbind_debug.Get())
	{
		this->player->PrintConsole(false, true, "Turn rates (%s): %d of %d within 2%% of %.0f counts/s, %d within 10%%", kind, peak,
								   (i32)rates.size(), peakRate, around);
	}
	if (peak >= TURN_MIN_PEAK && peak >= TURN_MIN_PEAKINESS * around)
	{
		this->AddExternalTurnRate(expf(peakCentre), tfm::format("%d of %d %s within 2%% of %.0f counts/s, %d within 10%%", peak, (i32)rates.size(),
																kind, peakRate, around));
	}
}
