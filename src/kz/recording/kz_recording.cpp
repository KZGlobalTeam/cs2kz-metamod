
#include "kz_recording.h"
#include "kz/timer/kz_timer.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/replays/kz_replaysystem.h"
#include "utils/simplecmds.h"

#include "sdk/cskeletoninstance.h"
#include "sdk/usercmd.h"

#include "filesystem.h"
#include "vprof.h"

static_global class : public KZTimerServiceEventListener
{
public:
	virtual void OnTimerStartPost(KZPlayer *player, u32 courseGUID)
	{
		player->recordingService->OnTimerStart();
	}

	virtual void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed)
	{
		player->recordingService->OnTimerEnd();
	}

	virtual void OnTimerStopped(KZPlayer *player, u32 courseGUID)
	{
		player->recordingService->OnTimerStop();
	}

	virtual void OnTimerInvalidated(KZPlayer *player)
	{
		// player->PrintChat(false, false, "timerinvalidated");
	}

	virtual void OnPausePost(KZPlayer *player)
	{
		player->recordingService->OnPause();
	}

	virtual void OnResumePost(KZPlayer *player)
	{
		player->recordingService->OnResume();
	}

	virtual void OnSplitZoneTouchPost(KZPlayer *player, u32 splitZone)
	{
		player->recordingService->OnSplit(splitZone);
	}

	virtual void OnCheckpointZoneTouchPost(KZPlayer *player, u32 checkpoint)
	{
		player->recordingService->OnCPZ(checkpoint);
	}

	virtual void OnStageZoneTouchPost(KZPlayer *player, u32 stageZone)
	{
		player->recordingService->OnStage(stageZone);
	}
} timerEventListener;

CConVar<bool> kz_replay_recording_debug("kz_replay_recording_debug", FCVAR_NONE, "Debug replay recording", true);
CConVar<i32> kz_replay_recording_min_jump_tier("kz_replay_recording_min_jump_tier", FCVAR_CHEAT, "Minimum jump tier to record", DistanceTier_Wrecker,
											   true, DistanceTier_Meh, true, DistanceTier_Wrecker);

static_function f32 StringToFloat(const char *str)
{
	if (!str || str[0] == '\0')
	{
		return 0.0f;
	}
	if (KZ_STREQI(str, "inf"))
	{
		return FLT_MAX;
	}
	if (KZ_STREQI(str, "-inf"))
	{
		return -FLT_MAX;
	}
	if (KZ_STREQI(str, "nan"))
	{
		return 0.0f;
	}
	return static_cast<f32>(atof(str));
}

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
	i32 currentTick = g_pKZUtils->GetServerGlobals()->tickcount;

	i32 numToRemove = 0;
	for (i32 i = 0; i < this->circularRecording.cmdData->GetReadAvailable(); i++)
	{
		// Remove old commands from circular buffer (keep 2 minutes)
		CmdData data;
		if (!this->circularRecording.cmdData->Peek(&data, 1, i))
		{
			break;
		}
		if (data.serverTick + 2 * 60 * 64 < currentTick)
		{
			numToRemove++;
		}
		else
		{
			break;
		}
	}
	this->circularRecording.cmdData->Advance(numToRemove);
	this->circularRecording.cmdSubtickData->Advance(numToRemove);
	// record data for replay playback
	if (this->player->IsFakeClient())
	{
		return;
	}
	for (i32 i = 0; i < numCmds; i++)
	{
		auto &pc = cmds[i];

		if (pc.cmdNum <= this->lastCmdNumReceived)
		{
			continue;
		}
		CmdData data;
		data.serverTick = currentTick;
		data.gameTime = g_pKZUtils->GetServerGlobals()->curtime;
		data.realTime = g_pKZUtils->GetServerGlobals()->realtime;
		time_t unixTime = 0;
		time(&unixTime);
		data.unixTime = (u64)unixTime;
		INetChannelInfo *netchan = interfaces::pEngine->GetPlayerNetInfo(this->player->GetPlayerSlot());
		netchan->GetRemoteFramerate(&data.framerate, nullptr, nullptr);
		data.latency = netchan->GetEngineLatency();
		data.avgLoss = netchan->GetAvgLoss(FLOW_INCOMING) + netchan->GetAvgChoke(FLOW_INCOMING);
		data.cmdNumber = pc.cmdNum;
		data.clientTick = pc.base().client_tick();
		data.forward = pc.base().forwardmove();
		data.left = pc.base().leftmove();
		data.up = pc.base().upmove();
		data.buttons[0] = pc.base().buttons_pb().buttonstate1();
		data.buttons[1] = pc.base().buttons_pb().buttonstate2();
		data.buttons[2] = pc.base().buttons_pb().buttonstate3();
		data.angles = {pc.base().viewangles().x(), pc.base().viewangles().y(), pc.base().viewangles().z()};
		data.mousedx = pc.base().mousedx();
		data.mousedy = pc.base().mousedy();
		this->circularRecording.cmdData->Write(data);
		for (auto &recorder : this->runRecorders)
		{
			recorder.cmdData.push_back(data);
		}
		for (auto &recorder : this->jumpRecorders)
		{
			recorder.cmdData.push_back(data);
		}
		SubtickData subtickData;
		subtickData.numSubtickMoves = pc.base().subtick_moves_size();
		for (u32 j = 0; j < subtickData.numSubtickMoves && j < 64; j++)
		{
			auto &move = pc.base().subtick_moves(j);
			subtickData.subtickMoves[j].when = move.when();
			subtickData.subtickMoves[j].button = move.button();
			if (move.button())
			{
				subtickData.subtickMoves[j].pressed = move.pressed();
			}
			else
			{
				subtickData.subtickMoves[j].analogMove.analog_forward_delta = move.analog_forward_delta();
				subtickData.subtickMoves[j].analogMove.analog_left_delta = move.analog_left_delta();
				subtickData.subtickMoves[j].analogMove.pitch_delta = move.pitch_delta();
				subtickData.subtickMoves[j].analogMove.yaw_delta = move.yaw_delta();
			}
		}
		this->circularRecording.cmdSubtickData->Write(subtickData);
		for (auto &recorder : this->runRecorders)
		{
			recorder.cmdSubtickData.push_back(subtickData);
		}
		for (auto &recorder : this->jumpRecorders)
		{
			recorder.cmdSubtickData.push_back(subtickData);
		}
		this->lastCmdNumReceived = pc.cmdNum;
	}
}

void KZRecordingService::OnPlayerJoinTeam(i32 team) {}

void KZRecordingService::Reset()
{
	this->circularRecording.tickData->Advance(this->circularRecording.tickData->GetReadAvailable());
	this->circularRecording.subtickData->Advance(this->circularRecording.subtickData->GetReadAvailable());
	this->circularRecording.cmdData->Advance(this->circularRecording.cmdData->GetReadAvailable());
	this->circularRecording.cmdSubtickData->Advance(this->circularRecording.cmdSubtickData->GetReadAvailable());
	this->circularRecording.weaponChangeEvents->Advance(this->circularRecording.weaponChangeEvents->GetReadAvailable());
	this->circularRecording.rpEvents->Advance(this->circularRecording.rpEvents->GetReadAvailable());
	this->runRecorders.clear();
	this->lastCmdNumReceived = 0;
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
		this->player->PrintChat(false, false, "Timer start");
	}
	// TODO: Initiate a new recorder of type RP_RUN
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = RpEvent::RpEventData::TimerEvent::TIMER_START;
	// Mapper-defined course ID.
	event.data.timer.index = this->player->timerService->GetCourse()->id;
	event.data.timer.time = this->player->timerService->GetTime();
	this->InsertEvent(event);
}

