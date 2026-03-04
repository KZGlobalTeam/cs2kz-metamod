#pragma once

#include "kz_replay.h"

namespace KZ::replaysystem::compression
{
	// Compressed section header (written before each compressed data block)
	struct CompressedSectionHeader
	{
		u32 compressedSize;   // Size of compressed data
		u32 uncompressedSize; // Original size of data
		u32 elementCount;     // Number of elements (e.g., tick count)
	};

	// Compress a buffer using zstd level 3
	bool Compress(const void *src, size_t srcSize, void **outDst, size_t *outDstSize);

	// Decompress a buffer using zstd
	bool Decompress(const void *src, size_t srcSize, void *dst, size_t dstSize);

	// Write compressed tick data with delta encoding
	i32 WriteTickDataCompressed(std::vector<char> &outBuffer, const std::vector<TickData> &tickData, const std::vector<SubtickData> &subtickData);

	// Read compressed tick data with delta decoding
	bool ReadTickDataCompressed(const char *&cursor, const char *end, std::vector<TickData> &outTickData, std::vector<SubtickData> &outSubtickData);

	// Read compressed weapon changes
	bool ReadWeaponsCompressed(const char *&cursor, const char *end, std::vector<std::pair<i32, EconInfo>> &outWeaponTable);

	// Read compressed events
	bool ReadEventsCompressed(const char *&cursor, const char *end, std::vector<RpEvent> &outEvents);

	// Read compressed jumps
	bool ReadJumpsCompressed(const char *&cursor, const char *end, std::vector<RpJumpStats> &outJumps);

	// Read compressed CmdData
	bool ReadCmdDataCompressed(const char *&cursor, const char *end, std::vector<CmdData> &outCmdData, std::vector<SubtickData> &outCmdSubtickData);

	// Write compressed weapon changes
	i32 WriteWeaponsCompressed(std::vector<char> &outBuffer, const std::vector<std::pair<i32, EconInfo>> &weaponTable);

	// Write compressed events
	i32 WriteEventsCompressed(std::vector<char> &outBuffer, const std::vector<RpEvent> &events);

	// Write compressed jumps
	i32 WriteJumpsCompressed(std::vector<char> &outBuffer, const std::vector<RpJumpStats> &jumps);

	// Write compressed CmdData
	i32 WriteCmdDataCompressed(std::vector<char> &outBuffer, const std::vector<CmdData> &cmdData, const std::vector<SubtickData> &cmdSubtickData);
} // namespace KZ::replaysystem::compression
