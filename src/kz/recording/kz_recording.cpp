
#include "kz_recording.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "utils/simplecmds.h"
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

void KZRecordingService::OnPostThinkPost()
{
	// record data for replay playback
	if (!this->player->IsAlive())
	{
		return;
	}

	Tickdata tickdata = {};
	tickdata.gameTick = g_pKZUtils->GetServerGlobals()->tickcount;
	tickdata.gameTime = g_pKZUtils->GetServerGlobals()->curtime;
	tickdata.realTime = g_pKZUtils->GetServerGlobals()->realtime;
	time_t unixTime = 0;
	time(&unixTime);
	tickdata.unixTime = (u64)unixTime;
	this->player->GetOrigin(&tickdata.origin);
	this->player->GetVelocity(&tickdata.velocity);
	this->player->GetAngles(&tickdata.angles);
	auto movementServices = this->player->GetMoveServices();
	tickdata.duckSpeed = movementServices->m_flDuckSpeed;
	tickdata.duckAmount = movementServices->m_flDuckAmount;

	tickdata.buttons = movementServices->m_nButtons()->m_pButtonStates[0];
	tickdata.replayFlags.ducking = movementServices->m_bDucking;
	tickdata.replayFlags.ducked = movementServices->m_bDucked;
	tickdata.entityFlags = this->player->GetPlayerPawn()->m_fFlags();

	this->circularRecording.tickdata.Write(tickdata);
}

void KZRecordingService::Reset()
{
	this->circularRecording.events.Purge();
	this->circularRecording.tickdata.Advance(this->circularRecording.tickdata.GetReadAvailable());
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

		// write tickdata
		i32 tickdataCount = player->recordingService->circularRecording.tickdata.GetReadAvailable();
		g_pFullFileSystem->Write(&tickdataCount, sizeof(tickdataCount), file);
		// circular buffer reads FIFO
		for (i32 i = tickdataCount - 1; i >= 0; i--)
		{
			Tickdata tickdata = {};
			player->recordingService->circularRecording.tickdata.Peek(&tickdata, i);
			g_pFullFileSystem->Write(&tickdata, sizeof(tickdata), file);
		}
		g_pFullFileSystem->Close(file);
	}

	return MRES_SUPERCEDE;
}
