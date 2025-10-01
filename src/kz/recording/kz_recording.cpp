
#include "kz_recording.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
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
		player->PrintChat(false, false, "timerstartpost");
	}

	virtual void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed)
	{
		player->PrintChat(false, false, "timerendpost");
	}

	virtual void OnTimerStopped(KZPlayer *player, u32 courseGUID)
	{
		player->PrintChat(false, false, "timerstopped");
	}

	virtual void OnTimerInvalidated(KZPlayer *player)
	{
		player->PrintChat(false, false, "timerinvalidated");
	}

	virtual void OnPausePost(KZPlayer *player)
	{
		player->PrintChat(false, false, "pausepost");
	}

	virtual void OnResumePost(KZPlayer *player)
	{
		player->PrintChat(false, false, "resumepost");
	}

	virtual void OnSplitZoneTouchPost(KZPlayer *player, u32 splitZone)
	{
		player->PrintChat(false, false, "splitzonepost %d", splitZone);
	}

	virtual void OnCheckpointZoneTouchPost(KZPlayer *player, u32 checkpoint)
	{
		player->PrintChat(false, false, "checkpointzone %d", checkpoint);
	}

	virtual void OnStageZoneTouchPost(KZPlayer *player, u32 stageZone)
	{
		player->PrintChat(false, false, "stagezone %d", stageZone);
	}
} timerEventListener;

CConVar<bool> kz_replay_debug_recording("kz_replay_debug_recording", FCVAR_NONE, "Debug replay recording", false);

void KZRecordingService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
}

void KZRecordingService::OnProcessUsercmds(PlayerCommand *cmds, int numCmds)
{
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
		this->lastCmdNumReceived = pc.cmdNum;
	}
}

void KZRecordingService::OnPlayerJoinTeam(i32 team) {}

void KZRecordingService::Reset()
{
	this->circularRecording.events.Purge();
	this->circularRecording.tickData->Advance(this->circularRecording.tickData->GetReadAvailable());
	this->circularRecording.subtickData->Advance(this->circularRecording.subtickData->GetReadAvailable());
	this->circularRecording.cmdData->Advance(this->circularRecording.cmdData->GetReadAvailable());
	this->circularRecording.cmdSubtickData->Advance(this->circularRecording.cmdSubtickData->GetReadAvailable());
	this->circularRecording.weaponChangeEvents->Advance(this->circularRecording.weaponChangeEvents->GetReadAvailable());

	this->lastCmdNumReceived = 0;
}

void KZRecordingService::OnTimerStart()
{
	this->recorders.push_back(Recorder());
	this->currentRecorder = this->recorders.back().uuid;
}

void KZRecordingService::OnTimerStop()
{
	this->currentRecorder = {};
}

void KZRecordingService::OnTimerEnd()
{
	for (auto &recorder : this->recorders)
	{
		if (recorder.uuid == this->currentRecorder)
		{
			recorder.desiredStopTime = g_pKZUtils->GetServerGlobals()->curtime + 2.0f; // record 2 seconds after timer end
			break;
		}
	}
}

