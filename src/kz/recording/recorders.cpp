#include "kz_recording.h"
#include "utils/simplecmds.h"
#include "kz/timer/kz_timer.h"
#include "filesystem.h"
#include "cs2kz.h"
#include "utils/ctimer.h"
#include "utils/async_file_io.h"
#include "kz/kz.h"
#include "common.h"
#include "sdk/cskeletoninstance.h"
#include "sdk/usercmd.h"
#include "kz/replays/compression.h"

ManualRecorder::ManualRecorder(KZPlayer *player, f32 duration, KZPlayer *savedBy) : Recorder(player, duration, RP_MANUAL, true, DistanceTier_None)
{
	if (savedBy)
	{
		auto *manual = replayHeader.mutable_manual();
		auto *savedByMsg = manual->mutable_saved_by();
		savedByMsg->set_name(savedBy->GetName());
		savedByMsg->set_steamid64(savedBy->GetSteamId64());
	}
}

CheaterRecorder::CheaterRecorder(KZPlayer *player, const char *reason, KZPlayer *savedBy)
	: Recorder(player, 120.0f, RP_CHEATER, true, DistanceTier_None)
{
	auto *cheater = replayHeader.mutable_cheater();
	cheater->set_reason(reason);
	if (savedBy)
	{
		auto *reporter = cheater->mutable_reporter();
		reporter->set_name(savedBy->GetName());
		reporter->set_steamid64(savedBy->GetSteamId64());
	}
}

// JumpRecorder Implementation
JumpRecorder::JumpRecorder(Jump *jump) : Recorder(jump->player, 5.0f, RP_JUMPSTATS, false, DistanceTier_None)
{
	this->desiredStopTime = g_pKZUtils->GetServerGlobals()->curtime + 2.0f;
	auto *jumpProto = replayHeader.mutable_jump();
	auto modeInfo = KZ::mode::GetModeInfo(jump->player->modeService);
	jumpProto->mutable_mode()->set_name(modeInfo.longModeName.Get());
	jumpProto->mutable_mode()->set_short_name(modeInfo.shortModeName.Get());
	jumpProto->mutable_mode()->set_md5(modeInfo.md5);
	jumpProto->set_jump_type(jump->jumpType);
	jumpProto->set_distance(jump->GetDistance());
	jumpProto->set_block_distance(-1);
	jumpProto->set_num_strafes(jump->strafes.Count());
	jumpProto->set_air_time(jump->airtime);
	jumpProto->set_pre(jump->GetTakeoffSpeed());
	jumpProto->set_max(jump->GetMaxSpeed());
	jumpProto->set_sync(jump->GetSync());
}

// RunRecorder Implementation
RunRecorder::RunRecorder(KZPlayer *player) : Recorder(player, 5.0f, RP_RUN, true, DistanceTier_Ownage)
{
	auto *runProto = replayHeader.mutable_run();
	runProto->set_course_name(player->timerService->GetCourse()->GetName().Get());
	auto modeInfo = KZ::mode::GetModeInfo(player->modeService);
	runProto->mutable_mode()->set_name(modeInfo.longModeName.Get());
	runProto->mutable_mode()->set_short_name(modeInfo.shortModeName.Get());
	runProto->mutable_mode()->set_md5(modeInfo.md5);
	FOR_EACH_VEC(player->styleServices, i)
	{
		auto styleInfo = KZ::style::GetStyleInfo(player->styleServices[i]);
		auto *styleMsg = runProto->add_styles();
		styleMsg->set_name(styleInfo.longName);
		styleMsg->set_short_name(styleInfo.shortName);
		styleMsg->set_md5(styleInfo.md5);
	}
}

void RunRecorder::End(f32 time, i32 numTeleports)
{
	auto *runProto = replayHeader.mutable_run();
	runProto->set_time(time);
	runProto->set_num_teleports(numTeleports);
	this->desiredStopTime = g_pKZUtils->GetServerGlobals()->curtime + 4.0f;
}

