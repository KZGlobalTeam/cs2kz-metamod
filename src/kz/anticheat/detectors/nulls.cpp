/*
	Track subtick press and release timings using subtick move data.
*/
#include "kz/anticheat/kz_anticheat.h"
#include "sdk/usercmd.h"
#include "utils/simplecmds.h"

#define NUM_MIN_INPUT_EVENTS_FOR_DETECTION                    128
#define NUM_CONSECUTIVE_PERFECT_CSTRAFE_FOR_DETECTION_MINIMUM 128
#define NUM_CONSECUTIVE_PERFECT_CSTRAFE_FOR_DETECTION_MAXIMUM 640

// The higher the FPS, the less likely player can get perfect counter-strafes by chance.
#define FPS_FOR_MINIMUM_SUSPICION 64.0f // We shouldn't count any attempt below this FPS.
#define FPS_FOR_MAXIMUM_SUSPICION 256.0f
// A key bound to an analog command is not a controller, and there is no legitimate reason to fake a stick.
#define SYNTHETIC_ANALOG_CSTRAFE_WEIGHT 4.0f
// Analog deltas landing exactly on a hundredth. A real stick almost never does.
#define SYNTHETIC_ANALOG_GRID         0.01f
#define SYNTHETIC_ANALOG_GRID_EPSILON 1e-6f
#define MIN_AIR_SPEED_FOR_DETECTION   100.0f // Only consider airstrafes with at least this airspeed to avoid false positives.
// Only count counterstrafe attempts if the keypresses are at most this far apart, in either direction.
// Consider higher values as brand new inputs rather than a counter-strafe attempt.
#define GAP_DISCARD_THRESHOLD 0.2f
// Above this the player already loses enough of the jump that nulling would not meaningfully help them.
#define SWITCH_LOSS_FORGIVENESS_THRESHOLD 0.02f // ~10% of a flat ground jump, considering 7.5 strafes on average

// An exact comparison against 0 misses any null that carries a fraction of a subtick of jitter.
// Treat a small gap in either direction as a perfect swap.
#define NEAR_PERFECT_FRAMETIME_SCALE 0.5f
#define NEAR_PERFECT_MIN_DURATION    (ENGINE_FIXED_TICK_INTERVAL / 64.0f) // ~0.25ms
#define NEAR_PERFECT_MAX_DURATION    0.001f                               // 1ms

CConVar<bool> kz_ac_nulls_debug("kz_ac_nulls_debug", FCVAR_NONE, "Enable nulls detector debug messages", false);

