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
#define FPS_FOR_MINIMUM_SUSPICION   64.0f // We shouldn't count any attempt below this FPS.
#define FPS_FOR_MAXIMUM_SUSPICION   256.0f
#define ANALOG_CSTRAFE_WEIGHT       2.0f // Perfect analog strafes are extremely suspicious. Most (if not all) ingame null aliases abuse analog inputs.
#define MIN_AIR_SPEED_FOR_DETECTION 100.0f // Only consider airstrafes with at least this airspeed to avoid false positives.
// Only count counterstrafe attempts if the keypresses are at most this far apart, in either direction.
// Consider higher values as brand new inputs rather than a counter-strafe attempt.
#define GAP_DISCARD_THRESHOLD         0.2f
#define UNDERLAP_PERCENTAGE_THRESHOLD 0.1f // At least 10% of the strafes should have underlap to consider the median underlap duration.
// Higher underlap average means the player are unlikely to be nulling.
// If the player's underlap average is above this value, we won't consider them for nulls detection.
// If 10% or more of their strafes have underlap, we should start taking the threshold below into consideration.
#define UNDERLAP_MEDIAN_FORGIVENESS_THRESHOLD 0.02f // ~10% of a flat ground jump, considering 7.5 strafes on average

// An exact comparison against 0 misses any null that carries a fraction of a subtick of jitter.
// Treat a small gap in either direction as a perfect swap.
#define NEAR_PERFECT_FRAMETIME_SCALE 0.5f
#define NEAR_PERFECT_MIN_DURATION    (ENGINE_FIXED_TICK_INTERVAL / 64.0f) // ~0.25ms
#define NEAR_PERFECT_MAX_DURATION    0.001f                               // 1ms

CConVar<bool> kz_ac_nulls_debug("kz_ac_nulls_debug", FCVAR_CHEAT, "Enable nulls detector debug messages", false);

// forwardmove/sizemove/upmove is rejected by the server if it's over 10.
// In reality this never happens (either 0 or 1), but this matches what the server does.
static_global f32 SanitizeMoveValue(f32 value)
{
	if (value != value || fabsf(value) >= 10.0f)
	{
		return 0.0f;
	}
	return value;
}

