#include "cs2kz.h"
#include "kz_recording.h"
#include "kz/timer/kz_timer.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/language/kz_language.h"
#include "kz/spec/kz_spec.h"
#include "utils/simplecmds.h"

#include "sdk/cskeletoninstance.h"
#include "sdk/usercmd.h"

#include "filesystem.h"
#include "vprof.h"

CConVar<bool> kz_replay_recording_debug("kz_replay_recording_debug", FCVAR_NONE, "Debug replay recording", true);
CConVar<i32> kz_replay_recording_min_jump_tier("kz_replay_recording_min_jump_tier", FCVAR_CHEAT, "Minimum jump tier to record for jumpstat replays",
											   DistanceTier_Wrecker, true, DistanceTier_Meh, true, DistanceTier_Wrecker);

// Not sure what's the best place to put this, so putting it here for now.
void SubtickData::RpSubtickMove::FromMove(const CSubtickMoveStep &move)
{
	this->when = move.when();
	this->button = move.button();
	if (move.button())
	{
		this->pressed = move.pressed();
	}
	else
	{
		this->analogMove.analog_forward_delta = move.analog_forward_delta();
		this->analogMove.analog_left_delta = move.analog_left_delta();
		this->analogMove.pitch_delta = move.pitch_delta();
		this->analogMove.yaw_delta = move.yaw_delta();
	}
}

void GeneralReplayHeader::Init(KZPlayer *player)
{
	// Non-player-specific fields
	this->magicNumber = KZ_REPLAY_MAGIC;
	this->version = KZ_REPLAY_VERSION;

	V_snprintf(this->map.name, sizeof(this->map.name), "%s", g_pKZUtils->GetCurrentMapName().Get());
	g_pKZUtils->GetCurrentMapMD5(this->map.md5, sizeof(this->map.md5));
	V_snprintf(this->pluginVersion, sizeof(this->pluginVersion), "%s", g_KZPlugin.GetVersion());
	this->serverVersion = utils::GetServerVersion();

	// Player-specific fields
	V_snprintf(this->player.name, sizeof(this->player.name), "%s", player->GetController()->GetPlayerName());
	this->player.steamid64 = player->GetSteamId64();

	this->firstWeapon = player->recordingService->circularRecording.earliestWeapon.value();

	this->gloves = player->GetPlayerPawn()->m_EconGloves();
	CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(player->GetPlayerPawn()->m_CBodyComponent()->m_pSceneNode());
	V_snprintf(this->modelName, sizeof(this->modelName), "%s", pSkeleton->m_modelState().m_ModelName().String());

	this->sensitivity = utils::StringToFloat(interfaces::pEngine->GetClientConVarValue(player->GetPlayerSlot(), "sensitivity"));
	this->yaw = utils::StringToFloat(interfaces::pEngine->GetClientConVarValue(player->GetPlayerSlot(), "m_yaw"));
	this->pitch = utils::StringToFloat(interfaces::pEngine->GetClientConVarValue(player->GetPlayerSlot(), "m_pitch"));
}

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

