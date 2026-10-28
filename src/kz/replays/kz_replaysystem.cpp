#include "cs2kz.h"
#include "kz/kz.h"
#include "kz_replaysystem.h"
#include "bot.h"
#include "data.h"
#include "playback.h"
#include "events.h"
#include "commands.h"
#include "controls.h"
#include "kz_replay.h"
#include "utils/uuid.h"
#include "filesystem/ifilesystem.h"

#include <algorithm>

namespace KZ::replaysystem
{
	static_global std::vector<ReplayEventListener *> replayEventListeners;

	bool RegisterReplayEventListener(ReplayEventListener *listener)
	{
		if (!listener || std::find(replayEventListeners.begin(), replayEventListeners.end(), listener) != replayEventListeners.end())
		{
			return false;
		}
		replayEventListeners.push_back(listener);
		return true;
	}

	bool UnregisterReplayEventListener(ReplayEventListener *listener)
	{
		const auto found = std::find(replayEventListeners.begin(), replayEventListeners.end(), listener);
		if (found == replayEventListeners.end())
		{
			return false;
		}
		replayEventListeners.erase(found);
		return true;
	}

	void NotifyReplayFileChanged(const char *uuid)
	{
		if (!uuid || !UUID_t::FromString(uuid))
		{
			return;
		}
		// A listener may unsubscribe while handling the notification.
		const auto listeners = replayEventListeners;
		for (ReplayEventListener *listener : listeners)
		{
			if (std::find(replayEventListeners.begin(), replayEventListeners.end(), listener) != replayEventListeners.end())
			{
				listener->OnReplayFileChanged(uuid);
			}
		}
	}

	bool FindReplayPath(const char *uuid, std::string &path)
	{
		path.clear();
		UUID_t parsed(false);
		if (!uuid || !g_pFullFileSystem || !UUID_t::FromString(uuid, &parsed))
		{
			return false;
		}
		const std::string filename = parsed.ToString() + ".replay";
		for (const char *directory : {KZ_REPLAY_PATH, KZ_REPLAY_DOWNLOADS_PATH})
		{
			const std::string candidate = std::string(directory) + "/" + filename;
			if (g_pFullFileSystem->FileExists(candidate.c_str()))
			{
				path = candidate;
				return true;
			}
		}
		return false;
	}

	void Init()
	{
		item::InitItemAttributes();
		controls::Init();
	}

	void Cleanup()
	{
		bot::KickBot();
		CleanupWatcher();
		data::Shutdown();
	}

	void OnRoundStart()
	{
		bot::KickBot();
	}

	void OnGameFrame()
	{
		data::ProcessAsyncLoadCompletion();
	}

	bool BeginServerTick(KZPlayer *player)
	{
		return playback::BeginServerTick(player);
	}

	void BeginReplayTick(KZPlayer *player)
	{
		playback::BeginReplayTick(player);
	}

	bool HasQueuedReplayTicks(KZPlayer *player)
	{
		return playback::HasQueuedReplayTicks(player);
	}

	bool IsHoldingTick(KZPlayer *player)
	{
		return playback::IsHoldingTick(player);
	}

	void EndServerTick(KZPlayer *player)
	{
		playback::EndServerTick(player);
	}

	void OnPhysicsSimulate(KZPlayer *player)
	{
		playback::OnPhysicsSimulate(player);
	}

	void OnProcessMovement(KZPlayer *player)
	{
		playback::OnProcessMovement(player);
	}

	void OnProcessMovementPost(KZPlayer *player)
	{
		playback::OnProcessMovementPost(player);
	}

	void OnFinishMovePre(KZPlayer *player, CMoveData *pMoveData)
	{
		playback::OnFinishMovePre(player, pMoveData);
	}

	void OnPhysicsSimulatePost(KZPlayer *player)
	{
		playback::OnPhysicsSimulatePost(player);
	}

	bool OnPlayerRunCommandPre(KZPlayer *player, CUserCmd *command)
	{
		return playback::OnPlayerRunCommandPre(player, command);
	}