// Recorder Implementation
Recorder::Recorder(KZPlayer *player, f32 numSeconds, ReplayType type, bool copyTimerEvents, DistanceTier copyJumps)
{
	Recorder::Init(replayHeader, player, type);

	CircularRecorder *circular = player->recordingService->circularRecording;
	if (!circular)
	{
		// No circular recorder initialized, nothing to copy
		return;
	}
	// Go through the events and fetch the first events within the time frame.
	// Iterate backwards to find the earliest event that is still within the time frame.
	i32 earliestWeaponIndex = -1;
	i32 earliestModeEventIndex = -1;
	i32 earliestStyleEventIndex = -1;
	i32 earliestCheckpointEventIndex = -1;

	if (circular->tickData->GetReadAvailable() == 0)
	{
		return;
	}
	i32 numTickData = MIN(circular->tickData->GetReadAvailable(), (i32)(numSeconds * ENGINE_FIXED_TICK_RATE));
	u32 earliestTick = circular->tickData->PeekSingle(circular->tickData->GetReadAvailable() - numTickData)->serverTick;
	for (i32 i = circular->tickData->GetReadAvailable() - numTickData; i < circular->tickData->GetReadAvailable(); i++)
	{
		TickData *tickData = circular->tickData->PeekSingle(i);
		if (!tickData)
		{
			break;
		}
		this->tickData.push_back(*tickData);
		// Pack subtick data into flat storage
		SubtickData *sd = circular->subtickData->PeekSingle(i);
		u8 count = (u8)MIN(sd->numSubtickMoves, MAX_SUBTICK_MOVES);
		this->subtickCounts.push_back(count);
		for (u8 j = 0; j < count; j++)
		{
			this->subtickMoves.push_back(sd->subtickMoves[j]);
		}
	}
	this->totalTicksRecorded = (u32)this->tickData.size();
	i32 first = 0;
	bool shouldCopy = false;
	for (; first < circular->rpEvents->GetReadAvailable(); first++)
	{
		shouldCopy = true;
		RpEvent *event = circular->rpEvents->PeekSingle(first);
		if (event->serverTick >= earliestTick)
		{
			break;
		}
		if (event->type == RPEVENT_MODE_CHANGE)
		{
			earliestModeEventIndex = first;
		}
		else if (event->type == RPEVENT_STYLE_CHANGE && event->data.styleChange.clearStyles)
		{
			earliestStyleEventIndex = first;
		}
	}

	if (earliestModeEventIndex == -1)
	{
		RpEvent baseModeEvent = {};
		baseModeEvent.serverTick = 0;
		baseModeEvent.type = RPEVENT_MODE_CHANGE;
		V_strncpy(baseModeEvent.data.modeChange.name, circular->earliestMode.value().name, sizeof(baseModeEvent.data.modeChange.name));
		V_strncpy(baseModeEvent.data.modeChange.md5, circular->earliestMode.value().md5, sizeof(baseModeEvent.data.modeChange.md5));
		this->rpEvents.push_back(baseModeEvent);
	}
	else
	{
		RpEvent baseModeEvent = *circular->rpEvents->PeekSingle(earliestModeEventIndex);
		baseModeEvent.serverTick = 0;
		this->rpEvents.push_back(baseModeEvent);
	}
	if (earliestStyleEventIndex == -1)
	{
		bool firstStyle = true;
		for (auto &style : circular->earliestStyles.value_or(std::vector<RpModeStyleInfo>()))
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
		RpEvent baseStyleEvent = *circular->rpEvents->PeekSingle(earliestStyleEventIndex);
		i32 eventServerTick = baseStyleEvent.serverTick;
		baseStyleEvent.serverTick = 0;
		this->rpEvents.push_back(baseStyleEvent);
		// Copy all style change events with the same server tick (they were part of the same batch)
		for (i32 i = earliestStyleEventIndex + 1; i < circular->rpEvents->GetReadAvailable(); i++)
		{
			RpEvent *event = circular->rpEvents->PeekSingle(i);
			if (!event || event->type != RPEVENT_STYLE_CHANGE || event->serverTick != eventServerTick)
			{
				break;
			}
			this->rpEvents.push_back(*event);
		}
	}
	if (shouldCopy)
	{
		for (i32 i = first; i < circular->rpEvents->GetReadAvailable(); i++)
		{
			if (!copyTimerEvents && circular->rpEvents->PeekSingle(i)->type == RPEVENT_TIMER_EVENT)
			{
				continue;
			}
			this->rpEvents.push_back(*circular->rpEvents->PeekSingle(i));
		}
	}

	shouldCopy = false;
	for (first = 0; first < circular->jumps.size(); first++)
	{
		const RpJumpStats &jump = circular->jumps[first];
		shouldCopy = true;
		if (jump.overall.serverTick >= earliestTick)
		{
			break;
		}
	}
	if (shouldCopy)
	{
		for (i32 i = first; i < circular->jumps.size(); i++)
		{
			const RpJumpStats &jump = circular->jumps[i];

			if (jump.overall.distanceTier < copyJumps)
			{
				continue;
			}
			this->jumps.push_back(jump);
		}
	}

	shouldCopy = false;
	for (first = 0; first < circular->cmdData->GetReadAvailable(); first++)
	{
		CmdData *cmdData = circular->cmdData->PeekSingle(first);
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
		for (i32 i = first; i < circular->cmdData->GetReadAvailable(); i++)
		{
			this->cmdData.push_back(*circular->cmdData->PeekSingle(i));
			// Pack cmd subtick data into flat storage
			SubtickData *sd = circular->cmdSubtickData->PeekSingle(i);
			u8 count = (u8)MIN(sd->numSubtickMoves, MAX_SUBTICK_MOVES);
			this->cmdSubtickCounts.push_back(count);
			for (u8 j = 0; j < count; j++)
			{
				this->cmdSubtickMoves.push_back(sd->subtickMoves[j]);
			}
		}
	}
}

