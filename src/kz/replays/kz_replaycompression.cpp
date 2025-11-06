#include "kz_replay.h"
#include "kz_replaycompression.h"
#include "filesystem.h"
#include "vendor/zstd/lib/zstd.h"

using namespace KZ::replaysystem::compression;

// ========================================
// Helper functions
// ========================================

static_function void AppendToBuffer(std::vector<char> &buffer, const void *data, size_t size)
{
	const char *bytes = static_cast<const char *>(data);
	buffer.insert(buffer.end(), bytes, bytes + size);
}

// ========================================
// Compression utility functions
// ========================================

bool KZ::replaysystem::compression::Compress(const void *src, size_t srcSize, void **outDst, size_t *outDstSize)
{
	// Get worst-case compressed size
	size_t maxCompressedSize = ZSTD_compressBound(srcSize);
	*outDst = new char[maxCompressedSize];
	if (!*outDst)
	{
		return false;
	}

	// Compress using zstd level 3
	size_t compressedSize = ZSTD_compress(*outDst, maxCompressedSize, src, srcSize, 3);
	if (ZSTD_isError(compressedSize))
	{
		delete[] static_cast<char *>(*outDst);
		*outDst = nullptr;
		return false;
	}

	*outDstSize = compressedSize;
	return true;
}

bool KZ::replaysystem::compression::Decompress(const void *src, size_t srcSize, void *dst, size_t dstSize)
{
	size_t decompressedSize = ZSTD_decompress(dst, dstSize, src, srcSize);
	if (ZSTD_isError(decompressedSize))
	{
		return false;
	}

	return decompressedSize == dstSize;
}

// ========================================
// Tick data compression
// ========================================

// Bit flags for which fields have changed
enum TickDataChangeFlags : u64
{
	// Top-level fields (0-9)
	CHANGED_SERVER_TICK = (1ULL << 0), // Only if difference != 1
	CHANGED_GAME_TIME = (1ULL << 1),
	CHANGED_REAL_TIME = (1ULL << 2),
	CHANGED_UNIX_TIME = (1ULL << 3),
	CHANGED_CMD_NUMBER = (1ULL << 4),
	CHANGED_CLIENT_TICK = (1ULL << 5),
	CHANGED_FORWARD = (1ULL << 6),
	CHANGED_LEFT = (1ULL << 7),
	CHANGED_UP = (1ULL << 8),
	CHANGED_LEFT_HANDED = (1ULL << 9),

	// Pre fields (10-23)
	CHANGED_PRE_ORIGIN = (1ULL << 10),
	CHANGED_PRE_VELOCITY = (1ULL << 11),
	CHANGED_PRE_ANGLES = (1ULL << 12),
	CHANGED_PRE_BUTTONS_0 = (1ULL << 13),
	CHANGED_PRE_BUTTONS_1 = (1ULL << 14),
	CHANGED_PRE_BUTTONS_2 = (1ULL << 15),
	CHANGED_PRE_JUMP_PRESSED_TIME = (1ULL << 16),
	CHANGED_PRE_DUCK_SPEED = (1ULL << 17),
	CHANGED_PRE_DUCK_AMOUNT = (1ULL << 18),
	CHANGED_PRE_DUCK_OFFSET = (1ULL << 19),
	CHANGED_PRE_LAST_DUCK_TIME = (1ULL << 20),
	CHANGED_PRE_REPLAY_FLAGS = (1ULL << 21),
	CHANGED_PRE_ENTITY_FLAGS = (1ULL << 22),
	CHANGED_PRE_MOVE_TYPE = (1ULL << 23),

	// Post fields (24-37)
	CHANGED_POST_ORIGIN = (1ULL << 24),
	CHANGED_POST_VELOCITY = (1ULL << 25),
	CHANGED_POST_ANGLES = (1ULL << 26),
	CHANGED_POST_BUTTONS_0 = (1ULL << 27),
	CHANGED_POST_BUTTONS_1 = (1ULL << 28),
	CHANGED_POST_BUTTONS_2 = (1ULL << 29),
	CHANGED_POST_JUMP_PRESSED_TIME = (1ULL << 30),
	CHANGED_POST_DUCK_SPEED = (1ULL << 31),
	CHANGED_POST_DUCK_AMOUNT = (1ULL << 32),
	CHANGED_POST_DUCK_OFFSET = (1ULL << 33),
	CHANGED_POST_LAST_DUCK_TIME = (1ULL << 34),
	CHANGED_POST_REPLAY_FLAGS = (1ULL << 35),
	CHANGED_POST_ENTITY_FLAGS = (1ULL << 36),
	CHANGED_POST_MOVE_TYPE = (1ULL << 37),

	CHANGED_CHECKPOINT = (1ULL << 38),
};

// Compare pre or post data and build change flags
static_function u64 BuildPreChangeFlags(const TickData::MovementData &current, const TickData::MovementData &previous)
{
	u64 flags = 0;
	// clang-format off
	if (current.origin != previous.origin) flags |= CHANGED_PRE_ORIGIN;
	if (current.velocity != previous.velocity) flags |= CHANGED_PRE_VELOCITY;
	if (current.angles != previous.angles) flags |= CHANGED_PRE_ANGLES;
	if (current.buttons[0] != previous.buttons[0]) flags |= CHANGED_PRE_BUTTONS_0;
	if (current.buttons[1] != previous.buttons[1]) flags |= CHANGED_PRE_BUTTONS_1;
	if (current.buttons[2] != previous.buttons[2]) flags |= CHANGED_PRE_BUTTONS_2;
	if (current.jumpPressedTime != previous.jumpPressedTime) flags |= CHANGED_PRE_JUMP_PRESSED_TIME;
	if (current.duckSpeed != previous.duckSpeed) flags |= CHANGED_PRE_DUCK_SPEED;
	if (current.duckAmount != previous.duckAmount) flags |= CHANGED_PRE_DUCK_AMOUNT;
	if (current.duckOffset != previous.duckOffset) flags |= CHANGED_PRE_DUCK_OFFSET;
	if (current.lastDuckTime != previous.lastDuckTime) flags |= CHANGED_PRE_LAST_DUCK_TIME;
	if (current.replayFlags.ducking != previous.replayFlags.ducking || 
		current.replayFlags.ducked != previous.replayFlags.ducked || 
		current.replayFlags.desiresDuck != previous.replayFlags.desiresDuck) 
	{
		flags |= CHANGED_PRE_REPLAY_FLAGS;
	}
	if (current.entityFlags != previous.entityFlags) flags |= CHANGED_PRE_ENTITY_FLAGS;
	if (current.moveType != previous.moveType) flags |= CHANGED_PRE_MOVE_TYPE;
	// clang-format on
	return flags;
}

static_function u64 BuildPostChangeFlags(const TickData::MovementData &current, const TickData::MovementData &previous)
{
	u64 flags = 0;
	// clang-format off
	if (current.origin != previous.origin) flags |= CHANGED_POST_ORIGIN;
	if (current.velocity != previous.velocity) flags |= CHANGED_POST_VELOCITY;
	if (current.angles != previous.angles) flags |= CHANGED_POST_ANGLES;
	if (current.buttons[0] != previous.buttons[0]) flags |= CHANGED_POST_BUTTONS_0;
	if (current.buttons[1] != previous.buttons[1]) flags |= CHANGED_POST_BUTTONS_1;
	if (current.buttons[2] != previous.buttons[2]) flags |= CHANGED_POST_BUTTONS_2;
	if (current.jumpPressedTime != previous.jumpPressedTime) flags |= CHANGED_POST_JUMP_PRESSED_TIME;
	if (current.duckSpeed != previous.duckSpeed) flags |= CHANGED_POST_DUCK_SPEED;
	if (current.duckAmount != previous.duckAmount) flags |= CHANGED_POST_DUCK_AMOUNT;
	if (current.duckOffset != previous.duckOffset) flags |= CHANGED_POST_DUCK_OFFSET;
	if (current.lastDuckTime != previous.lastDuckTime) flags |= CHANGED_POST_LAST_DUCK_TIME;
	if (current.replayFlags.ducking != previous.replayFlags.ducking || 
		current.replayFlags.ducked != previous.replayFlags.ducked || 
		current.replayFlags.desiresDuck != previous.replayFlags.desiresDuck) 
	{
		flags |= CHANGED_POST_REPLAY_FLAGS;
	}
	if (current.entityFlags != previous.entityFlags) flags |= CHANGED_POST_ENTITY_FLAGS;
	if (current.moveType != previous.moveType) flags |= CHANGED_POST_MOVE_TYPE;
	// clang-format on
	return flags;
}