	bool IsReplayBot(KZPlayer *player)
	{
		return bot::IsValidBot(player ? player->GetController() : nullptr);
	}

	bool CanTouchTrigger(KZPlayer *player, CBaseTrigger *trigger)
	{
		// Don't care about non-bot players.
		if (!bot::IsValidBot(player->GetController()))
		{
			return true;
		}

		// Don't care about non timer triggers.
		const KzTrigger *kzTrigger = KZ::mapapi::GetKzTrigger(trigger);
		if (!kzTrigger)
		{
			return true;
		}

		if (KZ::mapapi::IsTimerTrigger(kzTrigger->type) || KZ::mapapi::IsTeleportTrigger(kzTrigger->type))
		{
			return false;
		}

		return true;
	}

	void UpdateControls(KZPlayer *player)
	{
		if (!player->IsFakeClient())
		{
			player->uiService->GetWindow<controls::ControlsWindow>()->Update();
		}
	}

	void OnProcessUsercmds(KZPlayer *player, CUserCmd *cmds, i32 numCmds)
	{
		if (!player->IsFakeClient())
		{
			player->uiService->GetWindow<controls::ControlsWindow>()->OnProcessUsercmds(cmds, numCmds);
		}
	}

	RunStatus GetRunStatus()
	{
		RunStatus out {};
		auto replay = data::GetCurrentReplay();
		const ReplayType type = static_cast<ReplayType>(replay->header.type());
		if (!replay->playingReplay || type == ReplayType::RP_JUMPSTATS)
		{
			return out;
		}
		out.timed = true;
		for (u32 i = 0; i < replay->currentEvent && i < replay->numEvents; i++)
		{
			const RpEvent &event = replay->events[i];
			if (event.type != RPEVENT_TIMER_EVENT)
			{
				continue;
			}
			switch (event.data.timer.type)
			{
				case RpEvent::RpEventData::TimerEvent::TIMER_START:
				{
					out.course = KZ::course::GetCourseByCourseID(event.data.timer.index);
					out.running = out.course != nullptr;
					out.stages = out.checkpoints = out.splits = 0;
					break;
				}
				case RpEvent::RpEventData::TimerEvent::TIMER_END:
				case RpEvent::RpEventData::TimerEvent::TIMER_STOP:
				{
					out.running = false;
					break;
				}
				// The recorder only writes a zone the first time the run reaches it.
				case RpEvent::RpEventData::TimerEvent::TIMER_SPLIT:
				{
					out.splits++;
					break;
				}
				case RpEvent::RpEventData::TimerEvent::TIMER_CPZ:
				{
					out.checkpoints++;
					break;
				}
				case RpEvent::RpEventData::TimerEvent::TIMER_STAGE:
				{
					out.stages++;
					break;
				}
				default:
				{
					break;
				}
			}
		}
		// Like a live timer, a run that is not going has reached nothing.
		if (!out.running)
		{
			out.stages = out.checkpoints = out.splits = 0;
		}
		if (type == ReplayType::RP_RUN && replay->header.has_run() && out.course
			&& KZ_STREQI(replay->header.run().course_name().c_str(), out.course->GetName().c_str()))
		{
			out.runTime = replay->header.run().time();
			out.runPro = replay->header.run().num_teleports() == 0;
		}
		return out;
	}

	const char *GetPlayerName()
	{
		return data::GetCurrentReplay()->header.player().name().c_str();
	}

	i32 GetCurrentCpIndex()
	{
		return data::GetCurrentCpIndex();
	}

	i32 GetCheckpointCount()
	{
		return data::GetCheckpointCount();
	}

	i32 GetTeleportCount()
	{
		return data::GetTeleportCount();
	}

	f32 GetTime()
	{
		return data::GetReplayTime();
	}

	f32 GetEndTime()
	{
		return data::GetEndTime();
	}

	bool GetPaused()
	{
		return data::GetPaused();
	}

	const char *GetCourseName()
	{
		return data::GetCourseName();
	}

} // namespace KZ::replaysystem
