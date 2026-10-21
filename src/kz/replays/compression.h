#pragma once

#include "kz_replay.h"
#include <functional>

namespace KZ::replaysystem::compression
{
	// Compressed section header (written before each compressed data block). v1-6 replays used u32 fields instead.
	struct CompressedSectionHeader
	{
		u64 compressedSize;   // Size of compressed data
		u64 uncompressedSize; // Original size of data
		u64 elementCount;     // Number of elements (e.g., tick count)
	};

	// Compress a buffer using zstd level 3
	bool Compress(const void *src, size_t srcSize, void **outDst, size_t *outDstSize);

	// Decompress a buffer using zstd
	bool Decompress(const void *src, size_t srcSize, void *dst, size_t dstSize);

	// Write compressed tick data with delta encoding
	bool WriteTickDataCompressed(std::vector<char> &outBuffer, const std::vector<TickData> &tickData, const std::vector<SubtickData> &subtickData);

	// The route reader receives decoded ticks without retaining full tick/subtick arrays.
	// Returning false aborts decoding, for example when a map change cancels loading.
	using TickVisitor = std::function<bool(const TickData &)>;

	// Read compressed tick data with delta decoding
	bool ReadTickDataCompressed(const char *&cursor, const char *end, std::vector<TickData> &outTickData, std::vector<SubtickData> &outSubtickData,
								u32 replayVersion);

	// Visits movement ticks without allocating playback tick or subtick arrays.
	bool ReadTickDataCompressed(const char *&cursor, const char *end, u32 replayVersion, const TickVisitor &visitor);

	// Bounds-checks and advances past one compressed block without decompressing it.
	bool SkipCompressedSection(const char *&cursor, const char *end, u32 replayVersion);

	// Read compressed weapon changes
	bool ReadWeaponsCompressed(const char *&cursor, const char *end, std::vector<std::pair<i32, EconInfo>> &outWeaponTable, u32 replayVersion);

	// Read compressed events
	bool ReadEventsCompressed(const char *&cursor, const char *end, std::vector<RpEvent> &outEvents, u32 replayVersion);

	// Read compressed jumps
	bool ReadJumpsCompressed(const char *&cursor, const char *end, std::vector<RpJumpStats> &outJumps, u32 replayVersion);

	// Read compressed CmdData
	bool ReadCmdDataCompressed(const char *&cursor, const char *end, std::vector<CmdData> &outCmdData, std::vector<SubtickData> &outCmdSubtickData,
							   u32 replayVersion);

	// Write compressed weapon changes
	bool WriteWeaponsCompressed(std::vector<char> &outBuffer, const std::vector<std::pair<i32, EconInfo>> &weaponTable);

	// Write compressed events
	bool WriteEventsCompressed(std::vector<char> &outBuffer, const std::vector<RpEvent> &events);

	// Write compressed jumps
	bool WriteJumpsCompressed(std::vector<char> &outBuffer, const std::vector<RpJumpStats> &jumps);

	// Write compressed CmdData
	bool WriteCmdDataCompressed(std::vector<char> &outBuffer, const std::vector<CmdData> &cmdData, const std::vector<SubtickData> &cmdSubtickData);
} // namespace KZ::replaysystem::compression