void KZRecordingService::OnTimerStop()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		this->player->PrintChat(false, false, "Timer stop");
	}
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = RpEvent::RpEventData::TimerEvent::TIMER_STOP;
	event.data.timer.time = this->player->timerService->GetTime();
	this->InsertEvent(event);

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
		this->player->PrintChat(false, false, "Timer end");
	}
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = RpEvent::RpEventData::TimerEvent::TIMER_END;
	event.data.timer.index = this->player->timerService->GetCourse()->id;
	event.data.timer.time = this->player->timerService->GetTime();
	this->InsertEvent(event);

	for (auto &recorder : this->runRecorders)
	{
		if (recorder.desiredStopTime < 0.0f)
		{
			recorder.desiredStopTime = g_pKZUtils->GetServerGlobals()->curtime + 2.0f; // record 2 seconds after timer end
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
		this->player->PrintChat(false, false, "Timer pause");
	}
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = RpEvent::RpEventData::TimerEvent::TIMER_PAUSE;
	event.data.timer.time = this->player->timerService->GetTimerRunning() ? this->player->timerService->GetTime() : 0.0f;
	this->InsertEvent(event);
}

void KZRecordingService::OnResume()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		this->player->PrintChat(false, false, "Timer resume");
	}
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = RpEvent::RpEventData::TimerEvent::TIMER_RESUME;
	event.data.timer.time = this->player->timerService->GetTimerRunning() ? this->player->timerService->GetTime() : 0.0f;
	this->InsertEvent(event);
}

void KZRecordingService::OnSplit(i32 split)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		this->player->PrintChat(false, false, "Timer split %d", split);
	}
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = RpEvent::RpEventData::TimerEvent::TIMER_SPLIT;
	event.data.timer.index = split;
	event.data.timer.time = this->player->timerService->GetTime();
	this->InsertEvent(event);
}

void KZRecordingService::OnCPZ(i32 cpz)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		this->player->PrintChat(false, false, "Checkpoi32 %d", cpz);
	}
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = RpEvent::RpEventData::TimerEvent::TIMER_CPZ;
	event.data.timer.index = cpz;
	event.data.timer.time = this->player->timerService->GetTime();
	this->InsertEvent(event);
}

void KZRecordingService::OnStage(i32 stage)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		this->player->PrintChat(false, false, "Stage %d", stage);
	}
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = RpEvent::RpEventData::TimerEvent::TIMER_STAGE;
	event.data.timer.index = stage;
	event.data.timer.time = this->player->timerService->GetTime();
	this->InsertEvent(event);
}

void KZRecordingService::OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	if (kz_replay_recording_debug.Get())
	{
		this->player->PrintChat(false, false, "Teleport");
	}
	RpEvent event;
	event.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	event.type = RpEventType::RPEVENT_TELEPORT;
	event.data.teleport.hasOrigin = origin != nullptr;
	event.data.teleport.hasAngles = angles != nullptr;
	event.data.teleport.hasVelocity = velocity != nullptr;
	if (origin)
	{
		for (i32 i = 0; i < 3; i++)
		{
			event.data.teleport.origin[i] = origin->operator[](i);
		}
	}
	if (angles)
	{
		for (i32 i = 0; i < 3; i++)
		{
			event.data.teleport.angles[i] = angles->operator[](i);
		}
	}
	if (velocity)
	{
		for (i32 i = 0; i < 3; i++)
		{
			event.data.teleport.velocity[i] = velocity->operator[](i);
		}
	}
	this->InsertEvent(event);
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
		this->player->PrintChat(false, false, "Jump finish");
	}
	RpJumpStats &rpJump = this->circularRecording.jumps->GetNextWriteRef();
	RpJumpStats::FromJump(rpJump, jump);
	// Only write the jump if it's ownage or better to save storage for run replays.
	if (jump->IsValid() && jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->jumpType, jump->GetDistance()) >= DistanceTier_Ownage)
	{
		for (auto &recorder : this->runRecorders)
		{
			recorder.jumps.push_back(rpJump);
			break;
		}
	}
	// Create a new jump recorder if the jump is good enough.
	if (jump->IsValid()
		&& jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->jumpType, jump->GetDistance()) >= kz_replay_recording_min_jump_tier.Get())
	{
		this->jumpRecorders.push_back(JumpRecorder(jump));
	}
	// Add to all active jump recorders.
	for (auto &recorder : this->jumpRecorders)
	{
		recorder.jumps.push_back(rpJump);
	}
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

void KZRecordingService::InsertEvent(const RpEvent &event)
{
	// 1. Insert into circular buffer
	// 2. Insert into all active recorders
	this->circularRecording.rpEvents->Write(event);
	for (auto &recorder : this->runRecorders)
	{
		recorder.rpEvents.push_back(event);
	}
	if (event.type != RpEventType::RPEVENT_TIMER_EVENT)
	{
		for (auto &recorder : this->jumpRecorders)
		{
			recorder.rpEvents.push_back(event);
		}
	}
}

bool KZRecordingService::GetCurrentRunUUID(UUID_t &out_uuid)
{
	// If desiredStopTime is set, the run has ended.
	if (!this->runRecorders.empty() && this->runRecorders.back().desiredStopTime < 0.0f)
	{
		out_uuid = this->runRecorders.back().uuid;
		return true;
	}
	return false;
}