void KZAnticheatService::CreateInputEvents(PlayerCommand *cmd)
{
	// Ignore bots.
	if (this->player->IsFakeClient() || this->player->IsCSTV())
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

	INetChannelInfo *netchan = interfaces::pEngine->GetPlayerNetInfo(this->player->GetPlayerSlot());
	bool airborne = (this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND) == 0 && this->player->GetMoveType() == MOVETYPE_WALK;
	// This isn't the actual airspeed at the time of the input, but it's close enough for our purposes.
	f32 airSpeed = airborne ? this->player->moveDataPost.m_vecVelocity.Length2D() : -1.0f;

	// Emit the press/release pair implied by an axis crossing between directions. Anything the axis does
	// while staying on one side of zero is a magnitude change, not a direction change, and carries no
	// counter-strafe information.
	auto emitTransition = [&](u64 wasHeld, u64 nowHeld, f32 when, bool analog)
	{
		if (wasHeld == nowHeld)
		{
			return;
		}
		auto push = [&](u64 button, bool pressed)
		{
			InputEvent event {cmd->cmdNum, when, -1.0f, button, pressed, analog, airSpeed};
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
		if (wasHeld)
		{
			push(wasHeld, false);
		}
		if (nowHeld)
		{
			push(nowHeld, true);
		}
	};

	if (cmd->base().subtick_moves_size() == 0 || VerifySubtickMoves(cmd, forwardAxis, sideAxis) != SubtickRejection::None)
	{
		// A command is allowed to change the movement buttons without carrying any subtick move.
		// In such scenario, SetupMove derives the impulses from the final button state
		// and treat it as an input that happened at the very start of the tick.
		u64 buttons = cmd->base().buttons_pb().buttonstate1();
		f32 newForward = (buttons & (IN_FORWARD | IN_BACK)) ? SanitizeMoveValue(cmd->base().forwardmove()) : 0.0f;
		f32 newSide = (buttons & (IN_MOVELEFT | IN_MOVERIGHT)) ? SanitizeMoveValue(cmd->base().leftmove()) : 0.0f;
		emitTransition(heldButton(forwardAxis, IN_FORWARD, IN_BACK), heldButton(newForward, IN_FORWARD, IN_BACK), 0.0f, false);
		emitTransition(heldButton(sideAxis, IN_MOVELEFT, IN_MOVERIGHT), heldButton(newSide, IN_MOVELEFT, IN_MOVERIGHT), 0.0f, false);
		return;
	}

	for (i32 i = 0; i < cmd->base().subtick_moves_size(); ++i)
	{
		const CSubtickMoveStep &step = cmd->base().subtick_moves(i);
		u64 oldForward = heldButton(forwardAxis, IN_FORWARD, IN_BACK);
		u64 oldSide = heldButton(sideAxis, IN_MOVELEFT, IN_MOVERIGHT);

		if (step.has_button() && step.button())
		{
			// Buttons that are not direction inputs are ignored.
			u64 button = step.button();
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
			else
			{
				continue;
			}
		}
		else if (step.has_analog_forward_delta() || step.has_analog_left_delta())
		{
			forwardAxis += step.analog_forward_delta();
			sideAxis += step.analog_left_delta();
		}
		else
		{
			continue;
		}

		// The engine never clamps the impulses. It only refuses the whole command if they leave the range
		// VerifySubtickMoves checks, so anything reaching here accumulates as is.
		emitTransition(oldForward, heldButton(forwardAxis, IN_FORWARD, IN_BACK), step.when(), !step.has_button());
		emitTransition(oldSide, heldButton(sideAxis, IN_MOVELEFT, IN_MOVERIGHT), step.when(), !step.has_button());
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
	this->nullsUnderlapBuffer.clear();
	auto &underlapDurations = this->nullsUnderlapBuffer;

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
		f32 weight = event.analog ? ANALOG_CSTRAFE_WEIGHT : 1.0f;
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
			underlapDurations.push_back(timeDiff);
		}
	}
	f32 underlapMedian = 0.0f;
	if (!underlapDurations.empty())
	{
		std::sort(underlapDurations.begin(), underlapDurations.end());
		underlapMedian = underlapDurations[underlapDurations.size() / 2];
	}

	u32 total = numOverlaps + numPerfect + underlapDurations.size();
	// Ban if criteria met
	if (underlapMedian >= UNDERLAP_MEDIAN_FORGIVENESS_THRESHOLD && ((f32)underlapDurations.size() / (f32)(total) >= UNDERLAP_PERCENTAGE_THRESHOLD))
	{
		if (kz_ac_nulls_debug.Get())
		{
			this->player->PrintAlert(false, true, "Underlap median too high: %.2f ms", underlapMedian * 1000);
		}
		return;
	}

	// The higher the underlap median, the less likely the player is nulling.
	// We scale up the required perfect cstrafes based on how high the underlap median is.
	f32 underlapRatio = Clamp(underlapMedian / UNDERLAP_MEDIAN_FORGIVENESS_THRESHOLD, 0.0f, 1.0f);
	// Squared because we want to be more strict on lower underlap medians.
	u32 adjustedRequiredPerfectCstrafes =
		Lerp(underlapRatio * underlapRatio, requiredPerfectCstrafes, (u32)NUM_CONSECUTIVE_PERFECT_CSTRAFE_FOR_DETECTION_MAXIMUM);

	const char *axisName = (button1 == IN_FORWARD) ? "forward/backward" : "left/right";

	// The streak is the best run anywhere in the window, not the run still open at the end of it.
	if (maxConsecutivePerfect >= adjustedRequiredPerfectCstrafes)
	{
		std::string details = tinyformat::format("Nulls detection on axis %s. Streak: %d/%d, total %d/%d, OL: %d, DA median: %.2f ms, FPS: %.2f",
												 axisName, maxConsecutivePerfect, adjustedRequiredPerfectCstrafes, numPerfect, total, numOverlaps,
												 underlapMedian * 1000, 1 / medianFramerate);
		this->MarkInfraction(KZAnticheatService::Infraction::Type::Nulls, details);
	}

	if (kz_ac_nulls_debug.Get())
	{
		this->player->PrintAlert(false, true, "Perfect: %d (streak %d, ban %d) | Overlap %d\nUnderlap median: %.1f ms | FPS: %.1f | Sample count %d",
								 numPerfect, maxConsecutivePerfect, adjustedRequiredPerfectCstrafes, numOverlaps, underlapMedian * 1000,
								 1 / medianFramerate, (i32)(total));
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
