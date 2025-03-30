#include "kz_replay.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "sdk/usercmd.h"

static_global class KZJumpstatsServiceEventListener_Replay : public KZJumpstatsServiceEventListener
{
public:
	virtual void OnJumpStart(const Jump *jump) override
	{
		KZPlayer *player = jump->GetJumpPlayer();
		player->replayService->OnJumpStart(jump);
	}
} jumpstatsEventListener;

void KZReplayService::Init()
{
	KZJumpstatsService::RegisterEventListener(&jumpstatsEventListener);
}

ReplayRecorder *KZReplayService::CreateRecorder()
{
	this->recorders.push_back(new ReplayRecorder());
	ReplayRecorder *recorder = this->recorders.back();
	ReplayHeader &header = recorder->rpData->header;
	header.version = 0;

	// Player
	V_strncpy(header.player.name, this->player->GetName(), sizeof(this->player->GetName()));
	header.player.steamid64 = this->player->GetSteamId64();

	// Note: Mode and style informations in the header aren't used for the replay playback.

	// Mode
	auto modeInfo = KZ::mode::GetModeInfo(this->player->modeService);

	header.mode.name = modeInfo.longModeName;
	V_strncpy(header.mode.md5, modeInfo.md5, sizeof(header.mode.md5));

	// Style
	FOR_EACH_VEC(this->player->styleServices, i)
	{
		KZStyleManager::StylePluginInfo info = KZ::style::GetStyleInfo(this->player->styleServices[i]);

		ReplayHeader::StyleInfo &styleInfo = header.styles.emplace_back();
		styleInfo.name = info.longName;
		V_strncpy(styleInfo.md5, info.md5, sizeof(styleInfo.md5));
	}

	// Map
	V_strncpy(header.map.name, g_pKZUtils->GetCurrentMapName().Get(), sizeof(header.map.name));
	g_pKZUtils->GetCurrentMapMD5(header.map.md5, sizeof(header.map.md5));

	return recorder;
}

ReplayRecorder *KZReplayService::CreateRunRecorder()
{
	return nullptr;
}

void KZReplayService::OnTimerStart() {}

ReplayRecorder *KZReplayService::CreateJumpstatRecorder(ReplayEvent *jsEvent)
{
	ReplayRecorder *recorder = this->CreateRecorder();
	// Record 5 seconds of jumpstats
	recorder->PushBreather(this->circularRecorder, 192);
	recorder->idealEndTick = g_pKZUtils->GetGlobals()->tickcount + 128;
	u32 eventIndex;

	// There should always be at least the jumpstat event itself here.
	assert(recorder->rpData->events.size() > 0);
	for (ReplayEvent event : recorder->rpData->events)
	{
		if (event.guid == jsEvent->guid)
		{
			break;
		}
		eventIndex++;
	}
	recorder->rpData->header.typeSpecificHeader = JumpstatReplayHeader({eventIndex});

	return recorder;
}

ReplayRecorder *KZReplayService::CreateCheaterRecorder(u32 reason)
{
	return nullptr;
}

ReplayRecorder *KZReplayService::CreateManualRecorder(const std::string &name)
{
	return nullptr;
}

void KZReplayService::CheckRecorders()
{
	if (this->markedJumpstatEvent)
	{
		this->CreateJumpstatRecorder(this->markedJumpstatEvent);
		this->markedJumpstatEvent = nullptr;
	}
	this->circularRecorder.CheckEvents();
	for (ReplayRecorder *recorder : this->recorders)
	{
		if (g_pKZUtils->GetGlobals()->tickcount >= recorder->idealEndTick)
		{
			// TODO: this should run on another thread and make use of mutex there
			recorder->SaveToFile();
			this->recorders.erase(std::remove(this->recorders.begin(), this->recorders.end(), recorder), this->recorders.end());
			this->finishedRecorders.push_back(recorder);
			recorder->keepData = true;
			this->cachedReplays.push_back(recorder->rpData);
		}
	}
}