void KZRecordingService::OnPhysicsSimulate()
{
	if (KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}
	CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(this->player->GetPlayerPawn()->m_CBodyComponent()->m_pSceneNode());

	// record data for replay playback
	if (!this->player->IsAlive() || this->player->IsFakeClient())
	{
		return;
	}

	this->currentTickData = {};
	this->currentTickData.serverTick = g_pKZUtils->GetServerGlobals()->tickcount;
	this->currentTickData.gameTime = g_pKZUtils->GetServerGlobals()->curtime;
	this->currentTickData.realTime = g_pKZUtils->GetServerGlobals()->realtime;
	time_t unixTime = 0;
	time(&unixTime);
	this->currentTickData.unixTime = (u64)unixTime;

	this->player->GetOrigin(&this->currentTickData.pre.origin);
	this->player->GetVelocity(&this->currentTickData.pre.velocity);
	this->player->GetAngles(&this->currentTickData.pre.angles);
	auto movementServices = this->player->GetMoveServices();
	this->currentTickData.pre.buttons[0] = static_cast<u32>(movementServices->m_nButtons()->m_pButtonStates[0]);
	this->currentTickData.pre.buttons[1] = static_cast<u32>(movementServices->m_nButtons()->m_pButtonStates[1]);
	this->currentTickData.pre.buttons[2] = static_cast<u32>(movementServices->m_nButtons()->m_pButtonStates[2]);
	this->currentTickData.pre.jumpPressedTime = movementServices->m_flJumpPressedTime;
	this->currentTickData.pre.duckSpeed = movementServices->m_flDuckSpeed;
	this->currentTickData.pre.duckAmount = movementServices->m_flDuckAmount;
	this->currentTickData.pre.duckOffset = movementServices->m_flDuckOffset;
	this->currentTickData.pre.lastDuckTime = movementServices->m_flLastDuckTime;
	this->currentTickData.pre.replayFlags.ducking = movementServices->m_bDucking;
	this->currentTickData.pre.replayFlags.ducked = movementServices->m_bDucked;
	this->currentTickData.pre.replayFlags.desiresDuck = movementServices->m_bDesiresDuck;

	this->currentTickData.pre.entityFlags = this->player->GetPlayerPawn()->m_fFlags();
	this->currentTickData.pre.moveType = this->player->GetPlayerPawn()->m_nActualMoveType;
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

	this->currentTickData.cmdNumber = pc->cmdNum;
	this->currentTickData.clientTick = pc->base().client_tick();
	this->currentTickData.forward = pc->base().forwardmove();
	this->currentTickData.left = pc->base().leftmove();
	this->currentTickData.up = pc->base().upmove();
	this->currentTickData.pre.angles = {pc->base().viewangles().x(), pc->base().viewangles().y(), pc->base().viewangles().z()};
	this->currentSubtickData.numSubtickMoves = pc->base().subtick_moves_size();
	this->currentTickData.leftHanded = this->player->GetPlayerPawn()->m_bLeftHanded() || pc->left_hand_desired();
	for (u32 i = 0; i < this->currentSubtickData.numSubtickMoves && i < 64; i++)
	{
		auto &move = pc->base().subtick_moves(i);
		this->currentSubtickData.subtickMoves[i].when = move.when();
		this->currentSubtickData.subtickMoves[i].button = move.button();
		if (move.button())
		{
			this->currentSubtickData.subtickMoves[i].pressed = move.pressed();
		}
		else
		{
			this->currentSubtickData.subtickMoves[i].analogMove.analog_forward_delta = move.analog_forward_delta();
			this->currentSubtickData.subtickMoves[i].analogMove.analog_left_delta = move.analog_left_delta();
			this->currentSubtickData.subtickMoves[i].analogMove.pitch_delta = move.pitch_delta();
			this->currentSubtickData.subtickMoves[i].analogMove.yaw_delta = move.yaw_delta();
		}
	}
}

