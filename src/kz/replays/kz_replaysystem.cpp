
#include "kz/kz.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "kz/language/kz_language.h"
#include "kz/noclip/kz_noclip.h"
#include "kz_replay.h"
#include "kz_replaysystem.h"
#include "utils/simplecmds.h"

#include "utlvector.h"
#include "filesystem.h"

struct ReplayPlayback
{
	bool valid;
	ReplayHeader header;
	i32 tickCount;
	Tickdata *tickdata;
	i32 currentTick;
	bool playingReplay;
};

struct
{
	ReplayPlayback currentReplay;
	CHandle<CCSPlayerController> bot;
} g_replaySystem;

static_function void KickBots()
{
	auto bot = g_replaySystem.bot.Get();
	if (!bot)
	{
		return;
	}
	interfaces::pEngine->KickClient(bot->GetPlayerSlot(), "bye bot", NETWORK_DISCONNECT_KICKED);
	g_replaySystem.bot.Term();
}

static_function void SpawnBots()
{
	if (g_replaySystem.bot.Get())
	{
		return;
	}
	BotProfile *profile = g_pKZUtils->GetBotProfile(1, CS_TEAM_CT, 2);
	if (!profile)
	{
		return;
	}
	CCSPlayerController *bot = g_pKZUtils->CreateBot(profile, CS_TEAM_CT);
	if (!bot)
	{
		return;
	}

	g_replaySystem.bot = bot;
}

static_function void FreeReplayData(ReplayPlayback *replay)
{
	if (replay->tickdata)
	{
		delete replay->tickdata;
	}
	*replay = {};
}

static_function void MakeBotAlive()
{
	SpawnBots();
	auto bot = g_replaySystem.bot.Get();
	if (!bot)
	{
		return;
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
	KZ::misc::JoinTeam(player, CS_TEAM_CT, false);
}

static_function void MoveBotToSpec()
{
	auto bot = g_replaySystem.bot.Get();
	if (!bot)
	{
		return;
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
	KZ::misc::JoinTeam(player, CS_TEAM_SPECTATOR, false);
}

static_function ReplayPlayback LoadReplay(const char *path)
{
	ReplayPlayback result = {};

	FileHandle_t file = g_pFullFileSystem->Open(path, "rb");
	if (!file)
	{
		return result;
	}

	g_pFullFileSystem->Read(&result.header, sizeof(result.header), file);
	if (result.header.magicNumber != KZ_REPLAY_MAGIC)
	{
		return result;
	}
	if (result.header.version != KZ_REPLAY_VERSION)
	{
		return result;
	}
	g_pFullFileSystem->Read(&result.tickCount, sizeof(result.tickCount), file);
	result.tickdata = new Tickdata[result.tickCount];
	for (i32 i = 0; i < result.tickCount; i++)
	{
		Tickdata tickdata = {};
		g_pFullFileSystem->Read(&tickdata, sizeof(tickdata), file);
		result.tickdata[i] = tickdata;
	}
	result.valid = true;
	g_pFullFileSystem->Close(file);
	return result;
}

void KZ::replaysystem::Init()
{
	
}

void KZ::replaysystem::Cleanup()
{
	KickBots();
}

void KZ::replaysystem::OnRoundStart()
{
	KickBots();
	SpawnBots();
}

void KZ::replaysystem::FinishMovePre(KZPlayer *player, CMoveData *mv)
{
	if (!player || g_replaySystem.bot != player->GetController() || !mv->m_bFirstRunOfFunctions)
	{
		return;
	}
	
	ReplayPlayback *replay = &g_replaySystem.currentReplay;
	
	if (!replay->playingReplay)
	{
		return;
	}
	Tickdata *tickdata = &replay->tickdata[replay->currentTick];
	
	mv->m_vecAbsOrigin = tickdata->origin;
	mv->m_vecAbsViewAngles = tickdata->angles;
	mv->m_vecViewAngles = tickdata->angles;
	mv->m_vecVelocity = tickdata->velocity;
	
	auto moveServices = player->GetMoveServices();
	moveServices->m_flDuckSpeed = tickdata->duckSpeed;
	moveServices->m_flDuckAmount = tickdata->duckAmount;
	moveServices->m_bDucking = tickdata->replayFlags.ducking;
	moveServices->m_bDucked = tickdata->replayFlags.ducked;
	moveServices->m_nButtons()->m_pButtonStates[0] = tickdata->buttons;
	
	i32 playerFlagBits =  ((1 << (i32)PLAYER_FLAG_BITS) - 1) & ~((i32)(FL_CLIENT | FL_PAWN_FAKECLIENT | FL_CONTROLLER_FAKECLIENT));
	auto pawn = player->GetPlayerPawn();
	pawn->m_fFlags = (pawn->m_fFlags & ~playerFlagBits) | (tickdata->entityFlags & playerFlagBits);
	pawn->v_angle = tickdata->angles;
	
	replay->currentTick++;
	if (replay->currentTick >= replay->tickCount)
	{
		MoveBotToSpec();
		replay->playingReplay = false;
	}
}

SCMD(kz_replay, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!g_pFullFileSystem || !player)
	{
		return MRES_SUPERCEDE;
	}
	
	char replayPath[128];
	V_snprintf(replayPath, sizeof(replayPath), KZ_REPLAY_RUNS_PATH "/%s", g_pKZUtils->GetCurrentMapName().Get());

	auto modeInfo = KZ::mode::GetModeInfo(player->modeService);

	i32 courseId = INVALID_COURSE_NUMBER;
	const KZCourseDescriptor *course = player->timerService->GetCourse();
	if (course)
	{
		courseId = course->id;
	}
	else
	{
		player->languageService->PrintChat(true, false, "No replay course found");
		return MRES_SUPERCEDE;
	}

	char filename[512];
	V_snprintf(filename, sizeof(filename), "%s/%i_%s_%s.replay", replayPath, courseId, modeInfo.shortModeName.Get(),
			   player->timerService->GetCurrentTimeType() == KZTimerService::TimeType_Pro ? "PRO" : "TP");

	ReplayPlayback replay = LoadReplay(filename);
	if (!replay.valid)
	{
		player->languageService->PrintChat(true, false, "No replay for course", course->name);
		return MRES_SUPERCEDE;
	}
	
	MakeBotAlive();
	FreeReplayData(&g_replaySystem.currentReplay);
	g_replaySystem.currentReplay = replay;
	g_replaySystem.currentReplay.playingReplay = true;
	g_replaySystem.currentReplay.currentTick = 0;
	
	return MRES_SUPERCEDE;
}
