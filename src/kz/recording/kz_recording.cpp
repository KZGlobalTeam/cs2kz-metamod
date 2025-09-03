
#include "kz_recording.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "utils/simplecmds.h"
#include "sdk/usercmd.h"
#include "filesystem.h"

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
} timerEventListener;

void KZRecordingService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
}

void KZRecordingService::OnPlayerJoinTeam(i32 team) {}

void KZRecordingService::OnPostThinkPost() {}

void KZRecordingService::Reset()
{
	this->circularRecording.events.Purge();
	this->circularRecording.tickData.Advance(this->circularRecording.tickData.GetReadAvailable());
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
			this->currentSubtickData.subtickMoves[i].analogMove.analog_pitch_delta = move.analog_pitch_delta();
			this->currentSubtickData.subtickMoves[i].analogMove.analog_yaw_delta = move.analog_yaw_delta();
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
	this->circularRecording.tickData.Write(this->currentTickData);
	this->circularRecording.subtickData.Write(this->currentSubtickData);
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
	V_snprintf(replayPath, sizeof(replayPath), KZ_REPLAY_RUNS_PATH "/%s", g_pKZUtils->GetCurrentMapName().Get());
	char filename[512];

	i32 courseId = INVALID_COURSE_NUMBER;
	const KZCourseDescriptor *course = player->timerService->GetCourse();
	if (course)
	{
		courseId = course->id;
	}

	// TODO: Append uuid into the file name?
	V_snprintf(filename, sizeof(filename), "%s/%i_%s_%s.replay", replayPath, courseId, modeInfo.shortModeName.Get(),
			   player->timerService->GetCurrentTimeType() == KZTimerService::TimeType_Pro ? "PRO" : "TP");
	g_pFullFileSystem->CreateDirHierarchy(replayPath);

	FileHandle_t file = g_pFullFileSystem->Open(filename, "wb");
	if (file)
	{
		ReplayHeader header = {};
		header.magicNumber = KZ_REPLAY_MAGIC;
		header.version = KZ_REPLAY_VERSION;
		V_snprintf(header.player.name, sizeof(header.player.name), "%s", player->GetName());
		header.player.steamid64 = player->GetSteamId64();

		V_snprintf(header.mode.name, sizeof(header.mode.name), "%s", modeInfo.longModeName.Get());
		V_snprintf(header.mode.md5, sizeof(header.mode.md5), "%s", modeInfo.md5);

		V_snprintf(header.map.name, sizeof(header.map.name), "%s", g_pKZUtils->GetCurrentMapName().Get());
		g_pKZUtils->GetCurrentMapMD5(header.map.md5, sizeof(header.map.md5));
		// TODO: styles!

		g_pFullFileSystem->Write(&header, sizeof(header), file);

		// write tickData
		i32 tickdataCount = player->recordingService->circularRecording.tickData.GetReadAvailable();
		g_pFullFileSystem->Write(&tickdataCount, sizeof(tickdataCount), file);
		// circular buffer reads FIFO
		for (i32 i = tickdataCount - 1; i >= 0; i--)
		{
			TickData tickData = {};
			player->recordingService->circularRecording.tickData.Peek(&tickData, i);
			g_pFullFileSystem->Write(&tickData, sizeof(tickData), file);
		}
		for (i32 i = tickdataCount - 1; i >= 0; i--)
		{
			SubtickData subtickData = {};
			player->recordingService->circularRecording.subtickData.Peek(&subtickData, i);
			g_pFullFileSystem->Write(&subtickData.numSubtickMoves, sizeof(subtickData.numSubtickMoves), file);
			g_pFullFileSystem->Write(&subtickData.subtickMoves, sizeof(SubtickData::RpSubtickMove) * subtickData.numSubtickMoves, file);
		}
		g_pFullFileSystem->Close(file);
	}

	return MRES_SUPERCEDE;
}