i32 KZ::replaysystem::compression::WriteTickDataCompressed(FileHandle_t file, const std::vector<TickData> &tickData,
														   const std::vector<SubtickData> &subtickData)
{
	i32 bytesWritten = 0;
	std::vector<char> buffer;

	// Reserve approximate size
	buffer.reserve(tickData.size() * 120); // Rough estimate

	for (size_t i = 0; i < tickData.size(); i++)
	{
		const TickData &current = tickData[i];

		// Determine what to compare against
		// For tick 0: compare pre with zero, post with pre
		// For tick N: compare pre with previous post, post with current pre
		TickData::MovementData prevPre = {};
		TickData::MovementData prevPost = {};

		if (i == 0)
		{
			// First tick: prevPre is zero, prevPost is current.pre
			prevPost = current.pre;
		}
		else
		{
			// N-th tick: prevPre is previous.post, prevPost is current.pre
			prevPre = tickData[i - 1].post;
			prevPost = current.pre;
		}

		// Build change flags for top-level fields
		u64 flags = 0;

		// Server tick: only write if difference is not 1
		u32 expectedServerTick = (i == 0) ? 0 : tickData[i - 1].serverTick + 1;
		if (current.serverTick != expectedServerTick)
		{
			flags |= CHANGED_SERVER_TICK;
		}
		// clang-format off
		if (i == 0 || current.gameTime != tickData[i - 1].gameTime) flags |= CHANGED_GAME_TIME;
		if (i == 0 || current.realTime != tickData[i - 1].realTime) flags |= CHANGED_REAL_TIME;
		if (i == 0 || current.unixTime != tickData[i - 1].unixTime) flags |= CHANGED_UNIX_TIME;
		if (i == 0 || current.cmdNumber != tickData[i - 1].cmdNumber) flags |= CHANGED_CMD_NUMBER;
		if (i == 0 || current.clientTick != tickData[i - 1].clientTick) flags |= CHANGED_CLIENT_TICK;
		if (i == 0 || current.forward != tickData[i - 1].forward) flags |= CHANGED_FORWARD;
		if (i == 0 || current.left != tickData[i - 1].left) flags |= CHANGED_LEFT;
		if (i == 0 || current.up != tickData[i - 1].up) flags |= CHANGED_UP;
		if (i == 0 || current.leftHanded != tickData[i - 1].leftHanded) flags |= CHANGED_LEFT_HANDED;

		// Build change flags for pre/post
		flags |= BuildPreChangeFlags(current.pre, prevPre);
		flags |= BuildPostChangeFlags(current.post, prevPost);

		// Check for checkpoint data changes
		if (i == 0 || 
			current.checkpoint.index != tickData[i - 1].checkpoint.index ||
			current.checkpoint.checkpointCount != tickData[i - 1].checkpoint.checkpointCount ||
			current.checkpoint.teleportCount != tickData[i - 1].checkpoint.teleportCount)
		{
			flags |= CHANGED_CHECKPOINT;
		}
		
		// Write change flags (single u64)
		AppendToBuffer(buffer, &flags, sizeof(flags));
		
		// Write changed top-level fields
		if (flags & CHANGED_SERVER_TICK) AppendToBuffer(buffer, &current.serverTick, sizeof(current.serverTick));
		if (flags & CHANGED_GAME_TIME) AppendToBuffer(buffer, &current.gameTime, sizeof(current.gameTime));
		if (flags & CHANGED_REAL_TIME) AppendToBuffer(buffer, &current.realTime, sizeof(current.realTime));
		if (flags & CHANGED_UNIX_TIME) AppendToBuffer(buffer, &current.unixTime, sizeof(current.unixTime));
		if (flags & CHANGED_CMD_NUMBER) AppendToBuffer(buffer, &current.cmdNumber, sizeof(current.cmdNumber));
		if (flags & CHANGED_CLIENT_TICK) AppendToBuffer(buffer, &current.clientTick, sizeof(current.clientTick));
		if (flags & CHANGED_FORWARD) AppendToBuffer(buffer, &current.forward, sizeof(current.forward));
		if (flags & CHANGED_LEFT) AppendToBuffer(buffer, &current.left, sizeof(current.left));
		if (flags & CHANGED_UP) AppendToBuffer(buffer, &current.up, sizeof(current.up));
		if (flags & CHANGED_LEFT_HANDED) AppendToBuffer(buffer, &current.leftHanded, sizeof(current.leftHanded));
		
		// Write changed pre fields
		if (flags & CHANGED_PRE_ORIGIN) AppendToBuffer(buffer, &current.pre.origin, sizeof(current.pre.origin));
		if (flags & CHANGED_PRE_VELOCITY) AppendToBuffer(buffer, &current.pre.velocity, sizeof(current.pre.velocity));
		if (flags & CHANGED_PRE_ANGLES) AppendToBuffer(buffer, &current.pre.angles, sizeof(current.pre.angles));
		if (flags & CHANGED_PRE_BUTTONS_0) AppendToBuffer(buffer, &current.pre.buttons[0], sizeof(current.pre.buttons[0]));
		if (flags & CHANGED_PRE_BUTTONS_1) AppendToBuffer(buffer, &current.pre.buttons[1], sizeof(current.pre.buttons[1]));
		if (flags & CHANGED_PRE_BUTTONS_2) AppendToBuffer(buffer, &current.pre.buttons[2], sizeof(current.pre.buttons[2]));
		if (flags & CHANGED_PRE_JUMP_PRESSED_TIME) AppendToBuffer(buffer, &current.pre.jumpPressedTime, sizeof(current.pre.jumpPressedTime));
		if (flags & CHANGED_PRE_DUCK_SPEED) AppendToBuffer(buffer, &current.pre.duckSpeed, sizeof(current.pre.duckSpeed));
		if (flags & CHANGED_PRE_DUCK_AMOUNT) AppendToBuffer(buffer, &current.pre.duckAmount, sizeof(current.pre.duckAmount));
		if (flags & CHANGED_PRE_DUCK_OFFSET) AppendToBuffer(buffer, &current.pre.duckOffset, sizeof(current.pre.duckOffset));
		if (flags & CHANGED_PRE_LAST_DUCK_TIME) AppendToBuffer(buffer, &current.pre.lastDuckTime, sizeof(current.pre.lastDuckTime));
		if (flags & CHANGED_PRE_REPLAY_FLAGS) AppendToBuffer(buffer, &current.pre.replayFlags, sizeof(current.pre.replayFlags));
		if (flags & CHANGED_PRE_ENTITY_FLAGS) AppendToBuffer(buffer, &current.pre.entityFlags, sizeof(current.pre.entityFlags));
		if (flags & CHANGED_PRE_MOVE_TYPE) AppendToBuffer(buffer, &current.pre.moveType, sizeof(current.pre.moveType));
		
		// Write changed post fields
		if (flags & CHANGED_POST_ORIGIN) AppendToBuffer(buffer, &current.post.origin, sizeof(current.post.origin));
		if (flags & CHANGED_POST_VELOCITY) AppendToBuffer(buffer, &current.post.velocity, sizeof(current.post.velocity));
		if (flags & CHANGED_POST_ANGLES) AppendToBuffer(buffer, &current.post.angles, sizeof(current.post.angles));
		if (flags & CHANGED_POST_BUTTONS_0) AppendToBuffer(buffer, &current.post.buttons[0], sizeof(current.post.buttons[0]));
		if (flags & CHANGED_POST_BUTTONS_1) AppendToBuffer(buffer, &current.post.buttons[1], sizeof(current.post.buttons[1]));
		if (flags & CHANGED_POST_BUTTONS_2) AppendToBuffer(buffer, &current.post.buttons[2], sizeof(current.post.buttons[2]));
		if (flags & CHANGED_POST_JUMP_PRESSED_TIME) AppendToBuffer(buffer, &current.post.jumpPressedTime, sizeof(current.post.jumpPressedTime));
		if (flags & CHANGED_POST_DUCK_SPEED) AppendToBuffer(buffer, &current.post.duckSpeed, sizeof(current.post.duckSpeed));
		if (flags & CHANGED_POST_DUCK_AMOUNT) AppendToBuffer(buffer, &current.post.duckAmount, sizeof(current.post.duckAmount));
		if (flags & CHANGED_POST_DUCK_OFFSET) AppendToBuffer(buffer, &current.post.duckOffset, sizeof(current.post.duckOffset));
		if (flags & CHANGED_POST_LAST_DUCK_TIME) AppendToBuffer(buffer, &current.post.lastDuckTime, sizeof(current.post.lastDuckTime));
		if (flags & CHANGED_POST_REPLAY_FLAGS) AppendToBuffer(buffer, &current.post.replayFlags, sizeof(current.post.replayFlags));
		if (flags & CHANGED_POST_ENTITY_FLAGS) AppendToBuffer(buffer, &current.post.entityFlags, sizeof(current.post.entityFlags));
		if (flags & CHANGED_POST_MOVE_TYPE) AppendToBuffer(buffer, &current.post.moveType, sizeof(current.post.moveType));

		// Write checkpoint and cvar data if present
		if (flags & CHANGED_CHECKPOINT)
		{
			AppendToBuffer(buffer, &current.checkpoint.index, sizeof(current.checkpoint.index));
			AppendToBuffer(buffer, &current.checkpoint.checkpointCount, sizeof(current.checkpoint.checkpointCount));
			AppendToBuffer(buffer, &current.checkpoint.teleportCount, sizeof(current.checkpoint.teleportCount));
		}
		// clang-format on
	}

	// Compress the buffer
	void *compressedData = nullptr;
	size_t compressedSize = 0;

	bool success = Compress(buffer.data(), buffer.size(), &compressedData, &compressedSize);
	if (!success)
	{
		return 0;
	}

	// Write section header
	CompressedSectionHeader header;
	header.compressedSize = (u32)compressedSize;
	header.uncompressedSize = (u32)buffer.size();
	header.elementCount = (u32)tickData.size();

	bytesWritten += g_pFullFileSystem->Write(&header, sizeof(header), file);
	bytesWritten += g_pFullFileSystem->Write(compressedData, compressedSize, file);

	delete[] static_cast<char *>(compressedData);

	// Compress subtick data
	void *compressedSubtick = nullptr;
	size_t compressedSubtickSize = 0;
	size_t uncompressedSubtickSize = subtickData.size() * sizeof(SubtickData);

	success = Compress(subtickData.data(), uncompressedSubtickSize, &compressedSubtick, &compressedSubtickSize);

	if (success)
	{
		CompressedSectionHeader subtickHeader;
		subtickHeader.compressedSize = (u32)compressedSubtickSize;
		subtickHeader.uncompressedSize = (u32)uncompressedSubtickSize;
		subtickHeader.elementCount = (u32)subtickData.size();

		bytesWritten += g_pFullFileSystem->Write(&subtickHeader, sizeof(subtickHeader), file);
		bytesWritten += g_pFullFileSystem->Write(compressedSubtick, compressedSubtickSize, file);

		delete[] static_cast<char *>(compressedSubtick);
	}

	return bytesWritten;
}