void KZRecordingService::OnPhysicsSimulatePost()
{
	if (this->player->IsFakeClient() || KZ::replaysystem::IsReplayBot(this->player))
	{
		return;
	}

	// Go through all the active recorders and see if any need to be stopped.
	for (auto it = this->runRecorders.begin(); it != this->runRecorders.end();)
	{
		auto &recorder = *it;
		if (recorder.desiredStopTime >= 0.0f && g_pKZUtils->GetServerGlobals()->curtime >= recorder.desiredStopTime)
		{
			// Stop this recorder
			if (kz_replay_recording_debug.Get())
			{
				this->player->PrintChat(false, false, "Run recorder stopped");
			}
			recorder.WriteToFile();
			it = this->runRecorders.erase(it);
		}
		else
		{
			++it;
		}
	}
	for (auto it = this->jumpRecorders.begin(); it != this->jumpRecorders.end();)
	{
		auto &recorder = *it;
		if (recorder.desiredStopTime >= 0.0f && g_pKZUtils->GetServerGlobals()->curtime >= recorder.desiredStopTime)
		{
			// Stop this recorder
			if (kz_replay_recording_debug.Get())
			{
				this->player->PrintChat(false, false, "Run recorder stopped");
			}
			recorder.WriteToFile();
			it = this->jumpRecorders.erase(it);
		}
		else
		{
			++it;
		}
	}
	// record data for replay playback
	if (!this->player->IsAlive())
	{
		return;
	}
	this->player->GetOrigin(&this->currentTickData.post.origin);
	this->player->GetVelocity(&this->currentTickData.post.velocity);
	this->player->GetAngles(&this->currentTickData.post.angles);
	auto movementServices = this->player->GetMoveServices();
	this->currentTickData.post.buttons[0] = static_cast<u32>(movementServices->m_nButtons()->m_pButtonStates[0]);
	this->currentTickData.post.buttons[1] = static_cast<u32>(movementServices->m_nButtons()->m_pButtonStates[1]);
	this->currentTickData.post.buttons[2] = static_cast<u32>(movementServices->m_nButtons()->m_pButtonStates[2]);
	this->currentTickData.post.jumpPressedTime = movementServices->m_flJumpPressedTime;
	this->currentTickData.post.duckSpeed = movementServices->m_flDuckSpeed;
	this->currentTickData.post.duckAmount = movementServices->m_flDuckAmount;
	this->currentTickData.post.duckOffset = movementServices->m_flDuckOffset;
	this->currentTickData.post.lastDuckTime = movementServices->m_flLastDuckTime;
	this->currentTickData.post.replayFlags.ducking = movementServices->m_bDucking;
	this->currentTickData.post.replayFlags.ducked = movementServices->m_bDucked;
	this->currentTickData.post.replayFlags.desiresDuck = movementServices->m_bDesiresDuck;

	this->currentTickData.post.entityFlags = this->player->GetPlayerPawn()->m_fFlags();
	this->currentTickData.post.moveType = this->player->GetPlayerPawn()->m_nActualMoveType;
	this->circularRecording.tickData->Write(this->currentTickData);
	for (auto &recorder : this->runRecorders)
	{
		recorder.tickData.push_back(this->currentTickData);
	}
	for (auto &recorder : this->jumpRecorders)
	{
		recorder.tickData.push_back(this->currentTickData);
	}
	this->circularRecording.subtickData->Write(this->currentSubtickData);
	for (auto &recorder : this->runRecorders)
	{
		recorder.subtickData.push_back(this->currentSubtickData);
	}
	for (auto &recorder : this->jumpRecorders)
	{
		recorder.subtickData.push_back(this->currentSubtickData);
	}

	auto weapon = this->player->GetPlayerPawn()->m_pWeaponServices()->m_hActiveWeapon().Get();
	auto weaponEconInfo = EconInfo(weapon);
	if (this->currentWeaponEconInfo != weaponEconInfo)
	{
		this->currentWeaponEconInfo = weaponEconInfo;
		WeaponSwitchEvent event;
		event.serverTick = this->currentTickData.serverTick;
		event.econInfo = weaponEconInfo;
		this->circularRecording.weaponChangeEvents->Write(event);
		for (auto &recorder : this->runRecorders)
		{
			recorder.weaponChangeEvents.push_back(event);
		}
		for (auto &recorder : this->jumpRecorders)
		{
			recorder.weaponChangeEvents.push_back(event);
		}
	}
	if (this->player->checkpointService->GetCheckpointCount() != this->lastKnownCheckpointCount
		|| this->player->checkpointService->GetTeleportCount() != this->lastKnownTeleportCount
		|| this->player->checkpointService->GetCurrentCpIndex() != this->lastKnownCheckpointIndex)
	{
		this->lastKnownCheckpointCount = this->player->checkpointService->GetCheckpointCount();
		this->lastKnownTeleportCount = this->player->checkpointService->GetTeleportCount();
		this->lastKnownCheckpointIndex = this->player->checkpointService->GetCurrentCpIndex();
		RpEvent event;
		event.serverTick = this->currentTickData.serverTick;
		event.type = RpEventType::RPEVENT_CHECKPOINT;
		event.data.checkpoint.index = this->lastKnownCheckpointIndex;
		event.data.checkpoint.checkpointCount = this->lastKnownCheckpointCount;
		event.data.checkpoint.teleportCount = this->lastKnownTeleportCount;
		this->InsertEvent(event);
		if (kz_replay_recording_debug.Get())
		{
			this->player->PrintChat(false, false, "Checkpoint event: #%d CP %d, teleports %d", this->lastKnownCheckpointIndex,
									this->lastKnownCheckpointCount, this->lastKnownTeleportCount);
		}
	}
	auto currentModeInfo = KZ::mode::GetModeInfo(this->player->modeService);
	if (this->lastKnownMode.shortModeName != currentModeInfo.shortModeName || !KZ_STREQI(this->lastKnownMode.md5, currentModeInfo.md5))
	{
		this->lastKnownMode = currentModeInfo;
		RpEvent event;
		event.serverTick = this->currentTickData.serverTick;
		event.type = RpEventType::RPEVENT_MODE_CHANGE;
		V_strncpy(event.data.modeChange.name, currentModeInfo.longModeName.Get(), sizeof(event.data.modeChange.name));
		V_strncpy(event.data.modeChange.md5, currentModeInfo.md5, sizeof(event.data.modeChange.md5));
		this->InsertEvent(event);
		if (kz_replay_recording_debug.Get())
		{
			this->player->PrintChat(false, false, "Mode change event: %s", currentModeInfo.longModeName.Get());
		}
	}
	bool refreshStyles = this->player->styleServices.Count() != this->lastKnownStyles.size();
	if (!refreshStyles)
	{
		for (i32 i = 0; i < this->player->styleServices.Count(); i++)
		{
			if (!KZ_STREQI(this->lastKnownStyles[i].shortName, KZ::style::GetStyleInfo(this->player->styleServices[i]).shortName)
				|| !KZ_STREQI(this->lastKnownStyles[i].md5, KZ::style::GetStyleInfo(this->player->styleServices[i]).md5))
			{
				refreshStyles = true;
				break;
			}
		}
	}
	if (refreshStyles)
	{
		this->lastKnownStyles.clear();
		FOR_EACH_VEC(this->player->styleServices, i)
		{
			auto styleService = this->player->styleServices[i];
			auto styleInfo = KZ::style::GetStyleInfo(styleService);
			this->lastKnownStyles.push_back(styleInfo);
		}

		// We just send all styles again.
		for (auto &style : this->lastKnownStyles)
		{
			RpEvent event;
			event.serverTick = this->currentTickData.serverTick;
			event.type = RpEventType::RPEVENT_STYLE_CHANGE;
			V_strncpy(event.data.styleChange.name, style.shortName, sizeof(event.data.styleChange.name));
			V_strncpy(event.data.styleChange.md5, style.md5, sizeof(event.data.styleChange.md5));
			event.data.styleChange.clearStyles = refreshStyles;
			refreshStyles = false; // only the first styleChange needs to have clearStyles = true
			this->InsertEvent(event);
		}
		if (kz_replay_recording_debug.Get())
		{
			this->player->PrintChat(false, false, "Style change event: %u styles", this->lastKnownStyles.size());
		}
	}

	const char *sensitivityStr = interfaces::pEngine->GetClientConVarValue(this->player->GetPlayerSlot(), "sensitivity");
	f32 sensitivity = StringToFloat(sensitivityStr);
	if (this->lastSensitivity != sensitivity)
	{
		this->lastSensitivity = sensitivity;
		RpEvent event;
		event.serverTick = this->currentTickData.serverTick;
		event.type = RpEventType::RPEVENT_CVAR;
		event.data.cvar.cvar = RPCVAR_SENSITIVITY;
		this->InsertEvent(event);
		if (kz_replay_recording_debug.Get())
		{
			this->player->PrintChat(false, false, "Sensitivity change event: %f", sensitivity);
		}
	}

	const char *pitchStr = interfaces::pEngine->GetClientConVarValue(this->player->GetPlayerSlot(), "m_pitch");
	f32 pitch = StringToFloat(pitchStr);
	if (this->lastPitch != pitch)
	{
		this->lastPitch = pitch;
		RpEvent event;
		event.serverTick = this->currentTickData.serverTick;
		event.type = RpEventType::RPEVENT_CVAR;
		event.data.cvar.cvar = RPCVAR_M_PITCH;
		this->InsertEvent(event);
		if (kz_replay_recording_debug.Get())
		{
			this->player->PrintChat(false, false, "Pitch change event: %f", pitch);
		}
	}

	const char *yawStr = interfaces::pEngine->GetClientConVarValue(this->player->GetPlayerSlot(), "m_yaw");
	f32 yaw = StringToFloat(yawStr);
	if (this->lastYaw != yaw)
	{
		this->lastYaw = yaw;
		RpEvent event;
		event.serverTick = this->currentTickData.serverTick;
		event.type = RpEventType::RPEVENT_CVAR;
		event.data.cvar.cvar = RPCVAR_M_YAW;
		this->InsertEvent(event);
		if (kz_replay_recording_debug.Get())
		{
			this->player->PrintChat(false, false, "Yaw change event: %f", yaw);
		}
	}

	if (!this->circularRecording.earliestWeapon.has_value())
	{
		this->circularRecording.earliestWeapon = this->currentWeaponEconInfo;
	}
	if (!this->circularRecording.earliestMode.has_value())
	{
		auto modeInfo = KZ::mode::GetModeInfo(this->player->modeService);
		this->circularRecording.earliestMode = RpModeStyleInfo();
		V_strncpy(this->circularRecording.earliestMode->name, modeInfo.longModeName.Get(), sizeof(this->circularRecording.earliestMode->name));
		V_strncpy(this->circularRecording.earliestMode->md5, modeInfo.md5, sizeof(this->circularRecording.earliestMode->md5));
	}
	if (!this->circularRecording.earliestStyles.has_value())
	{
		this->circularRecording.earliestStyles = std::vector<RpModeStyleInfo>();

		FOR_EACH_VEC(this->player->styleServices, i)
		{
			auto styleService = this->player->styleServices[i];
			auto styleInfo = KZ::style::GetStyleInfo(styleService);
			RpModeStyleInfo style = {};
			V_strncpy(style.name, styleInfo.longName, sizeof(style.name));
			V_strncpy(style.md5, styleInfo.md5, sizeof(style.md5));
			this->circularRecording.earliestStyles->push_back(style);
		}
	}
	if (!this->circularRecording.earliestCheckpointCount.has_value())
	{
		this->circularRecording.earliestCheckpointCount = this->lastKnownCheckpointCount;
		this->circularRecording.earliestTeleportCount = this->lastKnownTeleportCount;
		this->circularRecording.earliestCheckpointIndex = this->lastKnownCheckpointIndex;
	}
	// Remove old events from circular buffer (keep 2 minutes)
	u32 currentTick = g_pKZUtils->GetServerGlobals()->tickcount;
	i32 numToRemove = 0;
	for (i32 i = 0; i < this->circularRecording.weaponChangeEvents->GetReadAvailable(); i++)
	{
		WeaponSwitchEvent data;
		if (!this->circularRecording.weaponChangeEvents->Peek(&data, 1, i))
		{
			break;
		}
		if (data.serverTick + 2 * 60 * 64 < currentTick)
		{
			this->circularRecording.earliestWeapon = data.econInfo;
			numToRemove++;
			continue;
		}
		break;
	}
	this->circularRecording.weaponChangeEvents->Advance(numToRemove);

	// Remove old mode and style changes from circular buffer.
	numToRemove = 0;
	for (i32 i = 0; i < this->circularRecording.rpEvents->GetReadAvailable(); i++)
	{
		RpEvent event;
		if (!this->circularRecording.rpEvents->Peek(&event, 1, i))
		{
			break;
		}
		if (event.serverTick + 2 * 60 * 64 < currentTick)
		{
			switch (event.type)
			{
				case RPEVENT_MODE_CHANGE:
				{
					V_strncpy(this->circularRecording.earliestMode.value().name, event.data.modeChange.name,
							  sizeof(this->circularRecording.earliestMode.value().name));
					V_strncpy(this->circularRecording.earliestMode.value().md5, event.data.modeChange.md5,
							  sizeof(this->circularRecording.earliestMode.value().md5));
					break;
				}
				case RPEVENT_STYLE_CHANGE:
				{
					if (event.data.styleChange.clearStyles)
					{
						this->circularRecording.earliestStyles.value().clear();
					}
					RpModeStyleInfo style = {};
					V_strncpy(style.name, event.data.styleChange.name, sizeof(style.name));
					V_strncpy(style.md5, event.data.styleChange.md5, sizeof(style.md5));
					this->circularRecording.earliestStyles.value().push_back(style);
				}
				case RPEVENT_CHECKPOINT:
				{
					this->circularRecording.earliestCheckpointIndex = event.data.checkpoint.index;
					this->circularRecording.earliestCheckpointCount = event.data.checkpoint.checkpointCount;
					this->circularRecording.earliestTeleportCount = event.data.checkpoint.teleportCount;
					break;
				}
			}
			// We don't care about timer events, we just do nothing during playback if they become "orphaned" from having its timer start event
			// removed. In a run replay, the relevant timer events will always have a timer start event to refer to.
			numToRemove++;
			continue;
		}
		break;
	}
	this->circularRecording.rpEvents->Advance(numToRemove);

	// Remove old jumps from circular buffer
	numToRemove = 0;
	for (i32 i = 0; i < this->circularRecording.jumps->GetReadAvailable(); i++)
	{
		RpJumpStats *jump = this->circularRecording.jumps->PeekSingle(i);
		if (!jump)
		{
			break;
		}
		if (jump->overall.serverTick + 2 * 60 * 64 < currentTick)
		{
			numToRemove++;
			continue;
		}
		break;
	}
	this->circularRecording.jumps->Advance(numToRemove);
}