void KZReplayService::OnCreateMovePost(CUserCmd *cmd, CMoveData *mv)
{
	if (mv->m_bHasZeroFrametime)
	{
		return;
	}
	CCSPlayer_MovementServices *ms = this->player->GetMoveServices();
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	memcpy(this->currentTickData.initialButtons, ms->m_nButtons()->m_pButtonStates(), 24);
	this->currentTickData.forwardmove = mv->m_flForwardMove;
	this->currentTickData.sidemove = mv->m_flSideMove;
	this->currentTickData.upmove = mv->m_flUpMove;
	this->player->GetOrigin(&this->currentTickData.preOrigin);
	this->player->GetAngles(&this->currentTickData.preAngles);
	this->player->GetVelocity(&this->currentTickData.preVelocity);
	this->currentTickData.crouchState = ms->m_nCrouchState();
	this->currentTickData.crouchTransitionStartTime = ms->m_flCrouchTransitionStartTime();
	this->currentTickData.jumpPressedTime = ms->m_flJumpPressedTime();
	this->currentTickData.lastDuckTime = ms->m_flLastDuckTime();
	this->currentTickData.moveType = this->player->GetPlayerPawn()->m_MoveType();
	this->currentTickData.numSubtickMoves = mv->m_SubtickMoves.Count() - 1;
	FOR_EACH_VEC(mv->m_SubtickMoves, i)
	{
		if (i == mv->m_SubtickMoves.Count() - 1)
		{
			break;
		}
		this->currentSubtickMoves[i] = SubtickMoveLite::FromSubtickMove(mv->m_SubtickMoves[i]);
	}
	this->currentTickData.flags |= (pawn->m_fFlags() & FL_ONGROUND) ? REPLAYTICK_PRE_FL_ONGROUND : 0;
	this->currentTickData.flags |= ms->m_bInCrouch() ? REPLAYTICK_PRE_IN_CROUCH : 0;
	this->currentTickData.flags |= ms->m_bDucked() ? REPLAYTICK_PRE_DUCKED : 0;
	this->currentTickData.flags |= ms->m_bDucking() ? REPLAYTICK_PRE_DUCKING : 0;
	this->currentTickData.flags |= ms->m_bOldJumpPressed() ? REPLAYTICK_PRE_OLD_JUMP_PRESSED : 0;
	this->currentTickData.flags |= (pawn->m_fFlags() & FL_BASEVELOCITY) ? REPLAYTICK_PRE_FL_BASEVELOCITY : 0;
	this->currentTickData.flags |= (pawn->m_fFlags() & FL_CONVEYOR_NEW) ? REPLAYTICK_PRE_FL_CONVEYOR_NEW : 0;
}

void KZReplayService::OnPhysicsSimulatePost()
{
	CCSPlayer_MovementServices *ms = this->player->GetMoveServices();
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	this->player->GetOrigin(&this->currentTickData.postOrigin);
	this->player->GetAngles(&this->currentTickData.postAngles);
	this->player->GetVelocity(&this->currentTickData.postVelocity);
	this->currentTickData.flags |= (pawn->m_fFlags() & FL_ONGROUND) ? REPLAYTICK_POST_FL_ONGROUND : 0;
	this->currentTickData.flags |= ms->m_bDucked() ? REPLAYTICK_POST_DUCKED : 0;
	this->currentTickData.flags |= ms->m_bDucking() ? REPLAYTICK_POST_DUCKING : 0;
	this->currentTickData.flags |= (pawn->m_fFlags() & FL_BASEVELOCITY) ? REPLAYTICK_POST_FL_BASEVELOCITY : 0;
	this->currentTickData.flags |= (pawn->m_fFlags() & FL_CONVEYOR_NEW) ? REPLAYTICK_POST_FL_CONVEYOR_NEW : 0;
	this->circularRecorder.Push(this->currentTickData, this->currentSubtickMoves);
}

void KZReplayService::OnGameFramePost()
{
	for (i32 i = 0; i <= g_pKZUtils->GetGlobals()->maxClients; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		player->replayService->CheckRecorders();
	}
}

void KZReplayService::OnTimerStop()
{
	// TODO: Stop active run recorder
}

void KZReplayService::OnClientDisconnect()
{
	// TODO: Save or delete all active recordings
}

void KZReplayService::OnJumpStart(const Jump *jump)
{
	// TODO: Save takeoff event
}

void KZReplayService::OnJumpEnd(const Jump *jump)
{
	ReplayEvent event;
	event.data = JumpstatEventData();
	std::get<JumpstatEventData>(event.data).FromJump(jump);
	// Do not create a new recorder for jump event here, because this can happen in the middle of a tick.
	// Instead, mark the player as needing a new jumpstat replay, and create a new replay at the end of a frame.
	// Technically, if the player hits two godlike jumps in a single tick, this will only save the last one.
	// However that shouldn't happen in the first place.
	if (jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance()) >= DistanceTier_Godlike)
	{
		this->markedJumpstatEvent = this->circularRecorder.PushEvent(event);
	}
}

void JumpstatEventData::FromJump(const Jump *jump)
{
	// TODO: This is very ugly.
	this->takeoffOrigin = jump->takeoffOrigin;
	this->adjustedTakeoffOrigin = jump->adjustedTakeoffOrigin;
	this->takeoffVelocity = jump->takeoffVelocity;
	this->landingOrigin = jump->landingOrigin;
	this->adjustedLandingOrigin = jump->adjustedLandingOrigin;
	this->type = jump->GetJumpType();
	this->release = jump->release;
	this->totalDistance = jump->totalDistance;
	this->block = jump->block;
	this->edge = jump->edge;
	this->miss = jump->miss;
	this->duckDuration = jump->duckDuration;
	this->duckEndDuration = jump->duckEndDuration;

	this->strafes.reserve(jump->strafes.Count());
	for (i32 i = 0; i < jump->strafes.Count(); ++i)
	{
		this->strafes[i].aaCalls.reserve(jump->strafes[i].aaCalls.Count());
		for (u32 j = 0; j < jump->strafes[i].aaCalls.Count(); ++j)
		{
			const AACall &call = jump->strafes[i].aaCalls[j];
			this->strafes[i].aaCalls[j] = {
				call.externalSpeedDiff, call.prevYaw,      call.currentYaw,      call.wishdir,  call.maxspeed,
				call.wishspeed,         call.accel,        call.surfaceFriction, call.duration, {call.buttons[0], call.buttons[1], call.buttons[2]},
				call.velocityPre,       call.velocityPost, call.ducking};
		}
	}
}