bool KZ::replaysystem::compression::ReadTickDataCompressed(FileHandle_t file, std::vector<TickData> &outTickData,
														   std::vector<SubtickData> &outSubtickData)
{
	// Read section header
	CompressedSectionHeader header;
	g_pFullFileSystem->Read(&header, sizeof(header), file);

	// Allocate buffer for compressed data
	char *compressedData = new char[header.compressedSize];
	if (!compressedData)
	{
		return false;
	}

	// Read compressed data
	g_pFullFileSystem->Read(compressedData, header.compressedSize, file);

	// Allocate buffer for decompressed data
	char *decompressedData = new char[header.uncompressedSize];
	if (!decompressedData)
	{
		delete[] compressedData;
		return false;
	}

	// Decompress
	bool success = Decompress(compressedData, header.compressedSize, decompressedData, header.uncompressedSize);
	delete[] compressedData;

	if (!success)
	{
		delete[] decompressedData;
		return false;
	}

	// Reconstruct tick data from delta-encoded buffer
	outTickData.resize(header.elementCount);
	const char *readPtr = decompressedData;

	for (u32 i = 0; i < header.elementCount; i++)
	{
		TickData &current = outTickData[i];

		// Read change flags
		u64 flags;
		memcpy(&flags, readPtr, sizeof(flags));
		readPtr += sizeof(flags);

		// Copy from previous tick if not changed (or apply expected increment)
		if (i > 0)
		{
			// clang-format off
			// Server tick is expected to increment by 1
			if (!(flags & CHANGED_SERVER_TICK)) current.serverTick = outTickData[i - 1].serverTick + 1;
			if (!(flags & CHANGED_GAME_TIME)) current.gameTime = outTickData[i - 1].gameTime;
			if (!(flags & CHANGED_REAL_TIME)) current.realTime = outTickData[i - 1].realTime;
			if (!(flags & CHANGED_UNIX_TIME)) current.unixTime = outTickData[i - 1].unixTime;
			if (!(flags & CHANGED_CMD_NUMBER)) current.cmdNumber = outTickData[i - 1].cmdNumber;
			if (!(flags & CHANGED_CLIENT_TICK)) current.clientTick = outTickData[i - 1].clientTick;
			if (!(flags & CHANGED_FORWARD)) current.forward = outTickData[i - 1].forward;
			if (!(flags & CHANGED_LEFT)) current.left = outTickData[i - 1].left;
			if (!(flags & CHANGED_UP)) current.up = outTickData[i - 1].up;
			if (!(flags & CHANGED_LEFT_HANDED)) current.leftHanded = outTickData[i - 1].leftHanded;
			// clang-format on
		}

		// Read changed fields
		// clang-format off
		if (flags & CHANGED_SERVER_TICK) { memcpy(&current.serverTick, readPtr, sizeof(current.serverTick)); readPtr += sizeof(current.serverTick); }
		if (flags & CHANGED_GAME_TIME) { memcpy(&current.gameTime, readPtr, sizeof(current.gameTime)); readPtr += sizeof(current.gameTime); }
		if (flags & CHANGED_REAL_TIME) { memcpy(&current.realTime, readPtr, sizeof(current.realTime)); readPtr += sizeof(current.realTime); }
		if (flags & CHANGED_UNIX_TIME) { memcpy(&current.unixTime, readPtr, sizeof(current.unixTime)); readPtr += sizeof(current.unixTime); }
		if (flags & CHANGED_CMD_NUMBER) { memcpy(&current.cmdNumber, readPtr, sizeof(current.cmdNumber)); readPtr += sizeof(current.cmdNumber); }
		if (flags & CHANGED_CLIENT_TICK) { memcpy(&current.clientTick, readPtr, sizeof(current.clientTick)); readPtr += sizeof(current.clientTick); }
		if (flags & CHANGED_FORWARD) { memcpy(&current.forward, readPtr, sizeof(current.forward)); readPtr += sizeof(current.forward); }
		if (flags & CHANGED_LEFT) { memcpy(&current.left, readPtr, sizeof(current.left)); readPtr += sizeof(current.left); }
		if (flags & CHANGED_UP) { memcpy(&current.up, readPtr, sizeof(current.up)); readPtr += sizeof(current.up); }
		if (flags & CHANGED_LEFT_HANDED) { memcpy(&current.leftHanded, readPtr, sizeof(current.leftHanded)); readPtr += sizeof(current.leftHanded); }

		// Reconstruct pre data
		// For tick 0: compare with zero
		// For tick N: compare with previous.post
		TickData::MovementData prevPre = {};
		if (i > 0)
		{
			prevPre = outTickData[i - 1].post;
		}
		
		// Copy from previous or read changed
		current.pre = prevPre;
		if (flags & CHANGED_PRE_ORIGIN) { memcpy(&current.pre.origin, readPtr, sizeof(current.pre.origin)); readPtr += sizeof(current.pre.origin); }
		if (flags & CHANGED_PRE_VELOCITY) { memcpy(&current.pre.velocity, readPtr, sizeof(current.pre.velocity)); readPtr += sizeof(current.pre.velocity); }
		if (flags & CHANGED_PRE_ANGLES) { memcpy(&current.pre.angles, readPtr, sizeof(current.pre.angles)); readPtr += sizeof(current.pre.angles); }
		if (flags & CHANGED_PRE_BUTTONS_0) { memcpy(&current.pre.buttons[0], readPtr, sizeof(current.pre.buttons[0])); readPtr += sizeof(current.pre.buttons[0]); }
		if (flags & CHANGED_PRE_BUTTONS_1) { memcpy(&current.pre.buttons[1], readPtr, sizeof(current.pre.buttons[1])); readPtr += sizeof(current.pre.buttons[1]); }
		if (flags & CHANGED_PRE_BUTTONS_2) { memcpy(&current.pre.buttons[2], readPtr, sizeof(current.pre.buttons[2])); readPtr += sizeof(current.pre.buttons[2]); }
		if (flags & CHANGED_PRE_JUMP_PRESSED_TIME) { memcpy(&current.pre.jumpPressedTime, readPtr, sizeof(current.pre.jumpPressedTime)); readPtr += sizeof(current.pre.jumpPressedTime); }
		if (flags & CHANGED_PRE_DUCK_SPEED) { memcpy(&current.pre.duckSpeed, readPtr, sizeof(current.pre.duckSpeed)); readPtr += sizeof(current.pre.duckSpeed); }
		if (flags & CHANGED_PRE_DUCK_AMOUNT) { memcpy(&current.pre.duckAmount, readPtr, sizeof(current.pre.duckAmount)); readPtr += sizeof(current.pre.duckAmount); }
		if (flags & CHANGED_PRE_DUCK_OFFSET) { memcpy(&current.pre.duckOffset, readPtr, sizeof(current.pre.duckOffset)); readPtr += sizeof(current.pre.duckOffset); }
		if (flags & CHANGED_PRE_LAST_DUCK_TIME) { memcpy(&current.pre.lastDuckTime, readPtr, sizeof(current.pre.lastDuckTime)); readPtr += sizeof(current.pre.lastDuckTime); }
		if (flags & CHANGED_PRE_REPLAY_FLAGS) { memcpy(&current.pre.replayFlags, readPtr, sizeof(current.pre.replayFlags)); readPtr += sizeof(current.pre.replayFlags); }
		if (flags & CHANGED_PRE_ENTITY_FLAGS) { memcpy(&current.pre.entityFlags, readPtr, sizeof(current.pre.entityFlags)); readPtr += sizeof(current.pre.entityFlags); }
		if (flags & CHANGED_PRE_MOVE_TYPE) { memcpy(&current.pre.moveType, readPtr, sizeof(current.pre.moveType)); readPtr += sizeof(current.pre.moveType); }
		
		// Reconstruct post data (compare with current.pre)
		current.post = current.pre;
		if (flags & CHANGED_POST_ORIGIN) { memcpy(&current.post.origin, readPtr, sizeof(current.post.origin)); readPtr += sizeof(current.post.origin); }
		if (flags & CHANGED_POST_VELOCITY) { memcpy(&current.post.velocity, readPtr, sizeof(current.post.velocity)); readPtr += sizeof(current.post.velocity); }
		if (flags & CHANGED_POST_ANGLES) { memcpy(&current.post.angles, readPtr, sizeof(current.post.angles)); readPtr += sizeof(current.post.angles); }
		if (flags & CHANGED_POST_BUTTONS_0) { memcpy(&current.post.buttons[0], readPtr, sizeof(current.post.buttons[0])); readPtr += sizeof(current.post.buttons[0]); }
		if (flags & CHANGED_POST_BUTTONS_1) { memcpy(&current.post.buttons[1], readPtr, sizeof(current.post.buttons[1])); readPtr += sizeof(current.post.buttons[1]); }
		if (flags & CHANGED_POST_BUTTONS_2) { memcpy(&current.post.buttons[2], readPtr, sizeof(current.post.buttons[2])); readPtr += sizeof(current.post.buttons[2]); }
		if (flags & CHANGED_POST_JUMP_PRESSED_TIME) { memcpy(&current.post.jumpPressedTime, readPtr, sizeof(current.post.jumpPressedTime)); readPtr += sizeof(current.post.jumpPressedTime); }
		if (flags & CHANGED_POST_DUCK_SPEED) { memcpy(&current.post.duckSpeed, readPtr, sizeof(current.post.duckSpeed)); readPtr += sizeof(current.post.duckSpeed); }
		if (flags & CHANGED_POST_DUCK_AMOUNT) { memcpy(&current.post.duckAmount, readPtr, sizeof(current.post.duckAmount)); readPtr += sizeof(current.post.duckAmount); }
		if (flags & CHANGED_POST_DUCK_OFFSET) { memcpy(&current.post.duckOffset, readPtr, sizeof(current.post.duckOffset)); readPtr += sizeof(current.post.duckOffset); }
		if (flags & CHANGED_POST_LAST_DUCK_TIME) { memcpy(&current.post.lastDuckTime, readPtr, sizeof(current.post.lastDuckTime)); readPtr += sizeof(current.post.lastDuckTime); }
		if (flags & CHANGED_POST_REPLAY_FLAGS) { memcpy(&current.post.replayFlags, readPtr, sizeof(current.post.replayFlags)); readPtr += sizeof(current.post.replayFlags); }
		if (flags & CHANGED_POST_ENTITY_FLAGS) { memcpy(&current.post.entityFlags, readPtr, sizeof(current.post.entityFlags)); readPtr += sizeof(current.post.entityFlags); }
		if (flags & CHANGED_POST_MOVE_TYPE) { memcpy(&current.post.moveType, readPtr, sizeof(current.post.moveType)); readPtr += sizeof(current.post.moveType); }

		// Read checkpoint and cvar data if present
		if (flags & CHANGED_CHECKPOINT)
		{
			memcpy(&current.checkpoint.index, readPtr, sizeof(current.checkpoint.index)); readPtr += sizeof(current.checkpoint.index);
			memcpy(&current.checkpoint.checkpointCount, readPtr, sizeof(current.checkpoint.checkpointCount)); readPtr += sizeof(current.checkpoint.checkpointCount);
			memcpy(&current.checkpoint.teleportCount, readPtr, sizeof(current.checkpoint.teleportCount)); readPtr += sizeof(current.checkpoint.teleportCount);
		}
		// clang-format on
	}

	delete[] decompressedData;

	// Read subtick data
	CompressedSectionHeader subtickHeader;
	g_pFullFileSystem->Read(&subtickHeader, sizeof(subtickHeader), file);

	char *compressedSubtick = new char[subtickHeader.compressedSize];
	if (!compressedSubtick)
	{
		return false;
	}

	g_pFullFileSystem->Read(compressedSubtick, subtickHeader.compressedSize, file);

	outSubtickData.resize(subtickHeader.elementCount);

	success = Decompress(compressedSubtick, subtickHeader.compressedSize, outSubtickData.data(), subtickHeader.uncompressedSize);

	delete[] compressedSubtick;

	return success;
}