SCMD(kz_testsavereplay, SCFL_REPLAY)
{
	// TODO: 1. Support saving certain parts of the circular buffer only (e.g., last 30 seconds)
	//       2. Clean this mess up
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!g_pFullFileSystem || !player)
	{
		return MRES_SUPERCEDE;
	}

	auto modeInfo = KZ::mode::GetModeInfo(player->modeService);
	char filename[512];

	UUID_t uuid;
	std::string uuidStr = uuid.ToString();
	V_snprintf(filename, sizeof(filename), "%s/%s.replay", KZ_REPLAY_PATH, uuidStr.c_str());
	g_pFullFileSystem->CreateDirHierarchy(KZ_REPLAY_PATH);

	FileHandle_t file = g_pFullFileSystem->Open(filename, "wb");

	if (file)
	{
		ReplayHeader header = {};
		header.magicNumber = KZ_REPLAY_MAGIC;
		header.version = KZ_REPLAY_VERSION;

		V_snprintf(header.player.name, sizeof(header.player.name), "%s", player->GetController()->GetPlayerName());
		header.player.steamid64 = player->GetSteamId64();

		header.type = RP_MANUAL;

		V_snprintf(header.map.name, sizeof(header.map.name), "%s", g_pKZUtils->GetCurrentMapName().Get());
		g_pKZUtils->GetCurrentMapMD5(header.map.md5, sizeof(header.map.md5));

		header.firstWeapon = player->recordingService->circularRecording.earliestWeapon.value();

		header.gloves = player->GetPlayerPawn()->m_EconGloves();

		CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(player->GetPlayerPawn()->m_CBodyComponent()->m_pSceneNode());
		V_snprintf(header.modelName, sizeof(header.modelName), "%s", pSkeleton->m_modelState().m_ModelName().String());

		g_pFullFileSystem->Write(&header, sizeof(header), file);

		i32 tickdataCount = player->recordingService->circularRecording.tickData->GetReadAvailable();
		g_pFullFileSystem->Write(&tickdataCount, sizeof(tickdataCount), file);
		player->recordingService->circularRecording.tickData->WriteToFile(g_pFullFileSystem, file, tickdataCount);

		for (i32 i = 0; i < tickdataCount; i++)
		{
			SubtickData *subtickData = player->recordingService->circularRecording.subtickData->PeekSingle(i);
			g_pFullFileSystem->Write(&subtickData->numSubtickMoves, sizeof(subtickData->numSubtickMoves), file);
			g_pFullFileSystem->Write(&subtickData->subtickMoves, sizeof(SubtickData::RpSubtickMove) * subtickData->numSubtickMoves, file);
		}

		i32 numWeaponChanges = player->recordingService->circularRecording.weaponChangeEvents->GetReadAvailable();
		g_pFullFileSystem->Write(&numWeaponChanges, sizeof(i32), file);

		for (i32 i = 0; i < numWeaponChanges; i++)
		{
			WeaponSwitchEvent *event = player->recordingService->circularRecording.weaponChangeEvents->PeekSingle(i);
			g_pFullFileSystem->Write(&event->serverTick, sizeof(event->serverTick), file);
			g_pFullFileSystem->Write(&event->econInfo.mainInfo, sizeof(event->econInfo.mainInfo), file);
			for (i32 j = 0; j < event->econInfo.mainInfo.numAttributes; j++)
			{
				g_pFullFileSystem->Write(&event->econInfo.attributes[j], sizeof(event->econInfo.attributes[j]), file);
			}
		}

		i32 numJumps = player->recordingService->circularRecording.jumps->GetReadAvailable();
		g_pFullFileSystem->Write(&numJumps, sizeof(i32), file);
		for (i32 i = 0; i < numJumps; i++)
		{
			RpJumpStats *jump = player->recordingService->circularRecording.jumps->PeekSingle(i);
			if (jump)
			{
				// Write jump data
				g_pFullFileSystem->Write(&jump->overall, sizeof(jump->overall), file);
				i32 numStrafes = jump->strafes.size();
				g_pFullFileSystem->Write(&numStrafes, sizeof(numStrafes), file);
				g_pFullFileSystem->Write(jump->strafes.data(), sizeof(RpJumpStats::StrafeData) * numStrafes, file);
				i32 numAACalls = jump->aaCalls.size();
				g_pFullFileSystem->Write(&numAACalls, sizeof(numAACalls), file);
				g_pFullFileSystem->Write(jump->aaCalls.data(), sizeof(RpJumpStats::AAData) * numAACalls, file);
				if (kz_replay_recording_debug.Get())
				{
					player->PrintChat(false, false, "Saved jump %d: %f seconds, %d strafes, %d aa calls", i, jump->overall.airtime, numStrafes,
									  numAACalls);
				}
			}
		}

		i32 numBaseModeStyleEvents = 2 + player->recordingService->circularRecording.earliestStyles.value().size();
		i32 numRpEvents = player->recordingService->circularRecording.rpEvents->GetReadAvailable() + numBaseModeStyleEvents;
		g_pFullFileSystem->Write(&numRpEvents, sizeof(i32), file);

		RpEvent baseCheckpointEvent = {};
		baseCheckpointEvent.serverTick = 0;
		baseCheckpointEvent.type = RPEVENT_CHECKPOINT;
		baseCheckpointEvent.data.checkpoint.index = player->recordingService->circularRecording.earliestCheckpointIndex.value();
		baseCheckpointEvent.data.checkpoint.checkpointCount = player->recordingService->circularRecording.earliestCheckpointCount.value();
		baseCheckpointEvent.data.checkpoint.teleportCount = player->recordingService->circularRecording.earliestTeleportCount.value();
		g_pFullFileSystem->Write(&baseCheckpointEvent, sizeof(RpEvent), file);

		RpEvent baseModeEvent = {};
		baseModeEvent.serverTick = 0;
		baseModeEvent.type = RPEVENT_MODE_CHANGE;
		V_strncpy(baseModeEvent.data.modeChange.name, player->recordingService->circularRecording.earliestMode.value().name,
				  sizeof(baseModeEvent.data.modeChange.name));
		V_strncpy(baseModeEvent.data.modeChange.md5, player->recordingService->circularRecording.earliestMode.value().md5,
				  sizeof(baseModeEvent.data.modeChange.md5));
		g_pFullFileSystem->Write(&baseModeEvent, sizeof(RpEvent), file);

		bool firstStyle = true;
		for (auto &style : player->recordingService->circularRecording.earliestStyles.value())
		{
			RpEvent baseStyleEvent = {};
			baseStyleEvent.serverTick = 0;
			baseStyleEvent.type = RPEVENT_STYLE_CHANGE;
			V_strncpy(baseStyleEvent.data.styleChange.name, style.name, sizeof(baseStyleEvent.data.styleChange.name));
			V_strncpy(baseStyleEvent.data.styleChange.md5, style.md5, sizeof(baseStyleEvent.data.styleChange.md5));
			baseStyleEvent.data.styleChange.clearStyles = firstStyle;
			firstStyle = false;
			g_pFullFileSystem->Write(&baseStyleEvent, sizeof(RpEvent), file);
		}

		numRpEvents = player->recordingService->circularRecording.rpEvents->GetReadAvailable();
		for (i32 i = 0; i < numRpEvents; i++)
		{
			RpEvent *rpEvent = player->recordingService->circularRecording.rpEvents->PeekSingle(i);
			g_pFullFileSystem->Write(rpEvent, sizeof(RpEvent), file);
		}

		// Command data is always written last because playback doesn't read this part of the replay.
		CmdData cmdData = {};
		i32 cmddataCount = player->recordingService->circularRecording.cmdData->GetReadAvailable();
		g_pFullFileSystem->Write(&cmddataCount, sizeof(cmddataCount), file);
		player->recordingService->circularRecording.cmdData->WriteToFile(g_pFullFileSystem, file, cmddataCount);

		for (i32 i = 0; i < cmddataCount; i++)
		{
			SubtickData *cmdSubtickData = player->recordingService->circularRecording.cmdSubtickData->PeekSingle(i);
			g_pFullFileSystem->Write(&cmdSubtickData->numSubtickMoves, sizeof(cmdSubtickData->numSubtickMoves), file);
			g_pFullFileSystem->Write(&cmdSubtickData->subtickMoves, sizeof(SubtickData::RpSubtickMove) * cmdSubtickData->numSubtickMoves, file);
		}
		g_pFullFileSystem->Close(file);
	}
	player->PrintChat(false, false, "Saved replay to %s", filename);
	return MRES_SUPERCEDE;
}

