
#include "kz/kz.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "kz/language/kz_language.h"
#include "kz/noclip/kz_noclip.h"
#include "kz_replay.h"
#include "kz_replaysystem.h"
#include "utils/simplecmds.h"
#include "sdk/usercmd.h"
#include "utlvector.h"
#include "filesystem.h"

struct ReplayPlayback
{
	bool valid;
	ReplayHeader header;
	i32 tickCount;
	TickData *tickData;
	i32 subtickDataCount;
	SubtickData *subtickData;
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
	BotProfile *profile = BotProfile::PersistentProfile(CS_TEAM_T);
	if (!profile)
	{
		return;
	}
	CCSPlayerController *bot = g_pKZUtils->CreateBot(profile, CS_TEAM_T);
	if (!bot)
	{
		return;
	}

	g_replaySystem.bot = bot;
}

static_function void FreeReplayData(ReplayPlayback *replay)
{
	if (replay->tickData)
	{
		delete replay->tickData;
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
	player->SetName("REPLAY");
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
	result.tickData = new TickData[result.tickCount];
	for (i32 i = 0; i < result.tickCount; i++)
	{
		TickData tickData = {};
		g_pFullFileSystem->Read(&tickData, sizeof(tickData), file);
		result.tickData[i] = tickData;
	}
	result.subtickData = new SubtickData[result.tickCount];
	for (i32 i = 0; i < result.tickCount; i++)
	{
		SubtickData subtickData = {};
		g_pFullFileSystem->Read(&subtickData.numSubtickMoves, sizeof(subtickData.numSubtickMoves), file);
		g_pFullFileSystem->Read(&subtickData.subtickMoves, sizeof(SubtickData::RpSubtickMove) * subtickData.numSubtickMoves, file);
		result.subtickData[i] = subtickData;
	}
	result.valid = true;
	g_pFullFileSystem->Close(file);
	return result;
}

void KZ::replaysystem::Init() {}

void KZ::replaysystem::Cleanup()
{
	KickBots();
}

void KZ::replaysystem::OnRoundStart()
{
	KickBots();
	SpawnBots();
}

void KZ::replaysystem::OnPhysicsSimulate(KZPlayer *player)
{
	if (!player || g_replaySystem.bot != player->GetController())
	{
		return;
	}

	ReplayPlayback *replay = &g_replaySystem.currentReplay;

	if (!replay->playingReplay)
	{
		return;
	}
	TickData *tickData = &replay->tickData[replay->currentTick];

	auto pawn = player->GetPlayerPawn();
	// Setting the origin via teleport will break client interp, set the values directly
	pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(tickData->pre.origin);
	pawn->m_angEyeAngles(tickData->pre.angles);
	pawn->m_vecAbsVelocity(tickData->pre.velocity);

	auto moveServices = player->GetMoveServices();
	moveServices->m_flJumpPressedTime = tickData->pre.jumpPressedTime;

	moveServices->m_flDuckSpeed = tickData->pre.duckSpeed;
	moveServices->m_flDuckAmount = tickData->pre.duckAmount;
	moveServices->m_flDuckOffset = tickData->pre.duckOffset;
	moveServices->m_flLastDuckTime = g_pKZUtils->GetServerGlobals()->curtime + tickData->pre.lastDuckTime - tickData->gameTime;
	moveServices->m_bDucking = tickData->pre.replayFlags.ducking;
	moveServices->m_bDucked = tickData->pre.replayFlags.ducked;
	moveServices->m_bDesiresDuck = tickData->pre.replayFlags.desiresDuck;

	u32 playerFlagBits = (-1) & ~((u32)(FL_CLIENT | FL_BOT));
	pawn->m_fFlags = (pawn->m_fFlags & ~playerFlagBits) | (tickData->pre.entityFlags & playerFlagBits);
	pawn->v_angle = tickData->pre.angles;
}

void KZ::replaysystem::OnProcessMovement(KZPlayer *player)
{
	if (!player || g_replaySystem.bot != player->GetController())
	{
		return;
	}
	ReplayPlayback *replay = &g_replaySystem.currentReplay;

	if (!replay->playingReplay)
	{
		return;
	}
	auto pawn = player->GetPlayerPawn();
	u32 newFlags = pawn->m_fFlags() | FL_BOT;
	pawn->m_fFlags(newFlags);
}

void KZ::replaysystem::OnDuck(KZPlayer *player)
{
	if (!player || g_replaySystem.bot != player->GetController())
	{
		return;
	}
	ReplayPlayback *replay = &g_replaySystem.currentReplay;

	if (!replay->playingReplay)
	{
		return;
	}

	auto pawn = player->GetPlayerPawn();
	auto ms = player->GetMoveServices();
	META_CONPRINTF("m_duckUntilOnGround = %i, FL_DUCKING = %s, m_flDuckAmount = %f, m_bDucking = %s, m_bDucked = %s, IN_DUCK = %i, curtime - "
				   "lastDuckTime = %f\n",
				   ms->m_duckUntilOnGround, pawn->m_fFlags() & FL_DUCKING ? "true" : "false", ms->m_flDuckAmount, ms->m_bDucking ? "true" : "false",
				   ms->m_bDucked ? "true" : "false", player->IsButtonPressed(IN_DUCK, true),
				   g_pKZUtils->GetGlobals()->curtime - ms->m_flLastDuckTime);
}

void KZ::replaysystem::OnProcessMovementPost(KZPlayer *player)
{
	if (!player || g_replaySystem.bot != player->GetController())
	{
		return;
	}
	ReplayPlayback *replay = &g_replaySystem.currentReplay;

	if (!replay->playingReplay)
	{
		return;
	}
	auto pawn = player->GetPlayerPawn();
	u32 newFlags = pawn->m_fFlags() | FL_BOT;
	pawn->m_fFlags(newFlags);
}

void KZ::replaysystem::OnPhysicsSimulatePost(KZPlayer *player)
{
	if (!player || g_replaySystem.bot != player->GetController())
	{
		return;
	}
	ReplayPlayback *replay = &g_replaySystem.currentReplay;

	if (!replay->playingReplay)
	{
		return;
	}
	auto pawn = player->GetPlayerPawn();
	TickData *tickData = &replay->tickData[replay->currentTick];
#if 0
	tickData->PrintDebug();
#endif
	// Setting the origin via teleport will break client interp, set the values directly
	pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin(tickData->post.origin);
	pawn->m_angEyeAngles(tickData->post.angles);
	pawn->m_vecAbsVelocity(tickData->post.velocity);

	auto moveServices = player->GetMoveServices();
	moveServices->m_nButtons()->m_pButtonStates[0] = tickData->post.buttons[0];
	moveServices->m_nButtons()->m_pButtonStates[1] = tickData->post.buttons[1];
	moveServices->m_nButtons()->m_pButtonStates[2] = tickData->post.buttons[2];
	moveServices->m_flJumpPressedTime = tickData->post.jumpPressedTime;

	moveServices->m_flDuckSpeed = tickData->post.duckSpeed;
	moveServices->m_flDuckAmount = tickData->post.duckAmount;
	moveServices->m_flDuckOffset = tickData->post.duckOffset;
	moveServices->m_flLastDuckTime = g_pKZUtils->GetServerGlobals()->curtime + tickData->post.lastDuckTime - tickData->gameTime;
	moveServices->m_bDucking = tickData->post.replayFlags.ducking;
	moveServices->m_bDucked = tickData->post.replayFlags.ducked;
	moveServices->m_bDesiresDuck = tickData->post.replayFlags.desiresDuck;

	u32 playerFlagBits = (-1) & ~((u32)(FL_CLIENT | FL_FAKECLIENT | FL_BOT));
	pawn->m_fFlags = (pawn->m_fFlags & ~playerFlagBits) | (tickData->post.entityFlags & playerFlagBits);
	pawn->v_angle = tickData->post.angles;
	replay->currentTick++;
	if (replay->currentTick >= replay->tickCount)
	{
		MoveBotToSpec();
		replay->playingReplay = false;
	}
}

void KZ::replaysystem::OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command)
{
	if (!player || g_replaySystem.bot != player->GetController())
	{
		return;
	}
	ReplayPlayback *replay = &g_replaySystem.currentReplay;

	if (!replay->playingReplay)
	{
		return;
	}
	if (replay->currentTick >= replay->tickCount)
	{
		return;
	}
	TickData *tickData = &replay->tickData[replay->currentTick];
	if (tickData->forward)
	{
		command->mutable_base()->set_forwardmove(tickData->forward);
	}
	if (tickData->left)
	{
		command->mutable_base()->set_leftmove(tickData->left);
	}
	if (tickData->up)
	{
		command->mutable_base()->set_upmove(tickData->up);
	}
	command->mutable_base()->mutable_viewangles()->Clear();
	if (tickData->pre.angles.x != 0)
	{
		command->mutable_base()->mutable_viewangles()->set_x(tickData->pre.angles.x);
	}
	if (tickData->pre.angles.y != 0)
	{
		command->mutable_base()->mutable_viewangles()->set_y(tickData->pre.angles.y);
	}
	if (tickData->pre.angles.z != 0)
	{
		command->mutable_base()->mutable_viewangles()->set_z(tickData->pre.angles.z);
	}
	if (tickData->pre.buttons[0])
	{
		command->mutable_base()->mutable_buttons_pb()->set_buttonstate1(tickData->pre.buttons[0]);
		command->buttonstates.buttons[0] = tickData->pre.buttons[0];
	}
	if (tickData->pre.buttons[1])
	{
		command->mutable_base()->mutable_buttons_pb()->set_buttonstate2(tickData->pre.buttons[1]);
		command->buttonstates.buttons[1] = tickData->pre.buttons[1];
	}
	if (tickData->pre.buttons[2])
	{
		command->mutable_base()->mutable_buttons_pb()->set_buttonstate3(tickData->pre.buttons[2]);
		command->buttonstates.buttons[2] = tickData->pre.buttons[2];
	}
	SubtickData *subtickData = &replay->subtickData[replay->currentTick];

	// Bots should never have any subtick move, but who knows?
	command->mutable_base()->clear_subtick_moves();
	for (u32 i = 0; i < subtickData->numSubtickMoves && i < 64; i++)
	{
		auto move = command->mutable_base()->add_subtick_moves();
		move->set_when(subtickData->subtickMoves[i].when);
		move->set_button(subtickData->subtickMoves[i].button);
		if (!subtickData->subtickMoves[i].IsAnalogInput())
		{
			move->set_pressed(subtickData->subtickMoves[i].pressed);
		}
		else
		{
			if (subtickData->subtickMoves[i].analogMove.analog_forward_delta != 0)
			{
				move->set_analog_forward_delta(subtickData->subtickMoves[i].analogMove.analog_forward_delta);
			}
			if (subtickData->subtickMoves[i].analogMove.analog_left_delta != 0)
			{
				move->set_analog_left_delta(subtickData->subtickMoves[i].analogMove.analog_left_delta);
			}
			if (subtickData->subtickMoves[i].analogMove.analog_pitch_delta != 0)
			{
				move->set_analog_pitch_delta(subtickData->subtickMoves[i].analogMove.analog_pitch_delta);
			}
			if (subtickData->subtickMoves[i].analogMove.analog_yaw_delta != 0)
			{
				move->set_analog_yaw_delta(subtickData->subtickMoves[i].analogMove.analog_yaw_delta);
			}
		}
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