// ========================================
// Weapon changes compression
// ========================================

bool KZ::replaysystem::compression::ReadWeaponChangesCompressed(FileHandle_t file, std::vector<WeaponSwitchEvent> &outWeaponEvents)
{
	// Read section header
	CompressedSectionHeader header;
	g_pFullFileSystem->Read(&header, sizeof(header), file);

	// Allocate buffer for compressed data
	char *compressedData = new char[header.compressedSize];
	if (!compressedData)
	{
		return false;
	}

	// Read compressed data
	g_pFullFileSystem->Read(compressedData, header.compressedSize, file);

	// Allocate buffer for decompressed data
	char *decompressedData = new char[header.uncompressedSize];
	if (!decompressedData)
	{
		delete[] compressedData;
		return false;
	}

	// Decompress
	bool success = Decompress(compressedData, header.compressedSize, decompressedData, header.uncompressedSize);

	delete[] compressedData;

	if (!success)
	{
		delete[] decompressedData;
		return false;
	}

	// Deserialize weapon events from buffer
	outWeaponEvents.clear();
	outWeaponEvents.reserve(header.elementCount);

	const char *readPtr = decompressedData;
	i32 numWeapons;
	memcpy(&numWeapons, readPtr, sizeof(numWeapons));
	readPtr += sizeof(numWeapons);

	for (i32 i = 0; i < numWeapons; i++)
	{
		WeaponSwitchEvent event = {};
		memcpy(&event.serverTick, readPtr, sizeof(event.serverTick));
		readPtr += sizeof(event.serverTick);
		memcpy(&event.econInfo.mainInfo, readPtr, sizeof(event.econInfo.mainInfo));
		readPtr += sizeof(event.econInfo.mainInfo);

		for (i32 j = 0; j < event.econInfo.mainInfo.numAttributes; j++)
		{
			memcpy(&event.econInfo.attributes[j], readPtr, sizeof(event.econInfo.attributes[j]));
			readPtr += sizeof(event.econInfo.attributes[j]);
		}

		outWeaponEvents.push_back(event);
	}

	delete[] decompressedData;
	return true;
}