JumpRecorder::JumpRecorder(Jump *jump) : Recorder(jump->player, 5.0f, false, DistanceTier_None)
{
	this->desiredStopTime = g_pKZUtils->GetServerGlobals()->curtime + 2.0f;
	this->baseHeader.type = RP_JUMPSTATS;
	this->header.jumpType = jump->jumpType;
	this->header.distance = jump->GetDistance();
	this->header.airTime = jump->airtime;
	this->header.pre = jump->GetTakeoffSpeed();
	this->header.max = jump->GetMaxSpeed();
	this->header.sync = jump->GetSync();
}

i32 JumpRecorder::WriteHeader(FileHandle_t file)
{
	i32 bytesWritten = Recorder::WriteHeader(file);
	return bytesWritten + g_pFullFileSystem->Write(&this->header, sizeof(this->header), file);
}

RunRecorder::RunRecorder(KZPlayer *player) : Recorder(player, 5.0f, true, DistanceTier_Ownage)
{
	this->baseHeader.type = RP_RUN;
	this->header.courseID = player->timerService->GetCourse() ? player->timerService->GetCourse()->id : INVALID_COURSE_NUMBER;
	V_strncpy(this->header.mode.name, KZ::mode::GetModeInfo(player->modeService).longModeName.Get(), sizeof(this->header.mode.name));
	V_strncpy(this->header.mode.md5, KZ::mode::GetModeInfo(player->modeService).md5, sizeof(this->header.mode.md5));

	FOR_EACH_VEC(player->styleServices, i)
	{
		auto styleService = player->styleServices[i];
		auto styleInfo = KZ::style::GetStyleInfo(styleService);
		RpModeStyleInfo style = {};
		V_strncpy(style.name, styleInfo.longName, sizeof(style.name));
		V_strncpy(style.md5, styleInfo.md5, sizeof(style.md5));
		this->header.styles.push_back(style);
	}
}

void RunRecorder::End(f32 time, i32 numTeleports)
{
	this->header.time = time;
	this->header.numTeleports = numTeleports;
	this->desiredStopTime = g_pKZUtils->GetServerGlobals()->curtime + 4.0f;
}

i32 RunRecorder::WriteHeader(FileHandle_t file)
{
	i32 bytesWritten = Recorder::WriteHeader(file);
	return bytesWritten + g_pFullFileSystem->Write(&this->header, sizeof(this->header), file);
}

