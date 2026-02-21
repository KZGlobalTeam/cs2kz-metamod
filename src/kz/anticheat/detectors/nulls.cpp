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
// Only count counterstrafe attempts as underlap if the keypresses are at most this far apart. Consider higher values as brand new inputs.
#define UNDERLAP_COUNT_THRESHOLD      0.2f
#define UNDERLAP_PERCENTAGE_THRESHOLD 0.1f // At least 10% of the strafes should have underlap to consider the median underlap duration.
// Higher underlap average means the player are unlikely to be nulling.
// If the player's underlap average is above this value, we won't consider them for nulls detection.
// If 10% or more of their strafes have underlap, we should start taking the threshold below into consideration.
#define UNDERLAP_MEDIAN_FORGIVENESS_THRESHOLD 0.02f // ~10% of a flat ground jump, considering 7.5 strafes on average

CConVar<bool> kz_ac_nulls_debug("kz_ac_nulls_debug", FCVAR_CHEAT, "Enable nulls detector debug messages", false);

void KZAnticheatService::CreateInputEvents(PlayerCommand *cmd)
{
	// Ignore bots.
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	// Note: We assume that the subtick inputs here is consistent with the button states in cmd->buttonstates.
	// Usually the only way this can be false is if the client is not legitimate.
	if (cmd->base().subtick_moves_size() == 0)
	{
		// Technically the player can abuse this to hide their nulls but this will be caught by the subtick abuse detection.
		return;
	}
	u64 oldButtons = 0;
	Vector &lastMovementImpulses = this->player->GetMoveServices()->m_vecLastMovementImpulses;
	if (lastMovementImpulses.x > 0)
	{
		oldButtons |= IN_FORWARD;
	}
	else if (lastMovementImpulses.x < 0)
	{
		oldButtons |= IN_BACK;
	}

	if (lastMovementImpulses.y > 0)
	{
		oldButtons |= IN_MOVELEFT;
	}
	else if (lastMovementImpulses.y < 0)
	{
		oldButtons |= IN_MOVERIGHT;
	}

	for (i32 i = 0; i < cmd->base().subtick_moves_size(); ++i)
	{
		bool hasButton = cmd->base().subtick_moves(i).has_button();
		// Buttons that are not direction inputs are ignored.
		u64 button = cmd->base().subtick_moves(i).button();
		if (hasButton)
		{
			if (button == IN_FORWARD || button == IN_BACK || button == IN_MOVELEFT || button == IN_MOVERIGHT)
			{
				const CSubtickMoveStep &step = cmd->base().subtick_moves(i);
				InputEvent event;
				event.cmdNum = cmd->cmdNum;
				event.fraction = step.when();
				event.button = button;
				event.pressed = step.pressed();
				INetChannelInfo *netchan = interfaces::pEngine->GetPlayerNetInfo(this->player->GetPlayerSlot());
				netchan->GetRemoteFramerate(&event.framerate, nullptr, nullptr);
				// Only record airspeed if the player might be airstrafing.
				if ((this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND) == 0 && this->player->GetMoveType() == MOVETYPE_WALK)
				{
					// This isn't the actual airspeed at the time of the input, but it's close enough for our purposes.
					event.airSpeed = this->player->moveDataPost.m_vecVelocity.Length2D();
				}
				if (button == IN_FORWARD || button == IN_BACK)
				{
					this->recentForwardBackwardEvents.push_back(event);
				}
				else // IN_MOVELEFT || IN_MOVERIGHT
				{
					this->recentLeftRightEvents.push_back(event);
				}
			}
		}
		else if (cmd->base().subtick_moves(i).has_analog_forward_delta() || cmd->base().subtick_moves(i).has_analog_left_delta())
		{
			// Analog movement input changes
			const CSubtickMoveStep &step = cmd->base().subtick_moves(i);
			// Helper to record an input event
			auto recordAnalogEvent = [&](u64 button, bool pressed)
			{
				InputEvent event {cmd->cmdNum, step.when(), -1.0f, button, pressed, true, -1.0f};
				INetChannelInfo *netchan = interfaces::pEngine->GetPlayerNetInfo(this->player->GetPlayerSlot());
				netchan->GetRemoteFramerate(&event.framerate, nullptr, nullptr);
				if ((this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND) == 0 && this->player->GetMoveType() == MOVETYPE_WALK)
				{
					event.airSpeed = this->player->moveDataPost.m_vecVelocity.Length2D();
				}
				if (button == IN_FORWARD || button == IN_BACK)
				{
					this->recentForwardBackwardEvents.push_back(event);
				}
				else // IN_MOVELEFT || IN_MOVERIGHT
				{
					this->recentLeftRightEvents.push_back(event);
				}
			};

			// Helper to release a button if it's currently pressed
			auto tryRelease = [&](int button)
			{
				if (oldButtons & button)
				{
					recordAnalogEvent(button, false);
					oldButtons &= ~button;
				}
			};

			// Helper to press a button if it's not currently pressed
			auto tryPress = [&](int button)
			{
				if (!(oldButtons & button))
				{
					recordAnalogEvent(button, true);
					oldButtons |= button;
				}
			};
			if (step.has_analog_forward_delta() && step.analog_forward_delta() != 0.0f)
			{
				float delta = step.analog_forward_delta();
				if (delta != 0.0f)
				{
					if (delta == 1.0f)
					{
						// Back to neutral OR neutral to forward
						if (oldButtons & IN_BACK)
						{
							tryRelease(IN_BACK);
						}
						else
						{
							tryPress(IN_FORWARD);
						}
					}
					else if (delta == -1.0f)
					{
						// Forward to neutral OR neutral to back
						if (oldButtons & IN_FORWARD)
						{
							tryRelease(IN_FORWARD);
						}
						else
						{
							tryPress(IN_BACK);
						}
					}
					else if (delta == 2.0f)
					{
						// Back to forward
						tryRelease(IN_BACK);
						tryPress(IN_FORWARD);
					}
					else if (delta == -2.0f)
					{
						// Forward to back
						tryRelease(IN_FORWARD);
						tryPress(IN_BACK);
					}
				}
			}
			if (step.has_analog_left_delta() && step.analog_left_delta() != 0.0f)
			{
				float delta = step.analog_left_delta();
				if (delta != 0.0f)
				{
					if (delta == 1.0f)
					{
						// Right to neutral OR neutral to left
						if (oldButtons & IN_MOVERIGHT)
						{
							tryRelease(IN_MOVERIGHT);
						}
						else
						{
							tryPress(IN_MOVELEFT);
						}
					}
					else if (delta == -1.0f)
					{
						// Left to neutral OR neutral to right
						if (oldButtons & IN_MOVELEFT)
						{
							tryRelease(IN_MOVELEFT);
						}
						else
						{
							tryPress(IN_MOVERIGHT);
						}
					}
					else if (delta == 2.0f)
					{
						// Right to left
						tryRelease(IN_MOVERIGHT);
						tryPress(IN_MOVELEFT);
					}
					else if (delta == -2.0f)
					{
						// Left to right
						tryRelease(IN_MOVELEFT);
						tryPress(IN_MOVERIGHT);
					}
				}
			}
		}
		// The button loop did not take into account buttons that are pressed and released on the same tick fraction.
		// They should cancel each other out, so we need to clean them up.
		auto cleanupEvents = [](std::deque<InputEvent> &events)
		{
			auto it = events.begin();
			while (it != events.end())
			{
				auto next = std::next(it);
				if (next == events.end())
				{
					break;
				}
				if (it->cmdNum == next->cmdNum && it->fraction == next->fraction && it->button == next->button && it->pressed != next->pressed)
				{
					// Cancel out
					it = events.erase(it);
					it = events.erase(it);
				}
				else
				{
					++it;
				}
			}
		};
		cleanupEvents(this->recentForwardBackwardEvents);
		cleanupEvents(this->recentLeftRightEvents);
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
	std::vector<f32> framerates;
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
	std::vector<f32> underlapDurations;

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
		// Check for overlap: pressing one key while opposite key is still held
		bool isOverlap = (event.button == button1 && button2Pressed) || (event.button == button2 && button1Pressed);
		if (isOverlap)
		{
			// Check if the opposite key gets released at the same tick/fraction (perfect null)
			bool nulled = false;
			for (i32 j = i + 1; j < events.size(); ++j)
			{
				const InputEvent &nextEvent = events[j];
				// If we've moved to a different tick/fraction, stop looking
				if (nextEvent.cmdNum != event.cmdNum || nextEvent.fraction != event.fraction)
				{
					break;
				}
				// Check if this is the release of the opposite button
				if (!nextEvent.pressed)
				{
					if ((event.button == button1 && nextEvent.button == button2) || (event.button == button2 && nextEvent.button == button1))
					{
						nulled = true;
						break;
					}
				}
			}

			if (nulled)
			{
				// This is actually a perfect counter-strafe, not an overlap
				if (shouldAnalyze)
				{
					if (kz_ac_nulls_debug.Get() && event.cmdNum == this->currentCmdNum)
					{
						this->player->PrintConsole(false, true, "Perfect @ %f", event.cmdNum + event.fraction);
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
					this->player->PrintConsole(false, true, "Overlap @ %f", event.cmdNum + event.fraction);
				}
				numOverlaps += weight;
				numConsecutivePerfect = 0;
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
		if (timeDiff > UNDERLAP_COUNT_THRESHOLD)
		{
			continue;
		}

		// Note: timeDiff < 0 (overlap) is already handled earlier in the loop
		if (timeDiff == 0.0f)
		{
			if (kz_ac_nulls_debug.Get() && event.cmdNum == this->currentCmdNum)
			{
				this->player->PrintConsole(false, true, "Perfect @ %f", event.cmdNum + event.fraction);
			}
			// Perfect: exactly 0 time between release and press
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

	if (numConsecutivePerfect >= adjustedRequiredPerfectCstrafes)
	{
		std::string details =
			tinyformat::format("Nulls detection on axis %s. Streak: %d/%d, total %d/%d, OL: %d, DA median: %.2f ms, FPS: %.2f",
							   (button1 == IN_FORWARD || button2 == IN_BACK) ? "forward/backward" : "left/right", numConsecutivePerfect,
							   adjustedRequiredPerfectCstrafes, numPerfect, total, numOverlaps, underlapMedian * 1000, 1 / medianFramerate);
		META_CONPRINTF("%s\n", details.c_str());
		this->MarkInfraction(KZAnticheatService::Infraction::Type::Nulls, details);
	}

	if (kz_ac_nulls_debug.Get())
	{
		this->player->PrintAlert(
			false, true, "Perfect: %d (consecutive %d, ban %d) | Overlap %d\nUnderlap median: %.1f ms | FPS: %.1f | Sample count %d", numPerfect,
			numConsecutivePerfect, adjustedRequiredPerfectCstrafes, numOverlaps, underlapMedian * 1000, 1 / medianFramerate, (i32)(total));
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