i32 KZ::replaysystem::compression::WriteWeaponChangesCompressed(FileHandle_t file, const std::vector<WeaponSwitchEvent> &weaponEvents)
{
	i32 bytesWritten = 0;

	// Serialize weapon events to a buffer first
	std::vector<char> buffer;
	i32 numWeapons = weaponEvents.size();

	// Reserve approximate size
	size_t approxSize = sizeof(numWeapons);
	for (const auto &event : weaponEvents)
	{
		approxSize +=
			sizeof(event.serverTick) + sizeof(event.econInfo.mainInfo) + event.econInfo.mainInfo.numAttributes * sizeof(event.econInfo.attributes[0]);
	}
	buffer.reserve(approxSize);

	// Serialize to buffer
	AppendToBuffer(buffer, &numWeapons, sizeof(numWeapons));
	for (const auto &event : weaponEvents)
	{
		AppendToBuffer(buffer, &event.serverTick, sizeof(event.serverTick));
		AppendToBuffer(buffer, &event.econInfo.mainInfo, sizeof(event.econInfo.mainInfo));
		for (i32 j = 0; j < event.econInfo.mainInfo.numAttributes; j++)
		{
			AppendToBuffer(buffer, &event.econInfo.attributes[j], sizeof(event.econInfo.attributes[j]));
		}
	}

	// Compress
	void *compressedData = nullptr;
	size_t compressedSize = 0;

	bool success = Compress(buffer.data(), buffer.size(), &compressedData, &compressedSize);
	if (!success)
	{
		return 0;
	}

	// Write header and compressed data
	CompressedSectionHeader header;
	header.compressedSize = (u32)compressedSize;
	header.uncompressedSize = (u32)buffer.size();
	header.elementCount = (u32)weaponEvents.size();

	bytesWritten += g_pFullFileSystem->Write(&header, sizeof(header), file);
	bytesWritten += g_pFullFileSystem->Write(compressedData, compressedSize, file);

	delete[] static_cast<char *>(compressedData);

	return bytesWritten;
}

// ========================================
// Events compression
// ========================================

bool KZ::replaysystem::compression::ReadEventsCompressed(FileHandle_t file, std::vector<RpEvent> &outEvents)
{
	// Read section header
	CompressedSectionHeader header;
	g_pFullFileSystem->Read(&header, sizeof(header), file);

	// Allocate buffer for compressed data
	char *compressedData = new char[header.compressedSize];
	if (!compressedData)
	{
		return false;
	}

	// Read compressed data
	g_pFullFileSystem->Read(compressedData, header.compressedSize, file);

	// Resize output vector
	outEvents.resize(header.elementCount);

	// Decompress directly into output vector
	bool success = Decompress(compressedData, header.compressedSize, outEvents.data(), header.uncompressedSize);

	delete[] compressedData;

	return success;
}

i32 KZ::replaysystem::compression::WriteEventsCompressed(FileHandle_t file, const std::vector<RpEvent> &events)
{
	i32 bytesWritten = 0;

	void *compressedData = nullptr;
	size_t compressedSize = 0;
	size_t uncompressedSize = events.size() * sizeof(RpEvent);

	bool success = Compress(events.data(), uncompressedSize, &compressedData, &compressedSize);
	if (!success)
	{
		return 0;
	}

	CompressedSectionHeader header;
	header.compressedSize = (u32)compressedSize;
	header.uncompressedSize = (u32)uncompressedSize;
	header.elementCount = (u32)events.size();

	bytesWritten += g_pFullFileSystem->Write(&header, sizeof(header), file);
	bytesWritten += g_pFullFileSystem->Write(compressedData, compressedSize, file);

	delete[] static_cast<char *>(compressedData);

	return bytesWritten;
}

// ========================================
// Jumps compression
// ========================================

