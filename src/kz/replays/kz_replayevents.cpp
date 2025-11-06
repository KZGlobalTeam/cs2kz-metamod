#include "cs2kz.h"
#include "kz/kz.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/language/kz_language.h"
#include "kz_replayevents.h"
#include "kz_replaydata.h"
#include "kz_replaybot.h"

extern CConVar<bool> kz_replay_playback_debug;

namespace KZ::replaysystem::events
{

	void CheckEvents(KZPlayer &player)
	{
		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		TickData *tickData = &replay->tickData[replay->currentTick];
		u32 serverTick = tickData->serverTick;

		while (replay->currentEvent < replay->numEvents && replay->events[replay->currentEvent].serverTick <= serverTick)
		{
			RpEvent *event = &replay->events[replay->currentEvent];

			switch (event->type)
			{
				case RPEVENT_TIMER_EVENT:
					HandleTimerEvent(player, event, replay);
					break;

				case RPEVENT_MODE_CHANGE:
					HandleModeChangeEvent(player, event);
					break;

				case RPEVENT_STYLE_CHANGE:
					HandleStyleChangeEvent(player, event);
					break;

				case RPEVENT_TELEPORT:
					HandleTeleportEvent(player, event);
					break;
			}

			replay->currentEvent++;
		}
	}

	void CheckJumps(KZPlayer &player)
	{
		auto replay = data::GetCurrentReplay();
		if (!replay->playingReplay)
		{
			return;
		}

		TickData *tickData = &replay->tickData[replay->currentTick];
		u32 serverTick = tickData->serverTick;

		while (replay->currentJump < replay->numJumps && replay->jumps[replay->currentJump].overall.serverTick <= serverTick)
		{
			RpJumpStats *jump = &replay->jumps[replay->currentJump];
			if (kz_replay_playback_debug.Get())
			{
				utils::PrintChatAll("Jump event: tick %d", jump->overall.serverTick);
			}
			jump->PrintJump(&player);
			replay->currentJump++;
		}
	}

