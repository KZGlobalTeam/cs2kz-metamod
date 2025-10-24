#include "cs2kz.h"
#include "kz_recording.h"
#include "kz/timer/kz_timer.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/replays/kz_replaysystem.h"

extern CConVar<bool> kz_replay_recording_debug;
extern CConVar<i32> kz_replay_recording_min_jump_tier;

// Timer event listener implementation
class TimerEventListener : public KZTimerServiceEventListener
{
public:
	virtual void OnTimerStartPost(KZPlayer *player, u32 courseGUID) override
	{
		player->recordingService->OnTimerStart();
	}

	virtual void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed) override
	{
		player->recordingService->OnTimerEnd();
	}

	virtual void OnTimerStopped(KZPlayer *player, u32 courseGUID) override
	{
		player->recordingService->OnTimerStop();
	}

	virtual void OnTimerInvalidated(KZPlayer *player) override
	{
		// player->PrintChat(false, false, "timerinvalidated");
	}

	virtual void OnPausePost(KZPlayer *player) override
	{
		player->recordingService->OnPause();
	}

	virtual void OnResumePost(KZPlayer *player) override
	{
		player->recordingService->OnResume();
	}

	virtual void OnSplitZoneTouchPost(KZPlayer *player, u32 splitZone) override
	{
		player->recordingService->OnSplit(splitZone);
	}

	virtual void OnCheckpointZoneTouchPost(KZPlayer *player, u32 checkpoint) override
	{
		player->recordingService->OnCPZ(checkpoint);
	}

	virtual void OnStageZoneTouchPost(KZPlayer *player, u32 stageZone) override
	{
		player->recordingService->OnStage(stageZone);
	}
} timerEventListener;

void KZRecordingService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
}

void KZRecordingService::OnProcessUsercmds(PlayerCommand *cmds, i32 numCmds)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (numCmds <= 0)
	{
		return;
	}

	// record data for replay playback
	if (this->player->IsFakeClient())
	{
		return;
	}

	this->RecordCommand(cmds, numCmds);
}

void KZRecordingService::OnTimerStart()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	this->runRecorders.push_back(RunRecorder(this->player));
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Timer start\n");
	}
	this->InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TIMER_START, this->player->timerService->GetTime(),
						   this->player->timerService->GetCourse()->id);
}

void KZRecordingService::OnTimerStop()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Timer stop\n");
	}
	this->InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TIMER_STOP, this->player->timerService->GetTime());

	// Remove all active run recorders, which are the ones without desired stop time set.
	auto it = this->runRecorders.begin();
	while (it != this->runRecorders.end())
	{
		if (it->desiredStopTime < 0.0f)
		{
			it = this->runRecorders.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void KZRecordingService::OnTimerEnd()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Timer end\n");
	}
	this->InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TIMER_END, this->player->timerService->GetTime(),
						   this->player->timerService->GetCourse()->id);

	for (auto &recorder : this->runRecorders)
	{
		if (recorder.desiredStopTime < 0.0f)
		{
			recorder.End(this->player->timerService->GetTime(), this->player->checkpointService->GetTeleportCount());
		}
	}
}

void KZRecordingService::OnPause()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Timer pause\n");
	}
	this->InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TIMER_PAUSE,
						   this->player->timerService->GetTimerRunning() ? this->player->timerService->GetTime() : 0.0f);
}

void KZRecordingService::OnResume()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Timer resume\n");
	}
	this->InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TIMER_RESUME,
						   this->player->timerService->GetTimerRunning() ? this->player->timerService->GetTime() : 0.0f);
}

void KZRecordingService::OnSplit(i32 split)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Timer split %d\n", split);
	}
	this->InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TIMER_SPLIT, this->player->timerService->GetTime(), split);
}

void KZRecordingService::OnCPZ(i32 cpz)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Checkpoint %d\n", cpz);
	}
	this->InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TIMER_CPZ, this->player->timerService->GetTime(), cpz);
}

void KZRecordingService::OnStage(i32 stage)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Stage %d\n", stage);
	}
	this->InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TIMER_STAGE, this->player->timerService->GetTime(), stage);
}

void KZRecordingService::OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Teleport\n");
	}
	this->InsertTeleportEvent(origin, angles, velocity);
}

void KZRecordingService::OnJumpFinish(Jump *jump)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	// If the player has a style, ignore it.
	if (this->player->styleServices.Count() > 0)
	{
		return;
	}
	// If a jump is way too short, ignore it.
	if (jump->airtime < 0.5f)
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: Jump finish\n");
	}
	RpJumpStats &rpJump = this->circularRecording.jumps->GetNextWriteRef();
	RpJumpStats::FromJump(rpJump, jump);
	// Only write the jump if it's ownage or better to save storage for run replays.
	if (jump->IsValid() && jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->jumpType, jump->GetDistance()) >= DistanceTier_Ownage)
	{
		this->PushToRecorders(rpJump, RecorderType::Run);
	}
	// Create a new jump recorder if the jump is good enough.
	if (jump->IsValid()
		&& jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->jumpType, jump->GetDistance()) >= kz_replay_recording_min_jump_tier.Get())
	{
		this->jumpRecorders.push_back(JumpRecorder(jump));
	}
	// Add to all active jump recorders.
	this->PushToRecorders(rpJump, RecorderType::Jump);
}

void KZRecordingService::OnClientDisconnect()
{
	for (auto &recorder : this->runRecorders)
	{
		if (recorder.desiredStopTime > 0.0f)
		{
			recorder.WriteToFile();
		}
	}
	this->runRecorders.clear();
	for (auto &recorder : this->jumpRecorders)
	{
		recorder.WriteToFile();
	}
	this->jumpRecorders.clear();
}

void KZRecordingService::OnPhysicsSimulate()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}

	// record data for replay playback
	if (!this->player->IsAlive() || this->player->IsFakeClient())
	{
		return;
	}

	this->RecordTickData_PhysicsSimulate();
}

void KZRecordingService::OnSetupMove(PlayerCommand *pc)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (!this->player->IsAlive() || this->player->IsFakeClient())
	{
		return;
	}
	this->RecordTickData_SetupMove(pc);
}

void KZRecordingService::OnPhysicsSimulatePost()
{
	if (this->player->IsFakeClient() || KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}

	this->CheckRecorders();

	// record data for replay playback
	if (!this->player->IsAlive())
	{
		return;
	}
	this->RecordTickData_PhysicsSimulatePost();
	this->CheckWeapons();
	this->CheckModeStyles();
	this->CheckCheckpoints();

	// Remove old events from circular buffer (keep 2 minutes)
	u32 currentTick = g_pKZUtils->GetServerGlobals()->tickcount;
	this->circularRecording.TrimOldData(currentTick);
}