bool KZ::replaysystem::compression::ReadJumpsCompressed(FileHandle_t file, std::vector<RpJumpStats> &outJumps)
{
	// Read section header
	CompressedSectionHeader header;
	g_pFullFileSystem->Read(&header, sizeof(header), file);

	// Allocate buffer for compressed data
	char *compressedData = new char[header.compressedSize];
	if (!compressedData)
	{
		return false;
	}

	// Read compressed data
	g_pFullFileSystem->Read(compressedData, header.compressedSize, file);

	// Allocate buffer for decompressed data
	char *decompressedData = new char[header.uncompressedSize];
	if (!decompressedData)
	{
		delete[] compressedData;
		return false;
	}

	// Decompress
	bool success = Decompress(compressedData, header.compressedSize, decompressedData, header.uncompressedSize);

	delete[] compressedData;

	if (!success)
	{
		delete[] decompressedData;
		return false;
	}

	// Deserialize jumps from buffer
	outJumps.clear();
	outJumps.reserve(header.elementCount);

	const char *readPtr = decompressedData;
	i32 numJumps;
	memcpy(&numJumps, readPtr, sizeof(numJumps));
	readPtr += sizeof(numJumps);

	for (i32 i = 0; i < numJumps; i++)
	{
		RpJumpStats jump = {};

		// Read jump overall data
		memcpy(&jump.overall, readPtr, sizeof(jump.overall));
		readPtr += sizeof(jump.overall);

		// Read strafes
		i32 numStrafes;
		memcpy(&numStrafes, readPtr, sizeof(numStrafes));
		readPtr += sizeof(numStrafes);

		jump.strafes.resize(numStrafes);
		memcpy(jump.strafes.data(), readPtr, sizeof(RpJumpStats::StrafeData) * numStrafes);
		readPtr += sizeof(RpJumpStats::StrafeData) * numStrafes;

		// Read AA calls
		i32 numAACalls;
		memcpy(&numAACalls, readPtr, sizeof(numAACalls));
		readPtr += sizeof(numAACalls);

		jump.aaCalls.resize(numAACalls);
		memcpy(jump.aaCalls.data(), readPtr, sizeof(RpJumpStats::AAData) * numAACalls);
		readPtr += sizeof(RpJumpStats::AAData) * numAACalls;

		outJumps.push_back(jump);
	}

	delete[] decompressedData;
	return true;
}

i32 KZ::replaysystem::compression::WriteJumpsCompressed(FileHandle_t file, const std::vector<RpJumpStats> &jumps)
{
	i32 bytesWritten = 0;

	// Serialize jumps to a buffer first (since they have variable-sized vectors)
	std::vector<char> buffer;

	// Reserve approximate size
	size_t approxSize = sizeof(i32); // numJumps
	for (const auto &jump : jumps)
	{
		approxSize += sizeof(jump.overall);
		approxSize += sizeof(i32) + jump.strafes.size() * sizeof(RpJumpStats::StrafeData);
		approxSize += sizeof(i32) + jump.aaCalls.size() * sizeof(RpJumpStats::AAData);
	}
	buffer.reserve(approxSize);

	// Serialize to buffer
	i32 numJumps = jumps.size();
	AppendToBuffer(buffer, &numJumps, sizeof(numJumps));

	for (const auto &jump : jumps)
	{
		// Write jump overall data
		AppendToBuffer(buffer, &jump.overall, sizeof(jump.overall));

		// Write strafes
		i32 numStrafes = jump.strafes.size();
		AppendToBuffer(buffer, &numStrafes, sizeof(numStrafes));
		AppendToBuffer(buffer, jump.strafes.data(), sizeof(RpJumpStats::StrafeData) * numStrafes);

		// Write AA calls
		i32 numAACalls = jump.aaCalls.size();
		AppendToBuffer(buffer, &numAACalls, sizeof(numAACalls));
		AppendToBuffer(buffer, jump.aaCalls.data(), sizeof(RpJumpStats::AAData) * numAACalls);
	}

	// Compress
	void *compressedData = nullptr;
	size_t compressedSize = 0;

	bool success = Compress(buffer.data(), buffer.size(), &compressedData, &compressedSize);
	if (!success)
	{
		return 0;
	}

	// Write header and compressed data
	CompressedSectionHeader header;
	header.compressedSize = (u32)compressedSize;
	header.uncompressedSize = (u32)buffer.size();
	header.elementCount = (u32)jumps.size();

	bytesWritten += g_pFullFileSystem->Write(&header, sizeof(header), file);
	bytesWritten += g_pFullFileSystem->Write(compressedData, compressedSize, file);

	delete[] static_cast<char *>(compressedData);

	return bytesWritten;
}

// ========================================
// CmdData compression
// ========================================

// Bit flags for which CmdData fields have changed
enum CmdDataChangeFlags : u64
{
	CHANGED_CMD_SERVER_TICK = (1ULL << 0), // Only if difference != 1
	CHANGED_CMD_GAME_TIME = (1ULL << 1),
	CHANGED_CMD_REAL_TIME = (1ULL << 2),
	CHANGED_CMD_UNIX_TIME = (1ULL << 3),
	CHANGED_CMD_FRAMERATE = (1ULL << 4),
	CHANGED_CMD_LATENCY = (1ULL << 5),
	CHANGED_CMD_AVG_LOSS = (1ULL << 6),
	CHANGED_CMD_CMD_NUMBER = (1ULL << 7),
	CHANGED_CMD_CLIENT_TICK = (1ULL << 8),
	CHANGED_CMD_FORWARD = (1ULL << 9),
	CHANGED_CMD_LEFT = (1ULL << 10),
	CHANGED_CMD_UP = (1ULL << 11),
	CHANGED_CMD_BUTTONS_0 = (1ULL << 12),
	CHANGED_CMD_BUTTONS_1 = (1ULL << 13),
	CHANGED_CMD_BUTTONS_2 = (1ULL << 14),
	CHANGED_CMD_ANGLES = (1ULL << 15),
	CHANGED_CMD_MOUSEDX = (1ULL << 16),
	CHANGED_CMD_MOUSEDY = (1ULL << 17),
	CHANGED_CMD_SENSITIVITY = (1ULL << 18),
	CHANGED_CMD_M_YAW = (1ULL << 19),
	CHANGED_CMD_M_PITCH = (1ULL << 20),
};