void KZAnticheatService::CreateInputEvents(PlayerCommand *cmd)
{
	// Ignore bots.
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	INetChannelInfo *netchan = interfaces::pEngine->GetPlayerNetInfo(this->player->GetPlayerSlot());
	if (!netchan)
	{
		return;
	}
	Vector &lastMovementImpulses = this->player->GetMoveServices()->m_vecLastMovementImpulses;
	f32 forwardAxis = lastMovementImpulses.x;
	f32 sideAxis = lastMovementImpulses.y;

	auto heldButton = [](f32 axis, u64 positive, u64 negative) -> u64
	{
		if (axis > 0.0f)
		{
			return positive;
		}
		return axis < 0.0f ? negative : 0;
	};

	bool airborne = (this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND) == 0 && this->player->GetMoveType() == MOVETYPE_WALK;
	// This isn't the actual airspeed at the time of the input, but it's close enough for our purposes.
	f32 airSpeed = airborne ? this->player->moveDataPost.m_vecVelocity.Length2D() : -1.0f;

	auto push = [&](u64 button, bool pressed, f32 when, bool analog, bool synthetic)
	{
		InputEvent event {cmd->cmdNum, when, -1.0f, button, pressed, analog, airSpeed, synthetic};
		netchan->GetRemoteFramerate(&event.framerate, nullptr, nullptr);
		if (button == IN_FORWARD || button == IN_BACK)
		{
			this->recentForwardBackwardEvents.push_back(event);
		}
		else // IN_MOVELEFT || IN_MOVERIGHT
		{
			this->recentLeftRightEvents.push_back(event);
		}
	};

	// Move one axis to the given set of held buttons, emitting only what actually changes. Both directions of
	// an axis can be held at once - that is exactly what an overlap is - so this cannot be derived from the
	// movement impulses, which cancel to zero while both are down.
	u64 &heldButtons = this->heldMovementButtons;
	auto setHeld = [&](u64 positive, u64 negative, u64 target, f32 when, bool analog, bool synthetic = false)
	{
		u64 axisMask = positive | negative;
		u64 held = heldButtons & axisMask;
		if (held == target)
		{
			return;
		}
		// Release before press, so a swap with no gap reads as a perfect counter-strafe and not as an overlap.
		if ((held & ~target) & positive)
		{
			push(positive, false, when, analog, synthetic);
		}
		if ((held & ~target) & negative)
		{
			push(negative, false, when, analog, synthetic);
		}
		if ((target & ~held) & positive)
		{
			push(positive, true, when, analog, synthetic);
		}
		if ((target & ~held) & negative)
		{
			push(negative, true, when, analog, synthetic);
		}
		heldButtons = (heldButtons & ~axisMask) | target;
	};

	if (cmd->base().subtick_moves_size() == 0 || VerifySubtickMoves(cmd, forwardAxis, sideAxis) != SubtickRejection::None)
	{
		// A command is allowed to change the movement buttons without carrying any subtick move.
		// In such scenario, SetupMove derives the impulses from the final button state
		// and treat it as an input that happened at the very start of the tick.
		u64 buttons = cmd->base().buttons_pb().buttonstate1();
		setHeld(IN_FORWARD, IN_BACK, buttons & (IN_FORWARD | IN_BACK), 0.0f, false);
		setHeld(IN_MOVELEFT, IN_MOVERIGHT, buttons & (IN_MOVELEFT | IN_MOVERIGHT), 0.0f, false);
		return;
	}

	for (i32 i = 0; i < cmd->base().subtick_moves_size(); ++i)
	{
		const CSubtickMoveStep &step = cmd->base().subtick_moves(i);

		if (step.has_button() && step.button())
		{
			u64 button = step.button();
			u64 positive = 0;
			u64 negative = 0;
			f32 delta = step.pressed() ? 1.0f : -1.0f;
			if (button == IN_FORWARD || button == IN_BACK)
			{
				positive = IN_FORWARD;
				negative = IN_BACK;
				forwardAxis += (button == IN_FORWARD) ? delta : -delta;
			}
			else if (button == IN_MOVELEFT || button == IN_MOVERIGHT)
			{
				positive = IN_MOVELEFT;
				negative = IN_MOVERIGHT;
				sideAxis += (button == IN_MOVELEFT) ? delta : -delta;
			}
			else
			{
				// Buttons that are not direction inputs are ignored.
				continue;
			}
			// A key press says nothing about the opposite key, which may well stay held. Take the move at face
			// value instead of reading the direction back off the axis, where holding both cancels out.
			u64 held = heldButtons & (positive | negative);
			setHeld(positive, negative, step.pressed() ? (held | button) : (held & ~button), step.when(), false);
		}
		else if (step.has_analog_forward_delta() || step.has_analog_left_delta())
		{
			// The engine never clamps the impulses. It only refuses the whole command if they leave the range
			// VerifySubtickMoves checks, so anything reaching here accumulates as is.
			forwardAxis += step.analog_forward_delta();
			sideAxis += step.analog_left_delta();
			// A stick carries no press or release, and only ever points one way, so the axis sign is the whole
			// held state for it.
			// A stick sweeps continuously and lands on values like 1.3e-05. A key bound to an analog command
			// emits whatever round number the alias was written with, so anything sitting exactly on a
			// hundredth did not come from hardware.
			auto isSynthetic = [](f32 delta)
			{ return fabsf(delta - roundf(delta / SYNTHETIC_ANALOG_GRID) * SYNTHETIC_ANALOG_GRID) < SYNTHETIC_ANALOG_GRID_EPSILON; };
			if (step.analog_forward_delta() != 0.0f)
			{
				setHeld(IN_FORWARD, IN_BACK, heldButton(forwardAxis, IN_FORWARD, IN_BACK), step.when(), true,
						isSynthetic(step.analog_forward_delta()));
			}
			if (step.analog_left_delta() != 0.0f)
			{
				setHeld(IN_MOVELEFT, IN_MOVERIGHT, heldButton(sideAxis, IN_MOVELEFT, IN_MOVERIGHT), step.when(), true,
						isSynthetic(step.analog_left_delta()));
			}
		}
	}
}

