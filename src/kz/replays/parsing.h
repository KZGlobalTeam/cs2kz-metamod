#pragma once

#include "kz_replay.h"

namespace KZ::replaysystem::parsing
{
	// Wire differences are selected once for each version, outside decoding loops.
	struct VersionParser
	{
		u64 weaponFlag;
		u64 modernActualFlag;
		u64 modernUsableFlag;
		u64 modernLandedFlag;
		bool modernJump;
		bool compactSubticks;
		bool jumpFractions;
		bool (*readEvents)(const char *, size_t, u32, std::vector<RpEvent> &);
	};

	// Returns the decoder for an existing version, or nullptr for unsupported files.
	const VersionParser *GetVersionParser(u32 version);

	// Writes extensible, length-prefixed v6 events; never writes native struct bytes.
	bool EncodeEventsV6(const std::vector<RpEvent> &events, std::vector<char> &bytes);
} // namespace KZ::replaysystem::parsing