bool KZ::replaysystem::compression::ReadCmdDataCompressed(FileHandle_t file, std::vector<CmdData> &outCmdData,
														  std::vector<SubtickData> &outCmdSubtickData)
{
	// Read cmd data section header
	CompressedSectionHeader header;
	g_pFullFileSystem->Read(&header, sizeof(header), file);

	// Allocate buffer for compressed data
	char *compressedData = new char[header.compressedSize];
	if (!compressedData)
	{
		return false;
	}

	// Read compressed data
	g_pFullFileSystem->Read(compressedData, header.compressedSize, file);

	// Allocate buffer for decompressed data
	char *decompressedData = new char[header.uncompressedSize];
	if (!decompressedData)
	{
		delete[] compressedData;
		return false;
	}

	// Decompress
	bool success = Decompress(compressedData, header.compressedSize, decompressedData, header.uncompressedSize);
	delete[] compressedData;

	if (!success)
	{
		delete[] decompressedData;
		return false;
	}

	// Reconstruct cmd data from delta-encoded buffer
	outCmdData.resize(header.elementCount);
	const char *readPtr = decompressedData;

	for (u32 i = 0; i < header.elementCount; i++)
	{
		CmdData &current = outCmdData[i];

		// Read change flags
		u64 flags;
		memcpy(&flags, readPtr, sizeof(flags));
		readPtr += sizeof(flags);

		// Copy from previous if not changed (or apply expected increment)
		if (i > 0)
		{
			// clang-format off
			// Server tick is expected to increment by 1
			if (!(flags & CHANGED_CMD_SERVER_TICK)) current.serverTick = outCmdData[i - 1].serverTick + 1;
			// gameTime is expected to increment by 1/64 (0.015625)
			if (!(flags & CHANGED_CMD_GAME_TIME)) current.gameTime = outCmdData[i - 1].gameTime + ENGINE_FIXED_TICK_INTERVAL;
			if (!(flags & CHANGED_CMD_REAL_TIME)) current.realTime = outCmdData[i - 1].realTime;
			if (!(flags & CHANGED_CMD_UNIX_TIME)) current.unixTime = outCmdData[i - 1].unixTime;
			if (!(flags & CHANGED_CMD_FRAMERATE)) current.framerate = outCmdData[i - 1].framerate;
			if (!(flags & CHANGED_CMD_LATENCY)) current.latency = outCmdData[i - 1].latency;
			if (!(flags & CHANGED_CMD_AVG_LOSS)) current.avgLoss = outCmdData[i - 1].avgLoss;
			if (!(flags & CHANGED_CMD_CMD_NUMBER)) current.cmdNumber = outCmdData[i - 1].cmdNumber;
			if (!(flags & CHANGED_CMD_CLIENT_TICK)) current.clientTick = outCmdData[i - 1].clientTick;
			if (!(flags & CHANGED_CMD_FORWARD)) current.forward = outCmdData[i - 1].forward;
			if (!(flags & CHANGED_CMD_LEFT)) current.left = outCmdData[i - 1].left;
			if (!(flags & CHANGED_CMD_UP)) current.up = outCmdData[i - 1].up;
			if (!(flags & CHANGED_CMD_BUTTONS_0)) current.buttons[0] = outCmdData[i - 1].buttons[0];
			if (!(flags & CHANGED_CMD_BUTTONS_1)) current.buttons[1] = outCmdData[i - 1].buttons[1];
			if (!(flags & CHANGED_CMD_BUTTONS_2)) current.buttons[2] = outCmdData[i - 1].buttons[2];
			if (!(flags & CHANGED_CMD_ANGLES)) current.angles = outCmdData[i - 1].angles;
			if (!(flags & CHANGED_CMD_MOUSEDX)) current.mousedx = outCmdData[i - 1].mousedx;
			if (!(flags & CHANGED_CMD_MOUSEDY)) current.mousedy = outCmdData[i - 1].mousedy;
			if (!(flags & CHANGED_CMD_SENSITIVITY)) current.sensitivity = outCmdData[i - 1].sensitivity;
			if (!(flags & CHANGED_CMD_M_YAW)) current.m_yaw = outCmdData[i - 1].m_yaw;
			if (!(flags & CHANGED_CMD_M_PITCH)) current.m_pitch = outCmdData[i - 1].m_pitch;
			// clang-format on
		}

		// Read changed fields
		// clang-format off
		if (flags & CHANGED_CMD_SERVER_TICK) { memcpy(&current.serverTick, readPtr, sizeof(current.serverTick)); readPtr += sizeof(current.serverTick); }
		if (flags & CHANGED_CMD_GAME_TIME) { memcpy(&current.gameTime, readPtr, sizeof(current.gameTime)); readPtr += sizeof(current.gameTime); }
		if (flags & CHANGED_CMD_REAL_TIME) { memcpy(&current.realTime, readPtr, sizeof(current.realTime)); readPtr += sizeof(current.realTime); }
		if (flags & CHANGED_CMD_UNIX_TIME) { memcpy(&current.unixTime, readPtr, sizeof(current.unixTime)); readPtr += sizeof(current.unixTime); }
		if (flags & CHANGED_CMD_FRAMERATE) { memcpy(&current.framerate, readPtr, sizeof(current.framerate)); readPtr += sizeof(current.framerate); }
		if (flags & CHANGED_CMD_LATENCY) { memcpy(&current.latency, readPtr, sizeof(current.latency)); readPtr += sizeof(current.latency); }
		if (flags & CHANGED_CMD_AVG_LOSS) { memcpy(&current.avgLoss, readPtr, sizeof(current.avgLoss)); readPtr += sizeof(current.avgLoss); }
		if (flags & CHANGED_CMD_CMD_NUMBER) { memcpy(&current.cmdNumber, readPtr, sizeof(current.cmdNumber)); readPtr += sizeof(current.cmdNumber); }
		if (flags & CHANGED_CMD_CLIENT_TICK) { memcpy(&current.clientTick, readPtr, sizeof(current.clientTick)); readPtr += sizeof(current.clientTick); }
		if (flags & CHANGED_CMD_FORWARD) { memcpy(&current.forward, readPtr, sizeof(current.forward)); readPtr += sizeof(current.forward); }
		if (flags & CHANGED_CMD_LEFT) { memcpy(&current.left, readPtr, sizeof(current.left)); readPtr += sizeof(current.left); }
		if (flags & CHANGED_CMD_UP) { memcpy(&current.up, readPtr, sizeof(current.up)); readPtr += sizeof(current.up); }
		if (flags & CHANGED_CMD_BUTTONS_0) { memcpy(&current.buttons[0], readPtr, sizeof(current.buttons[0])); readPtr += sizeof(current.buttons[0]); }
		if (flags & CHANGED_CMD_BUTTONS_1) { memcpy(&current.buttons[1], readPtr, sizeof(current.buttons[1])); readPtr += sizeof(current.buttons[1]); }
		if (flags & CHANGED_CMD_BUTTONS_2) { memcpy(&current.buttons[2], readPtr, sizeof(current.buttons[2])); readPtr += sizeof(current.buttons[2]); }
		if (flags & CHANGED_CMD_ANGLES) { memcpy(&current.angles, readPtr, sizeof(current.angles)); readPtr += sizeof(current.angles); }
		if (flags & CHANGED_CMD_MOUSEDX) { memcpy(&current.mousedx, readPtr, sizeof(current.mousedx)); readPtr += sizeof(current.mousedx); }
		if (flags & CHANGED_CMD_MOUSEDY) { memcpy(&current.mousedy, readPtr, sizeof(current.mousedy)); readPtr += sizeof(current.mousedy); }
		if (flags & CHANGED_CMD_SENSITIVITY) { memcpy(&current.sensitivity, readPtr, sizeof(current.sensitivity)); readPtr += sizeof(current.sensitivity); }
		if (flags & CHANGED_CMD_M_YAW) { memcpy(&current.m_yaw, readPtr, sizeof(current.m_yaw)); readPtr += sizeof(current.m_yaw); }
		if (flags & CHANGED_CMD_M_PITCH) { memcpy(&current.m_pitch, readPtr, sizeof(current.m_pitch)); readPtr += sizeof(current.m_pitch); }
		// clang-format on
	}

	delete[] decompressedData;

	// Read cmd subtick data
	CompressedSectionHeader subtickHeader;
	g_pFullFileSystem->Read(&subtickHeader, sizeof(subtickHeader), file);

	char *compressedSubtick = new char[subtickHeader.compressedSize];
	if (!compressedSubtick)
	{
		return false;
	}

	g_pFullFileSystem->Read(compressedSubtick, subtickHeader.compressedSize, file);

	outCmdSubtickData.resize(subtickHeader.elementCount);

	success = Decompress(compressedSubtick, subtickHeader.compressedSize, outCmdSubtickData.data(), subtickHeader.uncompressedSize);

	delete[] compressedSubtick;

	return success;
}