Recorder::~Recorder()
{
	CleanupTempFiles();
}

// Binary chunk format:
//   u32 numTicks
//   u32 numSubtickMoves (total RpSubtickMove entries)
//   u32 numEvents
//   u32 numJumps
//   u32 numCmds
//   u32 numCmdSubtickMoves
//   TickData[numTicks]
//   u8[numTicks] (subtick counts)
//   RpSubtickMove[numSubtickMoves]
//   RpEvent[numEvents]
//   RpJumpStats[numJumps]  -- serialized with size prefix per entry due to std::vector member
//   CmdData[numCmds]
//   u8[numCmds] (cmd subtick counts)
//   RpSubtickMove[numCmdSubtickMoves]

void Recorder::FlushChunkToDisk()
{
	if (tempFileBase.empty())
	{
		std::string uuidStr = this->uuid.ToString();
		char path[512];
		V_snprintf(path, sizeof(path), "%s/.tmp_%s", KZ_REPLAY_PATH, uuidStr.c_str());
		tempFileBase = path;
	}

	char chunkPath[512];
	V_snprintf(chunkPath, sizeof(chunkPath), "%s_%u.chunk", tempFileBase.c_str(), numFlushedChunks);

	std::vector<char> buf;
	u32 numTicks = (u32)tickData.size();
	u32 numSubtickMovesTotal = (u32)subtickMoves.size();
	u32 numEvents = (u32)rpEvents.size();
	u32 numJumps = (u32)jumps.size();
	u32 numCmds = (u32)cmdData.size();
	u32 numCmdSubtickMovesTotal = (u32)cmdSubtickMoves.size();

	// Reserve approximate size
	buf.reserve(numTicks * (sizeof(TickData) + 1 + sizeof(SubtickData::RpSubtickMove) * 2) + numCmds * sizeof(CmdData) + 6 * sizeof(u32));

	auto appendRaw = [&buf](const void *data, size_t size) { buf.insert(buf.end(), (const char *)data, (const char *)data + size); };

	appendRaw(&numTicks, sizeof(u32));
	appendRaw(&numSubtickMovesTotal, sizeof(u32));
	appendRaw(&numEvents, sizeof(u32));
	appendRaw(&numJumps, sizeof(u32));
	appendRaw(&numCmds, sizeof(u32));
	appendRaw(&numCmdSubtickMovesTotal, sizeof(u32));

	// Tick data
	appendRaw(tickData.data(), numTicks * sizeof(TickData));
	// Subtick counts
	appendRaw(subtickCounts.data(), numTicks * sizeof(u8));
	// Subtick moves
	appendRaw(subtickMoves.data(), numSubtickMovesTotal * sizeof(SubtickData::RpSubtickMove));
	// Events
	appendRaw(rpEvents.data(), numEvents * sizeof(RpEvent));
	// Jumps - serialize each individually because RpJumpStats contains std::vector members
	for (const auto &jump : jumps)
	{
		// Write the GeneralData (trivially copyable part)
		appendRaw(&jump.overall, sizeof(RpJumpStats::GeneralData));
		// Write strafe count then strafe data
		u32 numStrafes = (u32)jump.strafes.size();
		appendRaw(&numStrafes, sizeof(u32));
		if (numStrafes > 0)
		{
			appendRaw(jump.strafes.data(), numStrafes * sizeof(RpJumpStats::StrafeData));
		}
		// Write AA call count then AA data
		u32 numAACalls = (u32)jump.aaCalls.size();
		appendRaw(&numAACalls, sizeof(u32));
		if (numAACalls > 0)
		{
			appendRaw(jump.aaCalls.data(), numAACalls * sizeof(RpJumpStats::AAData));
		}
	}
	// Cmd data
	appendRaw(cmdData.data(), numCmds * sizeof(CmdData));
	// Cmd subtick counts
	appendRaw(cmdSubtickCounts.data(), numCmds * sizeof(u8));
	// Cmd subtick moves
	appendRaw(cmdSubtickMoves.data(), numCmdSubtickMovesTotal * sizeof(SubtickData::RpSubtickMove));

	KZ_LOG_DEBUG(LogChannel::Recording, "Queuing chunk %u write: path='%s' bufSize=%zu\n", numFlushedChunks, chunkPath, buf.size());

	if (g_asyncFileIO)
	{
		g_asyncFileIO->QueueWriteBuffer(chunkPath, std::move(buf));
	}
	else
	{
		if (!utils::WriteBufferToFile(chunkPath, buf))
		{
			KZ_LOG_WARN(LogChannel::Recording, "Failed to flush chunk %u to disk (sync)\n", numFlushedChunks);
			return;
		}
	}

	numFlushedChunks++;

	// Clear in-memory data
	tickData.clear();
	subtickCounts.clear();
	subtickMoves.clear();
	rpEvents.clear();
	jumps.clear();
	cmdData.clear();
	cmdSubtickCounts.clear();
	cmdSubtickMoves.clear();
}