	void HandleTimerEvent(KZPlayer &player, const RpEvent *event, data::ReplayPlayback *replay)
	{
		if (kz_replay_playback_debug.Get())
		{
			utils::PrintChatAll("Timer event: tick %d, type %d, index %d, time %.3f", event->serverTick, event->data.timer.type,
								event->data.timer.index, event->data.timer.time);
		}

		switch (event->data.timer.type)
		{
			case RpEvent::RpEventData::TimerEvent::TIMER_START:
			{
				const KZCourseDescriptor *course = KZ::course::GetCourseByCourseID(event->data.timer.index);
				if (!course || course->GetName().IsEmpty())
				{
					break;
				}

				for (KZPlayer *spec = player.specService->GetNextSpectator(NULL); spec != NULL; spec = player.specService->GetNextSpectator(spec))
				{
					spec->timerService->PlayTimerStartSound();
				}

				replay->startTime = g_pKZUtils->GetServerGlobals()->curtime - event->data.timer.time;
				replay->endTime = 0.0f;
				replay->lastSplitTime = 0.0f;
				replay->lastCPZTime = 0.0f;
				replay->lastStageTime = 0.0f;
				replay->stopTick = 0;
				V_snprintf(replay->courseName, sizeof(replay->courseName), "%s", course->GetName().Get());
				replay->paused = false;
				break;
			}

			case RpEvent::RpEventData::TimerEvent::TIMER_END:
			{
				const KZCourseDescriptor *courseDesc = KZ::course::GetCourseByCourseID(event->data.timer.index);
				if (!courseDesc || courseDesc->GetName().IsEmpty())
				{
					break;
				}
				if (replay->startTime == 0.0f || !courseDesc->GetName().IsEqual_FastCaseInsensitive(replay->courseName))
				{
					break;
				}

				char formattedTime[32];
				utils::FormatTime(event->data.timer.time, formattedTime, sizeof(formattedTime));

				CUtlString combinedModeStyleText;
				combinedModeStyleText.Format("{purple}%s{grey}", player.modeService->GetModeName());
				FOR_EACH_VEC(player.styleServices, i)
				{
					KZStyleService *style = player.styleServices[i];
					combinedModeStyleText += " +{grey2}";
					combinedModeStyleText.Append(style->GetStyleName());
					combinedModeStyleText += "{grey}";
				}

				replay->endTime = event->data.timer.time;
				std::string teleportText = "{blue}PRO{grey}";
				if (replay->currentTeleport > 0)
				{
					teleportText = replay->currentTeleport == 1
									   ? player.languageService->PrepareMessage("1 Teleport Text")
									   : player.languageService->PrepareMessage("2+ Teleports Text", replay->currentTeleport);
				}

				for (KZPlayer *spec = player.specService->GetNextSpectator(NULL); spec != NULL; spec = player.specService->GetNextSpectator(spec))
				{
					spec->timerService->PlayTimerEndSound();
				}

				player.languageService->PrintChat(true, true, "Beat Course Info - Basic", replay->header.player.name,
												  courseDesc ? courseDesc->GetName() : "unknown", formattedTime, combinedModeStyleText.Get(),
												  teleportText.c_str());
				replay->startTime = 0.0f;
				replay->stopTick = replay->currentTick;
				replay->courseName[0] = '\0';
				break;
			}

			case RpEvent::RpEventData::TimerEvent::TIMER_STOP:
			{
				if (replay->startTime == 0.0f)
				{
					break;
				}

				for (KZPlayer *spec = player.specService->GetNextSpectator(NULL); spec != NULL; spec = player.specService->GetNextSpectator(spec))
				{
					spec->timerService->PlayTimerStopSound();
				}

				replay->endTime = event->data.timer.time;
				replay->stopTick = replay->currentTick;
				replay->startTime = 0.0f;
				replay->courseName[0] = '\0';
				break;
			}

			case RpEvent::RpEventData::TimerEvent::TIMER_SPLIT:
			{
				if (replay->startTime == 0.0f)
				{
					break;
				}

				CUtlString time = utils::FormatTime(event->data.timer.time);
				if (replay->lastSplitTime != 0)
				{
					f64 diff = event->data.timer.time - replay->lastSplitTime;
					CUtlString splitTime = KZTimerService::FormatDiffTime(diff);
					splitTime.Format(" {grey}({default}%s{grey})", splitTime.Get());
					time.Append(splitTime.Get());
				}

				for (KZPlayer *spec = player.specService->GetNextSpectator(NULL); spec != NULL; spec = player.specService->GetNextSpectator(spec))
				{
					spec->timerService->PlayReachedSplitSound();
				}

				player.languageService->PrintChat(true, true, "Course Split Reached", event->data.timer.index, time.Get(), "", "");
				replay->lastSplitTime = event->data.timer.time;
				break;
			}

			case RpEvent::RpEventData::TimerEvent::TIMER_CPZ:
			{
				if (replay->startTime == 0.0f)
				{
					break;
				}

				CUtlString time = utils::FormatTime(event->data.timer.time);
				if (replay->lastCPZTime != 0)
				{
					f64 diff = event->data.timer.time - replay->lastCPZTime;
					CUtlString splitTime = KZTimerService::FormatDiffTime(diff);
					splitTime.Format(" {grey}({default}%s{grey})", splitTime.Get());
					time.Append(splitTime.Get());
				}

				for (KZPlayer *spec = player.specService->GetNextSpectator(NULL); spec != NULL; spec = player.specService->GetNextSpectator(spec))
				{
					spec->timerService->PlayReachedCheckpointSound();
				}

				player.languageService->PrintChat(true, true, "Course Checkpoint Reached", event->data.timer.index, time.Get(), "", "");
				replay->lastCPZTime = event->data.timer.time;
				break;
			}

			case RpEvent::RpEventData::TimerEvent::TIMER_STAGE:
			{
				CUtlString time = utils::FormatTime(event->data.timer.time);
				if (replay->lastStageTime != 0)
				{
					f64 diff = event->data.timer.time - replay->lastStageTime;
					CUtlString splitTime = KZTimerService::FormatDiffTime(diff);
					splitTime.Format(" {grey}({default}%s{grey})", splitTime.Get());
					time.Append(splitTime.Get());
				}

				for (KZPlayer *spec = player.specService->GetNextSpectator(NULL); spec != NULL; spec = player.specService->GetNextSpectator(spec))
				{
					spec->timerService->PlayReachedStageSound();
				}

				player.languageService->PrintChat(true, true, "Course Stage Reached", event->data.timer.index, time.Get(), "", "");
				replay->lastStageTime = event->data.timer.time;
				break;
			}

			case RpEvent::RpEventData::TimerEvent::TIMER_PAUSE:
			{
				replay->paused = true;
				break;
			}

			case RpEvent::RpEventData::TimerEvent::TIMER_RESUME:
			{
				replay->paused = false;
				break;
			}
		}
	}