i32 KZ::replaysystem::compression::WriteCmdDataCompressed(FileHandle_t file, const std::vector<CmdData> &cmdData,
														  const std::vector<SubtickData> &cmdSubtickData)
{
	i32 bytesWritten = 0;
	std::vector<char> buffer;

	// Reserve approximate size
	buffer.reserve(cmdData.size() * 120); // Rough estimate

	for (size_t i = 0; i < cmdData.size(); i++)
	{
		const CmdData &current = cmdData[i];

		// Build change flags
		u64 flags = 0;

		// clang-format off
		// Server tick: only write if difference is not 1
		u32 expectedServerTick = (i == 0) ? 0 : cmdData[i - 1].serverTick + 1;
		if (current.serverTick != expectedServerTick) flags |= CHANGED_CMD_SERVER_TICK;
		// gameTime is expected to increment by 1/64 (0.015625)
		f32 expectedGameTime = (i == 0) ? 0.0f : cmdData[i - 1].gameTime + ENGINE_FIXED_TICK_INTERVAL;
		if (current.gameTime != expectedGameTime) flags |= CHANGED_CMD_GAME_TIME;
		if (i == 0 || current.realTime != cmdData[i - 1].realTime) flags |= CHANGED_CMD_REAL_TIME;
		if (i == 0 || current.unixTime != cmdData[i - 1].unixTime) flags |= CHANGED_CMD_UNIX_TIME;
		if (i == 0 || current.framerate != cmdData[i - 1].framerate) flags |= CHANGED_CMD_FRAMERATE;
		if (i == 0 || current.latency != cmdData[i - 1].latency) flags |= CHANGED_CMD_LATENCY;
		if (i == 0 || current.avgLoss != cmdData[i - 1].avgLoss) flags |= CHANGED_CMD_AVG_LOSS;
		if (i == 0 || current.cmdNumber != cmdData[i - 1].cmdNumber) flags |= CHANGED_CMD_CMD_NUMBER;
		if (i == 0 || current.clientTick != cmdData[i - 1].clientTick) flags |= CHANGED_CMD_CLIENT_TICK;
		if (i == 0 || current.forward != cmdData[i - 1].forward) flags |= CHANGED_CMD_FORWARD;
		if (i == 0 || current.left != cmdData[i - 1].left) flags |= CHANGED_CMD_LEFT;
		if (i == 0 || current.up != cmdData[i - 1].up) flags |= CHANGED_CMD_UP;
		if (i == 0 || current.buttons[0] != cmdData[i - 1].buttons[0]) flags |= CHANGED_CMD_BUTTONS_0;
		if (i == 0 || current.buttons[1] != cmdData[i - 1].buttons[1]) flags |= CHANGED_CMD_BUTTONS_1;
		if (i == 0 || current.buttons[2] != cmdData[i - 1].buttons[2]) flags |= CHANGED_CMD_BUTTONS_2;
		if (i == 0 || current.angles != cmdData[i - 1].angles) flags |= CHANGED_CMD_ANGLES;
		if (i == 0 || current.mousedx != cmdData[i - 1].mousedx) flags |= CHANGED_CMD_MOUSEDX;
		if (i == 0 || current.mousedy != cmdData[i - 1].mousedy) flags |= CHANGED_CMD_MOUSEDY;
		if (i == 0 || current.sensitivity != cmdData[i - 1].sensitivity) flags |= CHANGED_CMD_SENSITIVITY;
		if (i == 0 || current.m_yaw != cmdData[i - 1].m_yaw) flags |= CHANGED_CMD_M_YAW;
		if (i == 0 || current.m_pitch != cmdData[i - 1].m_pitch) flags |= CHANGED_CMD_M_PITCH;

		// Write change flags
		AppendToBuffer(buffer, &flags, sizeof(flags));

		// Write changed fields
		if (flags & CHANGED_CMD_SERVER_TICK) AppendToBuffer(buffer, &current.serverTick, sizeof(current.serverTick));
		if (flags & CHANGED_CMD_GAME_TIME) AppendToBuffer(buffer, &current.gameTime, sizeof(current.gameTime));
		if (flags & CHANGED_CMD_REAL_TIME) AppendToBuffer(buffer, &current.realTime, sizeof(current.realTime));
		if (flags & CHANGED_CMD_UNIX_TIME) AppendToBuffer(buffer, &current.unixTime, sizeof(current.unixTime));
		if (flags & CHANGED_CMD_FRAMERATE) AppendToBuffer(buffer, &current.framerate, sizeof(current.framerate));
		if (flags & CHANGED_CMD_LATENCY) AppendToBuffer(buffer, &current.latency, sizeof(current.latency));
		if (flags & CHANGED_CMD_AVG_LOSS) AppendToBuffer(buffer, &current.avgLoss, sizeof(current.avgLoss));
		if (flags & CHANGED_CMD_CMD_NUMBER) AppendToBuffer(buffer, &current.cmdNumber, sizeof(current.cmdNumber));
		if (flags & CHANGED_CMD_CLIENT_TICK) AppendToBuffer(buffer, &current.clientTick, sizeof(current.clientTick));
		if (flags & CHANGED_CMD_FORWARD) AppendToBuffer(buffer, &current.forward, sizeof(current.forward));
		if (flags & CHANGED_CMD_LEFT) AppendToBuffer(buffer, &current.left, sizeof(current.left));
		if (flags & CHANGED_CMD_UP) AppendToBuffer(buffer, &current.up, sizeof(current.up));
		if (flags & CHANGED_CMD_BUTTONS_0) AppendToBuffer(buffer, &current.buttons[0], sizeof(current.buttons[0]));
		if (flags & CHANGED_CMD_BUTTONS_1) AppendToBuffer(buffer, &current.buttons[1], sizeof(current.buttons[1]));
		if (flags & CHANGED_CMD_BUTTONS_2) AppendToBuffer(buffer, &current.buttons[2], sizeof(current.buttons[2]));
		if (flags & CHANGED_CMD_ANGLES) AppendToBuffer(buffer, &current.angles, sizeof(current.angles));
		if (flags & CHANGED_CMD_MOUSEDX) AppendToBuffer(buffer, &current.mousedx, sizeof(current.mousedx));
		if (flags & CHANGED_CMD_MOUSEDY) AppendToBuffer(buffer, &current.mousedy, sizeof(current.mousedy));
		if (flags & CHANGED_CMD_SENSITIVITY) AppendToBuffer(buffer, &current.sensitivity, sizeof(current.sensitivity));
		if (flags & CHANGED_CMD_M_YAW) AppendToBuffer(buffer, &current.m_yaw, sizeof(current.m_yaw));
		if (flags & CHANGED_CMD_M_PITCH) AppendToBuffer(buffer, &current.m_pitch, sizeof(current.m_pitch));
		// clang-format on
	}

	// Compress the buffer
	void *compressedData = nullptr;
	size_t compressedSize = 0;

	bool success = Compress(buffer.data(), buffer.size(), &compressedData, &compressedSize);
	if (!success)
	{
		return 0;
	}

	CompressedSectionHeader header;
	header.compressedSize = (u32)compressedSize;
	header.uncompressedSize = (u32)buffer.size();
	header.elementCount = (u32)cmdData.size();

	bytesWritten += g_pFullFileSystem->Write(&header, sizeof(header), file);
	bytesWritten += g_pFullFileSystem->Write(compressedData, compressedSize, file);

	delete[] static_cast<char *>(compressedData);

	// Compress cmd subtick data
	void *compressedSubtick = nullptr;
	size_t compressedSubtickSize = 0;
	size_t uncompressedSubtickSize = cmdSubtickData.size() * sizeof(SubtickData);

	success = Compress(cmdSubtickData.data(), uncompressedSubtickSize, &compressedSubtick, &compressedSubtickSize);

	if (success)
	{
		CompressedSectionHeader subtickHeader;
		subtickHeader.compressedSize = (u32)compressedSubtickSize;
		subtickHeader.uncompressedSize = (u32)uncompressedSubtickSize;
		subtickHeader.elementCount = (u32)cmdSubtickData.size();

		bytesWritten += g_pFullFileSystem->Write(&subtickHeader, sizeof(subtickHeader), file);
		bytesWritten += g_pFullFileSystem->Write(compressedSubtick, compressedSubtickSize, file);

		delete[] static_cast<char *>(compressedSubtick);
	}

	return bytesWritten;
}