void Recorder::LoadFlushedChunks(std::vector<TickData> &outTick, std::vector<u8> &outSubtickCounts,
								 std::vector<SubtickData::RpSubtickMove> &outSubtickMoves, std::vector<RpEvent> &outEvents,
								 std::vector<RpJumpStats> &outJumps, std::vector<CmdData> &outCmd, std::vector<u8> &outCmdSubtickCounts,
								 std::vector<SubtickData::RpSubtickMove> &outCmdSubtickMoves)
{
	// Ensure all async writes have completed before reading chunks back.
	if (g_asyncFileIO)
	{
		g_asyncFileIO->Drain();
	}

	for (u32 chunk = 0; chunk < numFlushedChunks; chunk++)
	{
		char chunkPath[512];
		V_snprintf(chunkPath, sizeof(chunkPath), "%s_%u.chunk", tempFileBase.c_str(), chunk);

		std::vector<char> buf;
		if (!utils::ReadBufferFromFile(chunkPath, buf))
		{
			KZ_LOG_WARN(LogChannel::Recording, "Failed to read chunk %u from disk\n", chunk);
			continue;
		}

		const char *cursor = buf.data();
		const char *end = buf.data() + buf.size();

		auto readRaw = [&cursor, end](void *dst, size_t size) -> bool
		{
			if (cursor + size > end)
			{
				return false;
			}
			memcpy(dst, cursor, size);
			cursor += size;
			return true;
		};

		u32 numTicks, numSubtickMovesTotal, numEvents, numJumps, numCmds, numCmdSubtickMovesTotal;
		if (!readRaw(&numTicks, sizeof(u32)))
		{
			continue;
		}
		if (!readRaw(&numSubtickMovesTotal, sizeof(u32)))
		{
			continue;
		}
		if (!readRaw(&numEvents, sizeof(u32)))
		{
			continue;
		}
		if (!readRaw(&numJumps, sizeof(u32)))
		{
			continue;
		}
		if (!readRaw(&numCmds, sizeof(u32)))
		{
			continue;
		}
		if (!readRaw(&numCmdSubtickMovesTotal, sizeof(u32)))
		{
			continue;
		}

		// Tick data
		size_t prevSize = outTick.size();
		outTick.resize(prevSize + numTicks);
		if (!readRaw(&outTick[prevSize], numTicks * sizeof(TickData)))
		{
			continue;
		}

		// Subtick counts
		prevSize = outSubtickCounts.size();
		outSubtickCounts.resize(prevSize + numTicks);
		if (!readRaw(&outSubtickCounts[prevSize], numTicks * sizeof(u8)))
		{
			continue;
		}

		// Subtick moves
		prevSize = outSubtickMoves.size();
		outSubtickMoves.resize(prevSize + numSubtickMovesTotal);
		if (!readRaw(&outSubtickMoves[prevSize], numSubtickMovesTotal * sizeof(SubtickData::RpSubtickMove)))
		{
			continue;
		}

		// Events
		prevSize = outEvents.size();
		outEvents.resize(prevSize + numEvents);
		if (!readRaw(&outEvents[prevSize], numEvents * sizeof(RpEvent)))
		{
			continue;
		}

		// Jumps
		for (u32 j = 0; j < numJumps; j++)
		{
			RpJumpStats jump = {};
			if (!readRaw(&jump.overall, sizeof(RpJumpStats::GeneralData)))
			{
				break;
			}
			u32 numStrafes;
			if (!readRaw(&numStrafes, sizeof(u32)))
			{
				break;
			}
			if (numStrafes > 0)
			{
				jump.strafes.resize(numStrafes);
				if (!readRaw(jump.strafes.data(), numStrafes * sizeof(RpJumpStats::StrafeData)))
				{
					break;
				}
			}
			u32 numAACalls;
			if (!readRaw(&numAACalls, sizeof(u32)))
			{
				break;
			}
			if (numAACalls > 0)
			{
				jump.aaCalls.resize(numAACalls);
				if (!readRaw(jump.aaCalls.data(), numAACalls * sizeof(RpJumpStats::AAData)))
				{
					break;
				}
			}
			outJumps.push_back(std::move(jump));
		}

		// Cmd data
		prevSize = outCmd.size();
		outCmd.resize(prevSize + numCmds);
		if (!readRaw(&outCmd[prevSize], numCmds * sizeof(CmdData)))
		{
			continue;
		}

		// Cmd subtick counts
		prevSize = outCmdSubtickCounts.size();
		outCmdSubtickCounts.resize(prevSize + numCmds);
		if (!readRaw(&outCmdSubtickCounts[prevSize], numCmds * sizeof(u8)))
		{
			continue;
		}

		// Cmd subtick moves
		prevSize = outCmdSubtickMoves.size();
		outCmdSubtickMoves.resize(prevSize + numCmdSubtickMovesTotal);
		if (!readRaw(&outCmdSubtickMoves[prevSize], numCmdSubtickMovesTotal * sizeof(SubtickData::RpSubtickMove)))
		{
			continue;
		}
	}

	// Append current in-memory remainder
	outTick.insert(outTick.end(), tickData.begin(), tickData.end());
	outSubtickCounts.insert(outSubtickCounts.end(), subtickCounts.begin(), subtickCounts.end());
	outSubtickMoves.insert(outSubtickMoves.end(), subtickMoves.begin(), subtickMoves.end());
	outEvents.insert(outEvents.end(), rpEvents.begin(), rpEvents.end());
	outJumps.insert(outJumps.end(), jumps.begin(), jumps.end());
	outCmd.insert(outCmd.end(), cmdData.begin(), cmdData.end());
	outCmdSubtickCounts.insert(outCmdSubtickCounts.end(), cmdSubtickCounts.begin(), cmdSubtickCounts.end());
	outCmdSubtickMoves.insert(outCmdSubtickMoves.end(), cmdSubtickMoves.begin(), cmdSubtickMoves.end());
}

