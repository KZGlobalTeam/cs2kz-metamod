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
	i32 WriteTickDataCompressed(FileHandle_t file, const std::vector<TickData> &tickData, const std::vector<SubtickData> &subtickData);

	// Read compressed tick data with delta decoding
	bool ReadTickDataCompressed(FileHandle_t file, std::vector<TickData> &outTickData, std::vector<SubtickData> &outSubtickData);

	// Read compressed weapon changes
	bool ReadWeaponChangesCompressed(FileHandle_t file, std::vector<WeaponSwitchEvent> &outWeaponEvents, std::vector<EconInfo> &outWeaponTable);

	// Read compressed events
	bool ReadEventsCompressed(FileHandle_t file, std::vector<RpEvent> &outEvents);

	// Read compressed jumps
	bool ReadJumpsCompressed(FileHandle_t file, std::vector<RpJumpStats> &outJumps);

	// Read compressed CmdData
	bool ReadCmdDataCompressed(FileHandle_t file, std::vector<CmdData> &outCmdData, std::vector<SubtickData> &outCmdSubtickData);

	// Write compressed weapon changes
	i32 WriteWeaponChangesCompressed(FileHandle_t file, const std::vector<WeaponSwitchEvent> &weaponEvents, const std::vector<EconInfo> &weaponTable);

	// Write compressed events
	i32 WriteEventsCompressed(FileHandle_t file, const std::vector<RpEvent> &events);

	// Write compressed jumps
	i32 WriteJumpsCompressed(FileHandle_t file, const std::vector<RpJumpStats> &jumps);

	// Write compressed CmdData
	i32 WriteCmdDataCompressed(FileHandle_t file, const std::vector<CmdData> &cmdData, const std::vector<SubtickData> &cmdSubtickData);
} // namespace KZ::replaysystem::compression