void KZRecordingService::OnPhysicsSimulate()
{
	CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(this->player->GetPlayerPawn()->m_CBodyComponent()->m_pSceneNode());
	utils::PrintAlertAll("ModelState: mesh group %llu", pSkeleton->m_modelState().m_MeshGroupMask());
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
	// record data for replay playback
	if (!this->player->IsAlive() || this->player->IsFakeClient())
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
	this->circularRecording.subtickData->Write(this->currentSubtickData);

	auto weapon = this->player->GetPlayerPawn()->m_pWeaponServices()->m_hActiveWeapon.Get();
	auto weaponEconInfo = EconInfo(weapon);
	if (this->currentWeaponEconInfo != weaponEconInfo)
	{
		this->currentWeaponEconInfo = weaponEconInfo;
		WeaponSwitchEvent event;
		event.serverTick = this->currentTickData.serverTick;
		event.econInfo = weaponEconInfo;
		this->circularRecording.weaponChangeEvents->Write(event);
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
			if (event.type == RPEVENT_MODE_CHANGE)
			{
				V_strncpy(this->circularRecording.earliestMode.value().name, event.data.modeChange.name,
						  sizeof(this->circularRecording.earliestMode.value().name));
				V_strncpy(this->circularRecording.earliestMode.value().md5, event.data.modeChange.md5,
						  sizeof(this->circularRecording.earliestMode.value().md5));
			}
			else if (event.type == RPEVENT_STYLE_ADD)
			{
				bool found = false;
				// Check if we already have this style. This shouldn't happen but just in case.
				for (auto &style : this->circularRecording.earliestStyles.value())
				{
					if (KZ_STREQI(style.name, event.data.styleChange.name))
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					RpModeStyleInfo style = {};
					V_strncpy(style.name, event.data.styleChange.name, sizeof(style.name));
					V_strncpy(style.md5, event.data.styleChange.md5, sizeof(style.md5));
					this->circularRecording.earliestStyles->push_back(style);
				}
			}
			else if (event.type == RPEVENT_STYLE_REMOVE)
			{
				for (auto it = this->circularRecording.earliestStyles->begin(); it != this->circularRecording.earliestStyles->end(); ++it)
				{
					if (KZ_STREQI(it->name, event.data.styleChange.name))
					{
						this->circularRecording.earliestStyles->erase(it);
						break;
					}
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
}

SCMD(kz_testsavereplay, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!g_pFullFileSystem || !player)
	{
		return MRES_SUPERCEDE;
	}

	auto modeInfo = KZ::mode::GetModeInfo(player->modeService);
	char replayPath[128];
	V_snprintf(replayPath, sizeof(replayPath), KZ_REPLAY_PATH, g_pKZUtils->GetCurrentMapName().Get());
	char filename[512];

	i32 courseId = INVALID_COURSE_NUMBER;
	const KZCourseDescriptor *course = player->timerService->GetCourse();
	if (course)
	{
		courseId = course->id;
	}

	// TODO: Append uuid into the file name?
	UUID_t uuid;
	std::string uuidStr = uuid.ToString();
	V_snprintf(filename, sizeof(filename), "%s/%s.replay", replayPath, uuidStr.c_str());
	g_pFullFileSystem->CreateDirHierarchy(replayPath);

	FileHandle_t file = g_pFullFileSystem->Open(filename, "wb");

	if (file)
	{
		VPROF_ENTER_SCOPE_KZ("SaveReplay (Header)");
		ReplayHeader header = {};
		header.magicNumber = KZ_REPLAY_MAGIC;
		header.version = KZ_REPLAY_VERSION;
		V_snprintf(header.player.name, sizeof(header.player.name), "%s", player->GetName());
		header.player.steamid64 = player->GetSteamId64();
		header.type = RP_MANUAL;
		V_snprintf(header.map.name, sizeof(header.map.name), "%s", g_pKZUtils->GetCurrentMapName().Get());
		g_pKZUtils->GetCurrentMapMD5(header.map.md5, sizeof(header.map.md5));
		header.firstWeapon = player->recordingService->circularRecording.earliestWeapon.value();
		header.gloves = player->GetPlayerPawn()->m_EconGloves();
		CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(player->GetPlayerPawn()->m_CBodyComponent()->m_pSceneNode());
		V_snprintf(header.modelName, sizeof(header.modelName), "%s", pSkeleton->m_modelState().m_ModelName().String());
		g_pFullFileSystem->Write(&header, sizeof(header), file);
		VPROF_EXIT_SCOPE();

		VPROF_ENTER_SCOPE_KZ("SaveReplay (TickData)");
		i32 tickdataCount = player->recordingService->circularRecording.tickData->GetReadAvailable();
		g_pFullFileSystem->Write(&tickdataCount, sizeof(tickdataCount), file);
		TickData tickData = {};
		player->recordingService->circularRecording.tickData->WriteToFile(g_pFullFileSystem, file, tickdataCount);
		VPROF_EXIT_SCOPE();

		SubtickData subtickData = {};
		for (i32 i = 0; i < tickdataCount; i++)
		{
			VPROF_ENTER_SCOPE_KZ("SaveReplay (SubtickData Peek)");
			player->recordingService->circularRecording.subtickData->Peek(&subtickData, 1, i);
			VPROF_EXIT_SCOPE();
			VPROF_ENTER_SCOPE_KZ("SaveReplay (SubtickData Write)");
			g_pFullFileSystem->Write(&subtickData.numSubtickMoves, sizeof(subtickData.numSubtickMoves), file);
			g_pFullFileSystem->Write(&subtickData.subtickMoves, sizeof(SubtickData::RpSubtickMove) * subtickData.numSubtickMoves, file);
			VPROF_EXIT_SCOPE();
		}

		VPROF_ENTER_SCOPE_KZ("SaveReplay (WeaponData)");
		i32 numWeaponChanges = player->recordingService->circularRecording.weaponChangeEvents->GetReadAvailable();
		g_pFullFileSystem->Write(&numWeaponChanges, sizeof(i32), file);
		// This struct is HUGE. Allocate it on the heap.
		WeaponSwitchEvent *event = new WeaponSwitchEvent();
		for (i32 i = 0; i < numWeaponChanges; i++)
		{
			player->recordingService->circularRecording.weaponChangeEvents->Peek(event, 1, i);
			g_pFullFileSystem->Write(&event->serverTick, sizeof(event->serverTick), file);
			g_pFullFileSystem->Write(&event->econInfo.mainInfo, sizeof(event->econInfo.mainInfo), file);
			for (int j = 0; j < event->econInfo.mainInfo.numAttributes; j++)
			{
				g_pFullFileSystem->Write(&event->econInfo.attributes[j], sizeof(event->econInfo.attributes[j]), file);
			}
		}
		delete event;
		VPROF_EXIT_SCOPE();

		VPROF_ENTER_SCOPE_KZ("SaveReplay (RpEvents)");
		i32 numRpEvents = player->recordingService->circularRecording.rpEvents->GetReadAvailable();
		g_pFullFileSystem->Write(&numRpEvents, sizeof(i32), file);
		RpEvent *rpEvent = new RpEvent();
		for (i32 i = 0; i < numRpEvents; i++)
		{
			player->recordingService->circularRecording.rpEvents->Peek(rpEvent, 1, i);
			g_pFullFileSystem->Write(&rpEvent->serverTick, sizeof(rpEvent->serverTick), file);
			g_pFullFileSystem->Write(&rpEvent->type, sizeof(rpEvent->type), file);
			g_pFullFileSystem->Write(&rpEvent->data, sizeof(rpEvent->data), file);
		}
		delete rpEvent;
		VPROF_EXIT_SCOPE();

		VPROF_ENTER_SCOPE_KZ("SaveReplay (CmdData)");
		// Command data is always written last because playback doesn't read this part of the replay.
		CmdData cmdData = {};
		i32 cmddataCount = player->recordingService->circularRecording.cmdData->GetReadAvailable();
		g_pFullFileSystem->Write(&cmddataCount, sizeof(cmddataCount), file);
		player->recordingService->circularRecording.cmdData->WriteToFile(g_pFullFileSystem, file, cmddataCount);
		VPROF_EXIT_SCOPE();

		SubtickData cmdSubtickData = {};
		for (i32 i = 0; i < cmddataCount; i++)
		{
			VPROF_ENTER_SCOPE_KZ("SaveReplay (CmdSubtickData Peek)");
			player->recordingService->circularRecording.cmdSubtickData->Peek(&cmdSubtickData, 1, i);
			VPROF_EXIT_SCOPE();
			VPROF_ENTER_SCOPE_KZ("SaveReplay (CmdSubtickData Write)");
			g_pFullFileSystem->Write(&cmdSubtickData.numSubtickMoves, sizeof(cmdSubtickData.numSubtickMoves), file);
			g_pFullFileSystem->Write(&cmdSubtickData.subtickMoves, sizeof(SubtickData::RpSubtickMove) * cmdSubtickData.numSubtickMoves, file);
			VPROF_EXIT_SCOPE();
		}
		g_pFullFileSystem->Close(file);
	}
	player->PrintChat(false, false, "Saved replay to %s", filename);
	return MRES_SUPERCEDE;
}