	void HandleModeChangeEvent(KZPlayer &player, const RpEvent *event)
	{
		if (kz_replay_playback_debug.Get())
		{
			utils::PrintChatAll("Mode change event: tick %d, mode %s", event->serverTick, event->data.modeChange.name);
		}

		g_pKZModeManager->SwitchToMode(&player, event->data.modeChange.name, true, true, false);
	}

	void HandleStyleChangeEvent(KZPlayer &player, const RpEvent *event)
	{
		if (kz_replay_playback_debug.Get())
		{
			utils::PrintChatAll("Style change event: tick %d, style %s, clear style %d", event->serverTick, event->data.styleChange.name,
								event->data.styleChange.clearStyles);
			META_CONPRINTF("Style change event: tick %d, style %s, clear style %d\n", event->serverTick, event->data.styleChange.name,
						   event->data.styleChange.clearStyles);
		}

		if (event->data.styleChange.clearStyles)
		{
			g_pKZStyleManager->ClearStyles(&player, true, false);
		}
		g_pKZStyleManager->AddStyle(&player, event->data.styleChange.name, true, false);
	}

	void HandleTeleportEvent(KZPlayer &player, const RpEvent *event)
	{
		if (kz_replay_playback_debug.Get())
		{
			utils::PrintChatAll("Teleport event: tick %d", event->serverTick);
		}

		player.GetPlayerPawn()->Teleport(event->data.teleport.hasOrigin ? (Vector *)&event->data.teleport.origin : nullptr,
										 event->data.teleport.hasAngles ? (QAngle *)&event->data.teleport.angles : nullptr,
										 event->data.teleport.hasVelocity ? (Vector *)&event->data.teleport.velocity : nullptr);
	}