void Recorder::CleanupTempFiles()
{
	if (tempFileBase.empty())
	{
		return;
	}

	KZ_LOG_DEBUG(LogChannel::Recording, "CleanupTempFiles: deleting %u chunks (base='%s'), draining...\n", numFlushedChunks, tempFileBase.c_str());

	// Ensure all async writes have completed before deleting.
	if (g_asyncFileIO)
	{
		g_asyncFileIO->Drain();
	}

	for (u32 i = 0; i < numFlushedChunks; i++)
	{
		char chunkPath[512];
		V_snprintf(chunkPath, sizeof(chunkPath), "%s_%u.chunk", tempFileBase.c_str(), i);
		utils::RemoveFile(chunkPath);
	}
	numFlushedChunks = 0;
	tempFileBase.clear();
}

bool Recorder::WriteToFile()
{
	std::vector<char> buffer;
	if (!this->WriteToMemory(buffer))
	{
		return false;
	}

	std::string uuidStr = this->uuid.ToString();
	char finalFilename[512];
	V_snprintf(finalFilename, sizeof(finalFilename), "%s/%s.replay", KZ_REPLAY_PATH, uuidStr.c_str());

	if (!utils::WriteBufferToFile(finalFilename, buffer))
	{
		return false;
	}

	KZ_LOG_DEBUG(LogChannel::Recording, "Saved replay to %s (%zu bytes)\n", finalFilename, buffer.size());
	CleanupTempFiles();
	return true;
}