void KZAnticheatService::AnalyzeNullsForAxis(const std::deque<InputEvent> &events, u64 button1, u64 button2)
{
	if (!this->player->IsAlive())
	{
		return;
	}
	// Not enough data to check.
	if (events.size() < NUM_MIN_INPUT_EVENTS_FOR_DETECTION)
	{
		if (kz_ac_nulls_debug.Get())
		{
			this->player->PrintAlert(false, true, "Not enough input events for nulls detection (%zu/%d)", events.size(),
									 NUM_MIN_INPUT_EVENTS_FOR_DETECTION);
		}
		return;
	}
	this->nullsFramerateBuffer.clear();
	auto &framerates = this->nullsFramerateBuffer;
	for (const InputEvent &event : events)
	{
		if (event.framerate > 0.0f)
		{
			framerates.push_back(event.framerate);
		}
	}
	if (framerates.size() == 0)
	{
		return;
	}
	std::sort(framerates.begin(), framerates.end());

	f32 medianFramerate = framerates[framerates.size() / 2];
	// The median FPS should not exceed fps_max set by players.
	if (this->currentMaxFps != 0)
	{
		medianFramerate = Max(medianFramerate, 1.0f / this->currentMaxFps); // Min(measured fps, fps_max)
	}
	if (medianFramerate == 0.0f)
	{
		// Fallback to engine tick interval if framerate is unavailable
		medianFramerate = ENGINE_FIXED_TICK_INTERVAL;
	}
	f32 ratio = Clamp((1 / medianFramerate - FPS_FOR_MINIMUM_SUSPICION) / (FPS_FOR_MAXIMUM_SUSPICION - FPS_FOR_MINIMUM_SUSPICION), 0.0f, 1.0f);
	u32 requiredPerfectCstrafes =
		Lerp(1 - ratio, NUM_CONSECUTIVE_PERFECT_CSTRAFE_FOR_DETECTION_MINIMUM, NUM_CONSECUTIVE_PERFECT_CSTRAFE_FOR_DETECTION_MAXIMUM);
	if (events.size() < requiredPerfectCstrafes)
	{
		if (kz_ac_nulls_debug.Get())
		{
			this->player->PrintAlert(false, true, "Not enough input events (%zu/%d)", events.size(), requiredPerfectCstrafes);
		}
		return;
	}
	// Analyze the input events for perfect counter-strafes.
	u32 numOverlaps = 0;
	u32 numPerfect = 0;
	u32 numConsecutivePerfect = 0;
	u32 maxConsecutivePerfect = 0;
	u32 numUnderlaps = 0;
	// Unweighted, so that the weighting cannot move the impact gate.
	u32 numAttempts = 0;
	f32 totalSwitchLoss = 0.0f;

	// Track the last release event and current press state for each direction
	const InputEvent *lastButton1Release = nullptr;
	const InputEvent *lastButton2Release = nullptr;
	bool button1Pressed = false;
	bool button2Pressed = false;

	for (i32 i = 0; i < events.size(); ++i)
	{
		const InputEvent &event = events[i];

		// Track release events regardless of FPS or airspeed
		if (!event.pressed)
		{
			if (event.button == button1)
			{
				lastButton1Release = &event;
				button1Pressed = false;
			}
			else if (event.button == button2)
			{
				lastButton2Release = &event;
				button2Pressed = false;
			}
			continue;
		}

		// Now handling press events
		// First, check if we should analyze this event (apply filters)
		bool shouldAnalyze = true;
		if (event.framerate > 0.0f && 1 / event.framerate < FPS_FOR_MINIMUM_SUSPICION)
		{
			shouldAnalyze = false;
		}
		if (event.airSpeed < MIN_AIR_SPEED_FOR_DETECTION)
		{
			shouldAnalyze = false;
		}
		f32 weight = event.synthetic ? SYNTHETIC_ANALOG_CSTRAFE_WEIGHT : 1.0f;
		// Note that InputEvent::framerate holds a frame time, not a rate.
		f32 nearPerfect = NEAR_PERFECT_MAX_DURATION;
		if (event.framerate > 0.0f)
		{
			nearPerfect = Clamp(NEAR_PERFECT_FRAMETIME_SCALE * event.framerate, NEAR_PERFECT_MIN_DURATION, NEAR_PERFECT_MAX_DURATION);
		}
		// Check for overlap: pressing one key while opposite key is still held
		bool isOverlap = (event.button == button1 && button2Pressed) || (event.button == button2 && button1Pressed);
		if (isOverlap)
		{
			// Measure how long both directions stay held.
			bool foundRelease = false;
			f32 overlapDuration = 0.0f;
			for (i32 j = i + 1; j < events.size(); ++j)
			{
				const InputEvent &nextEvent = events[j];
				f32 elapsed = ((nextEvent.cmdNum - event.cmdNum) + (nextEvent.fraction - event.fraction)) * ENGINE_FIXED_TICK_INTERVAL;
				if (elapsed > GAP_DISCARD_THRESHOLD)
				{
					break;
				}
				if (!nextEvent.pressed)
				{
					foundRelease = true;
					overlapDuration = elapsed;
					break;
				}
			}

			// Holding both directions is time spent not accelerating, same as a gap is.
			if (foundRelease && event.airSpeed >= MIN_AIR_SPEED_FOR_DETECTION)
			{
				numAttempts++;
				totalSwitchLoss += overlapDuration;
			}
			// Without a release inside the window there is nothing to classify: the player is simply holding
			// both directions, which is not a counter-strafe attempt.
			if (foundRelease)
			{
				if (overlapDuration < nearPerfect)
				{
					// Close enough to simultaneous to be a perfect counter-strafe rather than an overlap.
					if (shouldAnalyze)
					{
						if (kz_ac_nulls_debug.Get() && event.cmdNum == this->currentCmdNum)
						{
							this->player->PrintConsole(false, true, "Perfect (%.4f ms early) @ %f", overlapDuration * 1000,
													   event.cmdNum + event.fraction);
						}
						numPerfect += weight;
						numConsecutivePerfect += weight;
						if (numConsecutivePerfect > maxConsecutivePerfect)
						{
							maxConsecutivePerfect = numConsecutivePerfect;
						}
					}
				}
				else if (event.airSpeed >= MIN_AIR_SPEED_FOR_DETECTION)
				{
					if (kz_ac_nulls_debug.Get() && event.cmdNum == this->currentCmdNum)
					{
						this->player->PrintConsole(false, true, "Overlap %.3f ms @ %f", overlapDuration * 1000, event.cmdNum + event.fraction);
					}
					numOverlaps += weight;
					numConsecutivePerfect = 0;
				}
			}
		}

		// Update press state
		if (event.button == button1)
		{
			button1Pressed = true;
		}
		else if (event.button == button2)
		{
			button2Pressed = true;
		}

		// If it was an overlap or we're not analyzing, skip the rest
		if (isOverlap || !shouldAnalyze)
		{
			continue;
		}

		// Not an overlap, so check for perfect/underlap counter-strafe
		const InputEvent *oppositeRelease = nullptr;
		if (event.button == button1 && lastButton2Release != nullptr)
		{
			oppositeRelease = lastButton2Release;
		}
		else if (event.button == button2 && lastButton1Release != nullptr)
		{
			oppositeRelease = lastButton1Release;
		}

		if (oppositeRelease == nullptr)
		{
			continue; // No counter-strafe detected
		}

		// Calculate timing between opposite key release and current key press
		f32 timeDiff = ((event.cmdNum - oppositeRelease->cmdNum) + (event.fraction - oppositeRelease->fraction)) * ENGINE_FIXED_TICK_INTERVAL;

		// Only consider this if it's reasonably close (not a brand new input)
		if (timeDiff > GAP_DISCARD_THRESHOLD)
		{
			continue;
		}

		numAttempts++;
		totalSwitchLoss += timeDiff;

		// Note: timeDiff < 0 (overlap) is already handled earlier in the loop
		if (timeDiff < nearPerfect)
		{
			if (kz_ac_nulls_debug.Get() && event.cmdNum == this->currentCmdNum)
			{
				this->player->PrintConsole(false, true, "Perfect (%.4f ms late) @ %f", timeDiff * 1000, event.cmdNum + event.fraction);
			}
			// Perfect: no meaningful time between release and press
			numPerfect += weight;
			numConsecutivePerfect += weight;
			if (numConsecutivePerfect > maxConsecutivePerfect)
			{
				maxConsecutivePerfect = numConsecutivePerfect;
			}
		}
		else
		{
			if (kz_ac_nulls_debug.Get() && event.cmdNum == this->currentCmdNum)
			{
				this->player->PrintConsole(false, true, "Underlap %.3f ms @ %f", timeDiff * 1000, event.cmdNum + event.fraction);
			}
			// Underlap: gap between release and press
			numUnderlaps++;
		}
	}
	// Averaging over every attempt is what makes this a measure of impact rather than of style. Taking it
	// over the misses alone ignores the switches that cost nothing, so a player who nulls most of the time
	// and misses sloppily the rest reads as though every switch were sloppy.
	f32 meanSwitchLoss = numAttempts > 0 ? totalSwitchLoss / (f32)numAttempts : 0.0f;
	if (meanSwitchLoss >= SWITCH_LOSS_FORGIVENESS_THRESHOLD)
	{
		if (kz_ac_nulls_debug.Get())
		{
			this->player->PrintAlert(false, true, "Counter-strafes already cost too much to be worth checking: %.2f ms each", meanSwitchLoss * 1000);
		}
		return;
	}

	// The more a player's counter-strafes cost them, the less nulling would have gained them.
	// We scale up the required perfect cstrafes accordingly.
	f32 lossRatio = Clamp(meanSwitchLoss / SWITCH_LOSS_FORGIVENESS_THRESHOLD, 0.0f, 1.0f);
	// Squared because we want to be more strict when the switches cost the player almost nothing.
	u32 adjustedRequiredPerfectCstrafes =
		Lerp(lossRatio * lossRatio, requiredPerfectCstrafes, (u32)NUM_CONSECUTIVE_PERFECT_CSTRAFE_FOR_DETECTION_MAXIMUM);

	const char *axisName = (button1 == IN_FORWARD) ? "forward/backward" : "left/right";

	// The streak is the best run anywhere in the window, not the run still open at the end of it.
	if (maxConsecutivePerfect >= adjustedRequiredPerfectCstrafes)
	{
		std::string details = tinyformat::format(
			"Nulls detection on axis %s. Streak: %d/%d, perfect %d, UL: %d, OL: %d, loss/switch: %.2f ms, FPS: %.2f", axisName, maxConsecutivePerfect,
			adjustedRequiredPerfectCstrafes, numPerfect, numUnderlaps, numOverlaps, meanSwitchLoss * 1000, 1 / medianFramerate);
		this->MarkInfraction(KZAnticheatService::Infraction::Type::Nulls, details);
	}

	if (kz_ac_nulls_debug.Get())
	{
		this->player->PrintAlert(false, true, "Perfect: %d (streak %d, ban %d) | Overlap %d\nLoss/switch: %.2f ms | FPS: %.1f | Sample count %d",
								 numPerfect, maxConsecutivePerfect, adjustedRequiredPerfectCstrafes, numOverlaps, meanSwitchLoss * 1000,
								 1 / medianFramerate, numAttempts);
	}
}

void KZAnticheatService::CheckNulls()
{
	this->AnalyzeNullsForAxis(this->recentForwardBackwardEvents, IN_FORWARD, IN_BACK);
	this->AnalyzeNullsForAxis(this->recentLeftRightEvents, IN_MOVELEFT, IN_MOVERIGHT);
}

void KZAnticheatService::CleanupOldInputEvents()
{
	// 2048 input events should be more than enough to cover recent history.
	while (this->recentForwardBackwardEvents.size() > 2048)
	{
		this->recentForwardBackwardEvents.pop_front();
	}
	while (this->recentLeftRightEvents.size() > 2048)
	{
		this->recentLeftRightEvents.pop_front();
	}
}
