#include "kz_recording.h"
#include "utils/simplecmds.h"
#include "kz/timer/kz_timer.h"
#include "filesystem.h"
#include "cs2kz.h"
#include "utils/ctimer.h"
#include "kz/kz.h"
#include "common.h"
#include "sdk/cskeletoninstance.h"
#include "sdk/usercmd.h"

extern CConVar<bool> kz_replay_recording_debug;

ManualRecorder::ManualRecorder(KZPlayer *player, f32 duration, KZPlayer *savedBy) : Recorder(player, duration, true, DistanceTier_None)
{
	this->baseHeader.type = RP_MANUAL;
	if (savedBy)
	{
		V_strncpy(this->header.savedBy.name, savedBy->GetName(), sizeof(this->header.savedBy.name));
		this->header.savedBy.steamid64 = savedBy->GetSteamId64();
	}
}

i32 ManualRecorder::WriteHeader(FileHandle_t file)
{
	i32 bytesWritten = Recorder::WriteHeader(file);
	return bytesWritten + g_pFullFileSystem->Write(&this->header, sizeof(this->header), file);
}

CheaterRecorder::CheaterRecorder(KZPlayer *player, const char *reason, KZPlayer *savedBy) : Recorder(player, 120.0f, true, DistanceTier_None)
{
	this->baseHeader.type = RP_CHEATER;
	V_strncpy(this->header.reason, reason, sizeof(this->header.reason));
	if (savedBy)
	{
		V_strncpy(this->header.reporter.name, savedBy->GetName(), sizeof(this->header.reporter.name));
		this->header.reporter.steamid64 = savedBy->GetSteamId64();
	}
}

i32 CheaterRecorder::WriteHeader(FileHandle_t file)
{
	i32 bytesWritten = Recorder::WriteHeader(file);
	return bytesWritten + g_pFullFileSystem->Write(&this->header, sizeof(this->header), file);
}

// JumpRecorder Implementation
JumpRecorder::JumpRecorder(Jump *jump) : Recorder(jump->player, 5.0f, false, DistanceTier_None)
{
	this->desiredStopTime = g_pKZUtils->GetServerGlobals()->curtime + 2.0f;
	this->baseHeader.type = RP_JUMPSTATS;
	V_strncpy(this->header.mode.name, KZ::mode::GetModeInfo(jump->player->modeService).longModeName.Get(), sizeof(this->header.mode.name));
	V_strncpy(this->header.mode.shortName, KZ::mode::GetModeInfo(jump->player->modeService).shortModeName.Get(), sizeof(this->header.mode.shortName));
	V_strncpy(this->header.mode.md5, KZ::mode::GetModeInfo(jump->player->modeService).md5, sizeof(this->header.mode.md5));
	this->header.jumpType = jump->jumpType;
	this->header.distance = jump->GetDistance();
	this->header.blockDistance = -1;
	this->header.numStrafes = jump->strafes.Count();
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

// RunRecorder Implementation
RunRecorder::RunRecorder(KZPlayer *player) : Recorder(player, 5.0f, true, DistanceTier_Ownage)
{
	this->baseHeader.type = RP_RUN;
	V_strncpy(this->header.courseName, player->timerService->GetCourse()->GetName().Get(), sizeof(this->header.courseName));
	V_strncpy(this->header.mode.name, KZ::mode::GetModeInfo(player->modeService).longModeName.Get(), sizeof(this->header.mode.name));
	V_strncpy(this->header.mode.shortName, KZ::mode::GetModeInfo(player->modeService).shortModeName.Get(), sizeof(this->header.mode.shortName));
	V_strncpy(this->header.mode.md5, KZ::mode::GetModeInfo(player->modeService).md5, sizeof(this->header.mode.md5));
	FOR_EACH_VEC(player->styleServices, i)
	{
		auto styleInfo = KZ::style::GetStyleInfo(player->styleServices[i]);
		RpModeStyleInfo style = {};
		V_strncpy(style.name, styleInfo.longName, sizeof(style.name));
		V_strncpy(style.shortName, styleInfo.shortName, sizeof(style.shortName));
		V_strncpy(style.md5, styleInfo.md5, sizeof(style.md5));
		this->styles.push_back(style);
	}
	this->header.styleCount = player->styleServices.Count();
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
	for (auto &style : styles)
	{
		bytesWritten += g_pFullFileSystem->Write(&style, sizeof(style), file);
	}
	return bytesWritten + g_pFullFileSystem->Write(&this->header, sizeof(this->header), file);
}

// Recorder Implementation
Recorder::Recorder(KZPlayer *player, f32 numSeconds, bool copyTimerEvents, DistanceTier copyJumps)
{
	baseHeader.Init(player);

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

bool Recorder::WriteToFile()
{
	// Update the replay timestamp before writing.
	time_t unixTime = 0;
	time(&unixTime);
	this->baseHeader.timestamp = (u64)unixTime;

	char filename[512];
	std::string uuidStr = this->uuid.ToString();
	V_snprintf(filename, sizeof(filename), "%s/%s.replay", KZ_REPLAY_PATH, uuidStr.c_str());
	g_pFullFileSystem->CreateDirHierarchy(KZ_REPLAY_PATH);
	FileHandle_t file = g_pFullFileSystem->Open(filename, "wb");
	if (!file)
	{
		META_CONPRINTF("Failed to open replay file for writing: %s\n", filename);
		return false;
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
		META_CONPRINTF("kz_replay_recording_debug: Saved replay to %s (%d bytes)\n", filename, bytesWritten);
	}
	g_pFullFileSystem->Close(file);
	return true;
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
		META_CONPRINTF("kz_replay_recording_debug: Wrote %d tick data entries of %d bytes\n", tickdataCount, numWritten);
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
		META_CONPRINTF("kz_replay_recording_debug: Wrote %d weapon change events of %d bytes\n", numWeaponChanges, numWritten);
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
		META_CONPRINTF("kz_replay_recording_debug: Wrote %d jumps of %d bytes\n", numJumps, numWritten);
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
		META_CONPRINTF("kz_replay_recording_debug: Wrote %d events of %d bytes\n", numEvents, numWritten);
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
		META_CONPRINTF("kz_replay_recording_debug: Wrote %d cmd data entries of %d bytes\n", cmddataCount, numWritten);
	}
	return numWritten;
}