Recorder::Recorder(KZPlayer *player, f32 numSeconds, bool copyTimerEvents, DistanceTier copyJumps)
{
	baseHeader.magicNumber = KZ_REPLAY_MAGIC;
	baseHeader.version = KZ_REPLAY_VERSION;
	baseHeader.type = RP_MANUAL;
	V_snprintf(baseHeader.player.name, sizeof(baseHeader.player.name), "%s", player->GetController()->GetPlayerName());
	baseHeader.player.steamid64 = player->GetSteamId64();
	V_snprintf(baseHeader.map.name, sizeof(baseHeader.map.name), "%s", g_pKZUtils->GetCurrentMapName().Get());
	g_pKZUtils->GetCurrentMapMD5(baseHeader.map.md5, sizeof(baseHeader.map.md5));
	baseHeader.gloves = player->GetPlayerPawn()->m_EconGloves();
	CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(player->GetPlayerPawn()->m_CBodyComponent()->m_pSceneNode());
	V_snprintf(baseHeader.modelName, sizeof(baseHeader.modelName), "%s", pSkeleton->m_modelState().m_ModelName().String());

	CircularRecorder &circular = player->recordingService->circularRecording;
	// Go through the events and fetch the first events within the time frame.
	// Iterate backwards to find the earliest event that is still within the time frame.
	i32 earliestWeaponIndex = -1;
	i32 earliestModeEventIndex = -1;
	i32 earliestStyleEventIndex = -1;
	i32 earliestCheckpointEventIndex = -1;

	if (circular.tickData->GetReadAvailable() == 0)
	{
		return;
	}
	i32 numTickData = MIN(circular.tickData->GetReadAvailable(), (i32)(numSeconds * ENGINE_FIXED_TICK_RATE));
	u32 earliestTick = circular.tickData->PeekSingle(circular.tickData->GetReadAvailable() - numTickData)->serverTick;
	for (i32 i = circular.tickData->GetReadAvailable() - numTickData; i < circular.tickData->GetReadAvailable(); i++)
	{
		TickData *tickData = circular.tickData->PeekSingle(i);
		if (!tickData)
		{
			break;
		}
		this->tickData.push_back(*tickData);
		this->subtickData.push_back(*circular.subtickData->PeekSingle(i));
	}
	i32 first = 0;
	bool shouldCopy = false;
	for (; first < circular.rpEvents->GetReadAvailable(); first++)
	{
		shouldCopy = true;
		RpEvent *event = circular.rpEvents->PeekSingle(first);
		if (event->serverTick >= earliestTick)
		{
			break;
		}
		if (event->type == RPEVENT_CHECKPOINT)
		{
			earliestCheckpointEventIndex = first;
		}
		else if (event->type == RPEVENT_MODE_CHANGE)
		{
			earliestModeEventIndex = first;
		}
		else if (event->type == RPEVENT_STYLE_CHANGE && event->data.styleChange.clearStyles)
		{
			earliestStyleEventIndex = first;
		}
	}
	if (earliestCheckpointEventIndex == -1)
	{
		RpEvent baseCheckpointEvent = {};
		baseCheckpointEvent.serverTick = 0;
		baseCheckpointEvent.type = RPEVENT_CHECKPOINT;
		baseCheckpointEvent.data.checkpoint.index = circular.earliestCheckpointIndex.value_or(0);
		baseCheckpointEvent.data.checkpoint.checkpointCount = circular.earliestCheckpointCount.value_or(0);
		baseCheckpointEvent.data.checkpoint.teleportCount = circular.earliestTeleportCount.value_or(0);
		this->rpEvents.push_back(baseCheckpointEvent);
	}
	else
	{
		RpEvent baseCheckpointEvent = *circular.rpEvents->PeekSingle(earliestCheckpointEventIndex);
		baseCheckpointEvent.serverTick = 0;
		this->rpEvents.push_back(baseCheckpointEvent);
	}
	if (earliestModeEventIndex == -1)
	{
		RpEvent baseModeEvent = {};
		baseModeEvent.serverTick = 0;
		baseModeEvent.type = RPEVENT_MODE_CHANGE;
		V_strncpy(baseModeEvent.data.modeChange.name, circular.earliestMode.value().name, sizeof(baseModeEvent.data.modeChange.name));
		V_strncpy(baseModeEvent.data.modeChange.md5, circular.earliestMode.value().md5, sizeof(baseModeEvent.data.modeChange.md5));
		this->rpEvents.push_back(baseModeEvent);
	}
	else
	{
		RpEvent baseModeEvent = *circular.rpEvents->PeekSingle(earliestModeEventIndex);
		baseModeEvent.serverTick = 0;
		this->rpEvents.push_back(baseModeEvent);
	}
	if (earliestStyleEventIndex == -1)
	{
		bool firstStyle = true;
		for (auto &style : circular.earliestStyles.value_or(std::vector<RpModeStyleInfo>()))
		{
			RpEvent baseStyleEvent = {};
			baseStyleEvent.serverTick = 0;
			baseStyleEvent.type = RPEVENT_STYLE_CHANGE;
			V_strncpy(baseStyleEvent.data.styleChange.name, style.name, sizeof(baseStyleEvent.data.styleChange.name));
			V_strncpy(baseStyleEvent.data.styleChange.md5, style.md5, sizeof(baseStyleEvent.data.styleChange.md5));
			baseStyleEvent.data.styleChange.clearStyles = firstStyle;
			firstStyle = false;
			this->rpEvents.push_back(baseStyleEvent);
		}
	}
	else
	{
		RpEvent baseStyleEvent = *circular.rpEvents->PeekSingle(earliestStyleEventIndex);
		i32 eventServerTick = baseStyleEvent.serverTick;
		baseStyleEvent.serverTick = 0;
		this->rpEvents.push_back(baseStyleEvent);
		// Copy all style change events with the same server tick (they were part of the same batch)
		for (i32 i = earliestStyleEventIndex + 1; i < circular.rpEvents->GetReadAvailable(); i++)
		{
			RpEvent *event = circular.rpEvents->PeekSingle(i);
			if (!event || event->type != RPEVENT_STYLE_CHANGE || event->serverTick != eventServerTick)
			{
				break;
			}
			this->rpEvents.push_back(*event);
		}
	}
	if (shouldCopy)
	{
		for (i32 i = first; i < circular.rpEvents->GetReadAvailable(); i++)
		{
			if (!copyTimerEvents && circular.rpEvents->PeekSingle(i)->type == RPEVENT_TIMER_EVENT)
			{
				continue;
			}
			this->rpEvents.push_back(*circular.rpEvents->PeekSingle(i));
		}
	}
	shouldCopy = false;
	for (first = 0; first < circular.weaponChangeEvents->GetReadAvailable(); first++)
	{
		shouldCopy = true;
		WeaponSwitchEvent *event = circular.weaponChangeEvents->PeekSingle(first);

		if (event->serverTick >= earliestTick)
		{
			break;
		}
		earliestWeaponIndex = first;
	}
	if (earliestWeaponIndex != -1)
	{
		this->baseHeader.firstWeapon = circular.weaponChangeEvents->PeekSingle(earliestWeaponIndex)->econInfo;
	}
	else
	{
		this->baseHeader.firstWeapon = circular.earliestWeapon.value();
	}
	if (shouldCopy)
	{
		for (i32 i = first; i < circular.weaponChangeEvents->GetReadAvailable(); i++)
		{
			this->weaponChangeEvents.push_back(*circular.weaponChangeEvents->PeekSingle(i));
		}
	}
	shouldCopy = false;
	for (first = 0; first < circular.jumps->GetReadAvailable(); first++)
	{
		RpJumpStats *jump = circular.jumps->PeekSingle(first);
		if (!jump)
		{
			break;
		}
		shouldCopy = true;
		if (jump->overall.serverTick >= earliestTick)
		{
			break;
		}
	}
	if (shouldCopy)
	{
		for (i32 i = first; i < circular.jumps->GetReadAvailable(); i++)
		{
			RpJumpStats *jump = circular.jumps->PeekSingle(i);

			if (jump->overall.distanceTier < copyJumps)
			{
				continue;
			}
			this->jumps.push_back(*jump);
		}
	}

	shouldCopy = false;
	for (first = 0; first < circular.cmdData->GetReadAvailable(); first++)
	{
		CmdData *cmdData = circular.cmdData->PeekSingle(first);
		if (!cmdData)
		{
			break;
		}
		shouldCopy = true;
		if ((u32)cmdData->serverTick >= earliestTick)
		{
			break;
		}
	}
	if (shouldCopy)
	{
		for (i32 i = first; i < circular.cmdData->GetReadAvailable(); i++)
		{
			this->cmdData.push_back(*circular.cmdData->PeekSingle(i));
			this->cmdSubtickData.push_back(*circular.cmdSubtickData->PeekSingle(i));
		}
	}
}

