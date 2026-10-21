#pragma once

#include "kz_replay.h"
#include <functional>

namespace KZ::replaysystem::parsing
{
	struct VersionParser;
} // namespace KZ::replaysystem::parsing

namespace KZ::replaysystem::compression
{
	// Compressed section header (written before each compressed data block). v1-6 replays used u32 fields instead.
	struct CompressedSectionHeader
	{
		u64 compressedSize;   // Size of compressed data
		u64 uncompressedSize; // Original size of data
		u64 elementCount;     // Number of elements (e.g., tick count)
	};

	// The ticks of a loaded replay, kept delta-encoded as they are on disk. Every KEYFRAME_INTERVAL-th tick is also kept in
	// full, so a read decodes forward from the keyframe before it, and reading the tick after the last one decodes just that one.
	// The last tick read and the one before it are cached. Main thread only.
	class TickStore
	{
	public:
		static constexpr u32 KEYFRAME_INTERVAL = 64;

		u32 Count() const
		{
			return count;
		}

		// index must be below Count().
		TickData Get(u32 index);
		// The first tick recorded on or after serverTick, or the last tick if there is none. Count() must not be 0.
		u32 FindServerTick(u32 serverTick);

		// The ticks where the teleport count goes up, found while loading.
		const std::vector<u32> &TeleportTicks() const
		{
			return teleportTicks;
		}

	private:
		friend bool ReadTickStore(const char *&cursor, const char *end, TickStore &outTicks, std::vector<u8> &outSubtickCounts,
								  std::vector<SubtickData::RpSubtickMove> &outSubtickMoves, u32 replayVersion);

		struct Keyframe
		{
			TickData tick;
			// Where the tick after it starts in deltas.
			u64 nextOffset;
		};

		std::vector<char> deltas;
		std::vector<Keyframe> keyframes;
		std::vector<u32> teleportTicks;
		const parsing::VersionParser *parser = nullptr;
		u32 count = 0;

		TickData current {};
		TickData previous {};
		u32 currentIndex = 0;
		u64 currentEnd = 0;
		bool hasCurrent = false;
		bool hasPrevious = false;
	};

	// Reads the tick and subtick sections into a TickStore for playback.
	bool ReadTickStore(const char *&cursor, const char *end, TickStore &outTicks, std::vector<u8> &outSubtickCounts,
					   std::vector<SubtickData::RpSubtickMove> &outSubtickMoves, u32 replayVersion);

	// Compress a buffer using zstd level 3
	bool Compress(const void *src, size_t srcSize, void **outDst, size_t *outDstSize);

	// Decompress a buffer using zstd
	bool Decompress(const void *src, size_t srcSize, void *dst, size_t dstSize);

	// Subtick data travels packed: one move count per tick and the moves of all ticks back to back,
	// because most ticks have no subtick moves at all.

	// One compressed section assembled from consecutive pieces, each compressed as its own zstd frame.
	struct SectionBuilder
	{
		std::vector<char> frames;
		u64 uncompressedSize = 0;
		u64 elementCount = 0;
	};

	// Writes the tick, subtick, cmd and cmd subtick sections of a recording that arrives in consecutive pieces, such as the
	// chunks a long recording flushes to disk. Only the compressed sections and the current piece have to be in memory.
	class RecordingWriter
	{
	public:
		bool AddTicks(const std::vector<TickData> &ticks, const std::vector<u8> &subtickCounts, const std::vector<SubtickData::RpSubtickMove> &subtickMoves);
		bool AddCmds(const std::vector<CmdData> &cmds, const std::vector<u8> &cmdSubtickCounts,
					 const std::vector<SubtickData::RpSubtickMove> &cmdSubtickMoves);

		// The tick and subtick sections follow the replay header; the cmd and cmd subtick sections come last.
		bool AppendTickSections(std::vector<char> &outBuffer);
		bool AppendCmdSections(std::vector<char> &outBuffer);

		size_t CompressedSize() const
		{
			return tickSection.frames.size() + subtickSection.frames.size() + cmdSection.frames.size() + cmdSubtickSection.frames.size();
		}

	private:
		SectionBuilder tickSection;
		SectionBuilder subtickSection;
		SectionBuilder cmdSection;
		SectionBuilder cmdSubtickSection;
		// The delta encoding of each piece continues from the last element of the previous one.
		TickData lastTick {};
		bool hasLastTick = false;
		CmdData lastCmd {};
		bool hasLastCmd = false;
	};

	// Write compressed tick data with delta encoding
	bool WriteTickDataCompressed(std::vector<char> &outBuffer, const std::vector<TickData> &tickData, const std::vector<u8> &subtickCounts,
								 const std::vector<SubtickData::RpSubtickMove> &subtickMoves);

	// The route reader receives decoded ticks without retaining full tick/subtick arrays.
	// Returning false aborts decoding, for example when a map change cancels loading.
	using TickVisitor = std::function<bool(const TickData &)>;

	// Read compressed tick data with delta decoding
	bool ReadTickDataCompressed(const char *&cursor, const char *end, std::vector<TickData> &outTickData, std::vector<u8> &outSubtickCounts,
								std::vector<SubtickData::RpSubtickMove> &outSubtickMoves, u32 replayVersion);

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
	bool ReadCmdDataCompressed(const char *&cursor, const char *end, std::vector<CmdData> &outCmdData, std::vector<u8> &outCmdSubtickCounts,
							   std::vector<SubtickData::RpSubtickMove> &outCmdSubtickMoves, u32 replayVersion);

	// Write compressed weapon changes
	bool WriteWeaponsCompressed(std::vector<char> &outBuffer, const std::vector<std::pair<i32, EconInfo>> &weaponTable);

	// Write compressed events
	bool WriteEventsCompressed(std::vector<char> &outBuffer, const std::vector<RpEvent> &events);

	// Write compressed jumps
	bool WriteJumpsCompressed(std::vector<char> &outBuffer, const std::vector<RpJumpStats> &jumps);

	// Write compressed CmdData
	bool WriteCmdDataCompressed(std::vector<char> &outBuffer, const std::vector<CmdData> &cmdData, const std::vector<u8> &cmdSubtickCounts,
								const std::vector<SubtickData::RpSubtickMove> &cmdSubtickMoves);
} // namespace KZ::replaysystem::compression