void KZRecordingService::RecordTickData_PhysicsSimulate()
{
	// Reset the tick data.
	this->currentTickData = {};
	this->currentSubtickData = {};

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

void KZRecordingService::RecordTickData_SetupMove(PlayerCommand *pc)
{
	this->currentTickData.cmdNumber = pc->cmdNum;
	this->currentTickData.clientTick = pc->base().client_tick();
	this->currentTickData.forward = pc->base().forwardmove();
	this->currentTickData.left = pc->base().leftmove();
	this->currentTickData.up = pc->base().upmove();
	this->currentTickData.pre.angles = {pc->base().viewangles().x(), pc->base().viewangles().y(), pc->base().viewangles().z()};
	this->currentTickData.leftHanded = this->player->GetPlayerPawn()->m_bLeftHanded() || pc->left_hand_desired();

	this->currentSubtickData.numSubtickMoves = pc->base().subtick_moves_size();
	for (u32 i = 0; i < this->currentSubtickData.numSubtickMoves && i < 64; i++)
	{
		this->currentSubtickData.subtickMoves[i].FromMove(pc->base().subtick_moves(i));
	}
}

void KZRecordingService::RecordTickData_PhysicsSimulatePost()
{
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

	// Push the tick data to the circular buffer and recorders.
	this->circularRecording.tickData->Write(this->currentTickData);
	this->PushToRecorders(this->currentTickData, RecorderType::Both);

	this->circularRecording.subtickData->Write(this->currentSubtickData);
	this->PushToRecorders<Recorder::Vec::Tick>(this->currentSubtickData, RecorderType::Both);
}

void KZRecordingService::RecordCommand(PlayerCommand *cmds, i32 numCmds)
{
	i32 currentTick = g_pKZUtils->GetServerGlobals()->tickcount;
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
		this->PushToRecorders(data, RecorderType::Both);

		SubtickData subtickData;
		subtickData.numSubtickMoves = pc.base().subtick_moves_size();
		for (u32 j = 0; j < subtickData.numSubtickMoves && j < 64; j++)
		{
			subtickData.subtickMoves[j].FromMove(pc.base().subtick_moves(j));
		}
		this->circularRecording.cmdSubtickData->Write(subtickData);
		this->PushToRecorders<Recorder::Vec::Cmd>(subtickData, RecorderType::Both);
		this->lastCmdNumReceived = pc.cmdNum;
	}
}

void KZRecordingService::CheckRecorders()
{
	for (auto it = this->runRecorders.begin(); it != this->runRecorders.end();)
	{
		auto &recorder = *it;
		if (recorder.ShouldStopAndSave(g_pKZUtils->GetServerGlobals()->curtime))
		{
			// Stop this recorder
			if (kz_replay_recording_debug.Get())
			{
				META_CONPRINTF("kz_replay_recording_debug: Run recorder stopped\n");
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
		if (recorder.ShouldStopAndSave(g_pKZUtils->GetServerGlobals()->curtime))
		{
			// Stop this recorder
			if (kz_replay_recording_debug.Get())
			{
				META_CONPRINTF("kz_replay_recording_debug: Jump recorder stopped\n");
			}
			recorder.WriteToFile();
			it = this->jumpRecorders.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void KZRecordingService::CheckMouseConVars()
{
	const char *sensitivityStr = interfaces::pEngine->GetClientConVarValue(this->player->GetPlayerSlot(), "sensitivity");
	f32 sensitivity = utils::StringToFloat(sensitivityStr);
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
			META_CONPRINTF("kz_replay_recording_debug: Sensitivity change event: %f\n", sensitivity);
		}
	}

	const char *pitchStr = interfaces::pEngine->GetClientConVarValue(this->player->GetPlayerSlot(), "m_pitch");
	f32 pitch = utils::StringToFloat(pitchStr);
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
			META_CONPRINTF("kz_replay_recording_debug: Pitch change event: %f\n", pitch);
		}
	}

	const char *yawStr = interfaces::pEngine->GetClientConVarValue(this->player->GetPlayerSlot(), "m_yaw");
	f32 yaw = utils::StringToFloat(yawStr);
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
			META_CONPRINTF("kz_replay_recording_debug: Yaw change event: %f\n", yaw);
		}
	}
}

void KZRecordingService::CheckWeapons()
{
	auto weapon = this->player->GetPlayerPawn()->m_pWeaponServices()->m_hActiveWeapon().Get();
	auto weaponEconInfo = EconInfo(weapon);
	if (this->currentWeaponEconInfo != weaponEconInfo)
	{
		this->currentWeaponEconInfo = weaponEconInfo;
		WeaponSwitchEvent event;
		event.serverTick = this->currentTickData.serverTick;
		event.econInfo = weaponEconInfo;
		this->circularRecording.weaponChangeEvents->Write(event);
		this->PushToRecorders(event, RecorderType::Both);
	}
	if (!this->circularRecording.earliestWeapon.has_value())
	{
		this->circularRecording.earliestWeapon = this->currentWeaponEconInfo;
	}
}

void KZRecordingService::CheckModeStyles()
{
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
			META_CONPRINTF("kz_replay_recording_debug: Mode change event: %s\n", currentModeInfo.longModeName.Get());
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
			META_CONPRINTF("kz_replay_recording_debug: Style change event: %u styles\n", (unsigned int)this->lastKnownStyles.size());
		}
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
}