void Recorder::WriteToFile()
{
	char filename[512];
	std::string uuidStr = this->uuid.ToString();
	V_snprintf(filename, sizeof(filename), "%s/%s.replay", KZ_REPLAY_PATH, uuidStr.c_str());
	g_pFullFileSystem->CreateDirHierarchy(KZ_REPLAY_PATH);
	FileHandle_t file = g_pFullFileSystem->Open(filename, "wb");
	if (!file)
	{
		META_CONPRINTF("Failed to open replay file for writing: %s\n", filename);
		return;
	}
	// Order of writing must match order of reading in kz_replaydata.cpp
	i32 bytesWritten = 0;
	bytesWritten += this->WriteHeader(file);

	bytesWritten += this->WriteTickData(file);

	bytesWritten += this->WriteWeaponChanges(file);

	bytesWritten += this->WriteJumps(file);

	bytesWritten += this->WriteEvents(file);

	bytesWritten += this->WriteCmdData(file);
	if (kz_replay_recording_debug.Get())
	{
		utils::PrintChatAll("Saved replay to %s (%d bytes)", filename, bytesWritten);
	}
	g_pFullFileSystem->Close(file);
}

i32 Recorder::WriteHeader(FileHandle_t file)
{
	// Write the base header
	return g_pFullFileSystem->Write(&this->baseHeader, sizeof(this->baseHeader), file);
}

i32 Recorder::WriteTickData(FileHandle_t file)
{
	i32 numWritten = 0;
	i32 tickdataCount = this->tickData.size();
	numWritten += g_pFullFileSystem->Write(&tickdataCount, sizeof(tickdataCount), file);
	for (i32 i = 0; i < tickdataCount; i++)
	{
		TickData &tickData = this->tickData[i];
		numWritten += g_pFullFileSystem->Write(&tickData, sizeof(TickData), file);
	}
	for (i32 i = 0; i < tickdataCount; i++)
	{
		SubtickData &subtickData = this->subtickData[i];
		numWritten += g_pFullFileSystem->Write(&subtickData.numSubtickMoves, sizeof(subtickData.numSubtickMoves), file);
		numWritten += g_pFullFileSystem->Write(&subtickData.subtickMoves, sizeof(SubtickData::RpSubtickMove) * subtickData.numSubtickMoves, file);
	}
	if (kz_replay_recording_debug.Get())
	{
		utils::PrintChatAll("Wrote %d tick data entries of %d bytes", tickdataCount, numWritten);
	}
	return numWritten;
}

i32 Recorder::WriteWeaponChanges(FileHandle_t file)
{
	i32 numWritten = 0;
	i32 numWeaponChanges = this->weaponChangeEvents.size();
	numWritten += g_pFullFileSystem->Write(&numWeaponChanges, sizeof(numWeaponChanges), file);
	for (i32 i = 0; i < numWeaponChanges; i++)
	{
		WeaponSwitchEvent &event = this->weaponChangeEvents[i];
		numWritten += g_pFullFileSystem->Write(&event.serverTick, sizeof(event.serverTick), file);
		numWritten += g_pFullFileSystem->Write(&event.econInfo.mainInfo, sizeof(event.econInfo.mainInfo), file);
		for (i32 j = 0; j < event.econInfo.mainInfo.numAttributes; j++)
		{
			numWritten += g_pFullFileSystem->Write(&event.econInfo.attributes[j], sizeof(event.econInfo.attributes[j]), file);
		}
	}
	if (kz_replay_recording_debug.Get())
	{
		utils::PrintChatAll("Wrote %d weapon change events of %d bytes", numWeaponChanges, numWritten);
	}
	return numWritten;
}

i32 Recorder::WriteJumps(FileHandle_t file)
{
	i32 numWritten = 0;
	i32 numJumps = this->jumps.size();
	numWritten += g_pFullFileSystem->Write(&numJumps, sizeof(numJumps), file);
	for (i32 i = 0; i < numJumps; i++)
	{
		RpJumpStats &jump = this->jumps[i];
		// Write jump data
		numWritten += g_pFullFileSystem->Write(&jump.overall, sizeof(jump.overall), file);
		i32 numStrafes = jump.strafes.size();
		numWritten += g_pFullFileSystem->Write(&numStrafes, sizeof(numStrafes), file);
		numWritten += g_pFullFileSystem->Write(jump.strafes.data(), sizeof(RpJumpStats::StrafeData) * numStrafes, file);
		i32 numAACalls = jump.aaCalls.size();
		numWritten += g_pFullFileSystem->Write(&numAACalls, sizeof(numAACalls), file);
		numWritten += g_pFullFileSystem->Write(jump.aaCalls.data(), sizeof(RpJumpStats::AAData) * numAACalls, file);
	}
	if (kz_replay_recording_debug.Get())
	{
		utils::PrintChatAll("Wrote %d jumps of %d bytes", numJumps, numWritten);
	}
	return numWritten;
}

i32 Recorder::WriteEvents(FileHandle_t file)
{
	i32 numWritten = 0;
	i32 numEvents = this->rpEvents.size();
	numWritten += g_pFullFileSystem->Write(&numEvents, sizeof(numEvents), file);
	for (i32 i = 0; i < numEvents; i++)
	{
		RpEvent &event = this->rpEvents[i];
		numWritten += g_pFullFileSystem->Write(&event, sizeof(event), file);
	}
	if (kz_replay_recording_debug.Get())
	{
		utils::PrintChatAll("Wrote %d events of %d bytes", numEvents, numWritten);
	}
	return numWritten;
}

i32 Recorder::WriteCmdData(FileHandle_t file)
{
	i32 numWritten = 0;
	i32 cmddataCount = this->cmdData.size();
	numWritten += g_pFullFileSystem->Write(&cmddataCount, sizeof(cmddataCount), file);
	for (i32 i = 0; i < cmddataCount; i++)
	{
		CmdData &cmdData = this->cmdData[i];
		numWritten += g_pFullFileSystem->Write(&cmdData, sizeof(CmdData), file);
	}
	for (i32 i = 0; i < cmddataCount; i++)
	{
		SubtickData &cmdSubtickData = this->cmdSubtickData[i];
		numWritten += g_pFullFileSystem->Write(&cmdSubtickData.numSubtickMoves, sizeof(cmdSubtickData.numSubtickMoves), file);
		numWritten +=
			g_pFullFileSystem->Write(&cmdSubtickData.subtickMoves, sizeof(SubtickData::RpSubtickMove) * cmdSubtickData.numSubtickMoves, file);
	}
	if (kz_replay_recording_debug.Get())
	{
		utils::PrintChatAll("Wrote %d cmd data entries of %d bytes", cmddataCount, numWritten);
	}
	return numWritten;
}