i32 Recorder::WriteHeader(std::vector<char> &outBuffer)
{
	// Serialize unified protobuf header with length prefix
	std::string serialized;
	if (!this->replayHeader.SerializeToString(&serialized))
	{
		KZ_LOG_WARN(LogChannel::Recording, "Failed to serialize replay header protobuf\n");
		return 0;
	}
	u32 size = static_cast<u32>(serialized.size());
	i32 written = (i32)(sizeof(size) + serialized.size());
	const char *sizeBytes = reinterpret_cast<const char *>(&size);
	outBuffer.insert(outBuffer.end(), sizeBytes, sizeBytes + sizeof(size));
	outBuffer.insert(outBuffer.end(), serialized.begin(), serialized.end());
	return written;
}

bool Recorder::WriteToMemory(std::vector<char> &outBuffer)
{
	// Update the replay timestamp before serializing.
	time_t unixTime = 0;
	time(&unixTime);
	replayHeader.set_timestamp((u64)unixTime);

	// If chunks were flushed to disk, load them all back and combine with in-memory remainder.
	std::vector<TickData> allTickData;
	std::vector<u8> allSubtickCounts;
	std::vector<SubtickData::RpSubtickMove> allSubtickMoves;
	std::vector<RpEvent> allEvents;
	std::vector<RpJumpStats> allJumps;
	std::vector<CmdData> allCmdData;
	std::vector<u8> allCmdSubtickCounts;
	std::vector<SubtickData::RpSubtickMove> allCmdSubtickMoves;

	if (numFlushedChunks > 0)
	{
		LoadFlushedChunks(allTickData, allSubtickCounts, allSubtickMoves, allEvents, allJumps, allCmdData, allCmdSubtickCounts, allCmdSubtickMoves);
	}
	else
	{
		allTickData = std::move(this->tickData);
		allSubtickCounts = std::move(this->subtickCounts);
		allSubtickMoves = std::move(this->subtickMoves);
		allEvents = std::move(this->rpEvents);
		allJumps = std::move(this->jumps);
		allCmdData = std::move(this->cmdData);
		allCmdSubtickCounts = std::move(this->cmdSubtickCounts);
		allCmdSubtickMoves = std::move(this->cmdSubtickMoves);
	}

	// Unpack subtick data from flat storage into SubtickData format for compression.
	std::vector<SubtickData> unpackedSubtick;
	UnpackSubtickData(unpackedSubtick, allSubtickCounts, allSubtickMoves);

	std::vector<SubtickData> unpackedCmdSubtick;
	UnpackSubtickData(unpackedCmdSubtick, allCmdSubtickCounts, allCmdSubtickMoves);

	// Order of writing must match order of reading in kz_replaydata.cpp
	this->WriteHeader(outBuffer);
	KZ::replaysystem::compression::WriteTickDataCompressed(outBuffer, allTickData, unpackedSubtick);
	KZ::replaysystem::compression::WriteWeaponsCompressed(outBuffer, this->weaponTable);
	KZ::replaysystem::compression::WriteJumpsCompressed(outBuffer, allJumps);
	KZ::replaysystem::compression::WriteEventsCompressed(outBuffer, allEvents);
	KZ::replaysystem::compression::WriteCmdDataCompressed(outBuffer, allCmdData, unpackedCmdSubtick);

	return true;
}