void KZRecordingService::CheckCheckpoints()
{
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
			META_CONPRINTF("kz_replay_recording_debug: Checkpoint event: #%d CP %d, teleports %d\n", this->lastKnownCheckpointIndex,
						   this->lastKnownCheckpointCount, this->lastKnownTeleportCount);
		}
	}

	if (!this->circularRecording.earliestCheckpointCount.has_value())
	{
		this->circularRecording.earliestCheckpointCount = this->lastKnownCheckpointCount;
		this->circularRecording.earliestTeleportCount = this->lastKnownTeleportCount;
		this->circularRecording.earliestCheckpointIndex = this->lastKnownCheckpointIndex;
	}
}

void KZRecordingService::InsertEvent(const RpEvent &event)
{
	this->circularRecording.rpEvents->Write(event);
	this->PushToRecorders(event, RecorderType::Run);
	if (event.type != RpEventType::RPEVENT_TIMER_EVENT)
	{
		this->PushToRecorders(event, RecorderType::Jump);
	}
}

void KZRecordingService::InsertTimerEvent(RpEvent::RpEventData::TimerEvent::TimerEventType type, f32 time, i32 index)
{
	RpEvent event;
	event.serverTick = this->currentTickData.serverTick;
	event.type = RpEventType::RPEVENT_TIMER_EVENT;
	event.data.timer.type = type;
	event.data.timer.index = index;
	event.data.timer.time = time;
	this->InsertEvent(event);
}

void KZRecordingService::InsertTeleportEvent(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	RpEvent event;
	event.serverTick = this->currentTickData.serverTick;
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

void KZRecordingService::InsertModeChangeEvent(const char *name, const char *md5)
{
	RpEvent event;
	event.serverTick = this->currentTickData.serverTick;
	event.type = RpEventType::RPEVENT_MODE_CHANGE;
	V_strncpy(event.data.modeChange.name, name, sizeof(event.data.modeChange.name));
	V_strncpy(event.data.modeChange.md5, md5, sizeof(event.data.modeChange.md5));
	this->InsertEvent(event);
}

void KZRecordingService::InsertStyleChangeEvent(const char *name, const char *md5, bool firstStyle)
{
	RpEvent event;
	event.serverTick = this->currentTickData.serverTick;
	event.type = RpEventType::RPEVENT_STYLE_CHANGE;
	V_strncpy(event.data.styleChange.name, name, sizeof(event.data.styleChange.name));
	V_strncpy(event.data.styleChange.md5, md5, sizeof(event.data.styleChange.md5));
	event.data.styleChange.clearStyles = firstStyle;
	this->InsertEvent(event);
}

void KZRecordingService::InsertCheckpointEvent(i32 index, i32 checkpointCount, i32 teleportCount)
{
	RpEvent event;
	event.serverTick = this->currentTickData.serverTick;
	event.type = RpEventType::RPEVENT_CHECKPOINT;
	event.data.checkpoint.index = index;
	event.data.checkpoint.checkpointCount = checkpointCount;
	event.data.checkpoint.teleportCount = teleportCount;
	this->InsertEvent(event);
}

void KZRecordingService::InsertCvarEvent(RpCvar cvar, f32 value)
{
	RpEvent event;
	event.serverTick = this->currentTickData.serverTick;
	event.type = RpEventType::RPEVENT_CVAR;
	event.data.cvar.cvar = cvar;
	this->InsertEvent(event);
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

SCMD(kz_rpsave, SCFL_REPLAY)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!g_pFullFileSystem || !player)
	{
		return MRES_SUPERCEDE;
	}

	f32 duration = args->ArgC() > 1 ? utils::StringToFloat(args->Arg(1)) : 120.0f;
	std::string uuid;
	KZPlayer *target = player->IsAlive() ? player : player->specService->GetSpectatedPlayer();
	if (!target)
	{
		return MRES_SUPERCEDE;
	}
	duration = target->recordingService->WriteCircularBufferToFile(duration, "", &uuid);
	if (uuid.empty())
	{
		player->languageService->PrintChat(true, false, "Replay - Manual Replay Failed");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Replay - Manual Replay Saved", uuid.c_str(), duration);
	}
	return MRES_SUPERCEDE;
}