	void ReprocessEventsUpToTick(data::ReplayPlayback *replay, u32 targetTick)
	{
		// Get the bot player for applying changes
		auto bot = bot::GetBot();
		if (!bot)
		{
			return;
		}
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
		if (!player)
		{
			return;
		}

		// Get the server tick of the target tick data
		u32 targetServerTick = replay->tickData[targetTick].serverTick;
		u32 currentServerTick = replay->currentTick < replay->tickCount ? replay->tickData[replay->currentTick].serverTick : 0;

		// Optimization: If seeking forward, we only need to process events from current position
		bool isSeekingForward = (targetTick > replay->currentTick) && replay->currentTick > 0;
		u32 startEventIndex = 0;
		u32 startWeaponIndex = 0;
		u32 startJumpIndex = 0;

		if (isSeekingForward)
		{
			// Start from current positions when seeking forward
			startEventIndex = replay->currentEvent;
			startWeaponIndex = replay->currentWeapon;
			startJumpIndex = replay->currentJump;
		}
		else
		{
			// Reset player state when seeking backward or from beginning
			g_pKZModeManager->SwitchToMode(player, "Vanilla", true, false, false);
			g_pKZStyleManager->ClearStyles(player, true, false);

			// Reset all tracking indices
			replay->currentWeapon = 0;
			replay->currentJump = 0;
			replay->currentEvent = 0;

			// Reset checkpoint/teleport counters
			replay->currentCpIndex = -1;
			replay->currentCheckpoint = 0;
			replay->currentTeleport = 0;

			// Reset timer state
			replay->courseName[0] = '\0';
			replay->startTime = 0.0f;
			replay->paused = false;
			replay->endTime = 0.0f;
			replay->stopTick = 0;
			replay->lastSplitTime = 0.0f;
			replay->lastCPZTime = 0.0f;
			replay->lastStageTime = 0.0f;

			// Reset replay pause state
			replay->replayPaused = false;
		}

		// Variables for pause time calculation during event processing
		f32 totalPauseTime = 0.0f;
		bool inPause = false;
		u32 pauseStartTick = 0;
		bool inActiveTimerRun = false; // Track if we're currently in the target timer run

		// Process events from start index to target server tick
		for (u32 i = startEventIndex; i < replay->numEvents; i++)
		{
			RpEvent *event = &replay->events[i];
			if (event->serverTick > targetServerTick)
			{
				// If we're in a pause that extends past our target, add partial pause time
				if (inPause && inActiveTimerRun && replay->startTime > 0.0f)
				{
					// Find tick data indices
					u32 pauseStartTickIndex = 0;
					for (u32 j = 0; j < replay->tickCount; j++)
					{
						if (replay->tickData[j].serverTick >= pauseStartTick)
						{
							pauseStartTickIndex = j;
							break;
						}
					}

					TickData *pauseStartTickData = &replay->tickData[pauseStartTickIndex];
					TickData *targetTickData = &replay->tickData[targetTick];
					totalPauseTime += (targetTickData->gameTime - pauseStartTickData->gameTime);
				}
				break;
			}

			switch (event->type)
			{
				case RPEVENT_TIMER_EVENT:
				{
					switch (event->data.timer.type)
					{
						case RpEvent::RpEventData::TimerEvent::TIMER_START:
						{
							const KZCourseDescriptor *courseDesc = KZ::course::GetCourseByCourseID(event->data.timer.index);
							if (!courseDesc || courseDesc->GetName().IsEmpty())
							{
								break;
							}
							V_snprintf(replay->courseName, sizeof(replay->courseName), "%s", courseDesc->GetName().Get());
							replay->paused = false;
							replay->endTime = 0.0f;
							replay->stopTick = 0;
							replay->lastSplitTime = 0.0f;
							replay->lastCPZTime = 0.0f;
							replay->lastStageTime = 0.0f;

							// Calculate start time relative to current server time
							replay->startTime = g_pKZUtils->GetServerGlobals()->curtime - event->data.timer.time;

							// When seeking, adjust start time by the time difference
							f32 eventGameTime = event->serverTick * ENGINE_FIXED_TICK_INTERVAL;
							f32 targetGameTime = replay->tickData[targetTick].gameTime;
							f32 timeDifference = targetGameTime - eventGameTime;
							replay->startTime -= timeDifference;

							// Pause calculation
							inPause = false;
							totalPauseTime = 0.0f;
							inActiveTimerRun = true;
							break;
						}
						case RpEvent::RpEventData::TimerEvent::TIMER_END:
						case RpEvent::RpEventData::TimerEvent::TIMER_STOP:
						{
							replay->endTime = event->data.timer.time;
							replay->stopTick = event->serverTick;
							replay->startTime = 0.0f;
							replay->courseName[0] = '\0';
							inActiveTimerRun = false;
							inPause = false;
							break;
						}
						case RpEvent::RpEventData::TimerEvent::TIMER_PAUSE:
						{
							replay->paused = true;
							if (inActiveTimerRun && !inPause)
							{
								inPause = true;
								pauseStartTick = event->serverTick;
							}
							break;
						}
						case RpEvent::RpEventData::TimerEvent::TIMER_RESUME:
						{
							replay->paused = false;
							if (inActiveTimerRun && inPause)
							{
								// Calculate pause duration and add to total
								u32 pauseStartTickIndex = 0;
								u32 resumeTickIndex = 0;

								for (u32 j = 0; j < replay->tickCount; j++)
								{
									if (replay->tickData[j].serverTick >= pauseStartTick)
									{
										pauseStartTickIndex = j;
										break;
									}
								}

								for (u32 j = 0; j < replay->tickCount; j++)
								{
									if (replay->tickData[j].serverTick >= event->serverTick)
									{
										resumeTickIndex = j;
										break;
									}
								}

								TickData *pauseStartTickData = &replay->tickData[pauseStartTickIndex];
								TickData *resumeTickData = &replay->tickData[resumeTickIndex];
								totalPauseTime += (resumeTickData->gameTime - pauseStartTickData->gameTime);
								inPause = false;
							}
							break;
						}
						case RpEvent::RpEventData::TimerEvent::TIMER_SPLIT:
						{
							replay->lastSplitTime = event->data.timer.time;
							break;
						}
						case RpEvent::RpEventData::TimerEvent::TIMER_CPZ:
						{
							replay->lastCPZTime = event->data.timer.time;
							break;
						}
						case RpEvent::RpEventData::TimerEvent::TIMER_STAGE:
						{
							replay->lastStageTime = event->data.timer.time;
							break;
						}
					}
					break;
				}
				case RPEVENT_MODE_CHANGE:
				{
					g_pKZModeManager->SwitchToMode(player, event->data.modeChange.name, true, false, false);
					break;
				}
				case RPEVENT_STYLE_CHANGE:
				{
					if (event->data.styleChange.clearStyles)
					{
						g_pKZStyleManager->ClearStyles(player, true, false);
					}
					g_pKZStyleManager->AddStyle(player, event->data.styleChange.name, true, false);
					break;
				}
				case RPEVENT_TELEPORT:
				{
					// This event is only for interrupting interpolation during playback
					// For seeking, we don't need to do anything here since teleport count
					// is tracked via checkpoint data in tickData
					break;
				}
			}

			// Update current event index if this event is at or before target server tick
			if (event->serverTick <= targetServerTick)
			{
				replay->currentEvent = i + 1;
			}
		}

		// Apply pause time adjustment to startTime if we have an active timer
		if (replay->startTime > 0.0f)
		{
			replay->startTime += totalPauseTime;
		}

		// Update weapon tracking - optimize for forward seeking
		if (isSeekingForward)
		{
			// Continue from current weapon position
			for (i32 i = startWeaponIndex; i < replay->numWeapons; i++)
			{
				if (replay->weapons[i + 1].serverTick <= targetServerTick)
				{
					replay->currentWeapon = i + 1;
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			// Full reprocess from beginning
			replay->currentWeapon = 0;
			for (i32 i = 0; i < replay->numWeapons; i++)
			{
				if (replay->weapons[i + 1].serverTick <= targetServerTick)
				{
					replay->currentWeapon = i + 1;
				}
				else
				{
					break;
				}
			}
		}

		// Update jump tracking - optimize for forward seeking
		if (isSeekingForward)
		{
			// Continue from current jump position
			for (u32 i = startJumpIndex; i < replay->numJumps; i++)
			{
				if (replay->jumps[i].overall.serverTick <= targetServerTick)
				{
					replay->currentJump = i + 1;
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			// Full reprocess from beginning
			replay->currentJump = 0;
			for (u32 i = 0; i < replay->numJumps; i++)
			{
				if (replay->jumps[i].overall.serverTick <= targetServerTick)
				{
					replay->currentJump = i + 1;
				}
				else
				{
					break;
				}
			}
		}

		// Update checkpoint state from target tick data
		TickData *targetTickData = &replay->tickData[targetTick];
		replay->currentCpIndex = targetTickData->checkpoint.index;
		replay->currentCheckpoint = targetTickData->checkpoint.checkpointCount;
		replay->currentTeleport = targetTickData->checkpoint.teleportCount;
	}

} // namespace KZ::replaysystem::events
