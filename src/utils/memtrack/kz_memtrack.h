#pragma once

#include "kz_memtrack_tls.h"
#include "utils/logging.h"

#ifdef KZ_MEMTRACK_ENABLED

// CS2KZ allocation tracking.
//
// Hooks the engine's IMemAlloc (every allocation the plugin makes reaches it, including the ones
// tier0 makes on our behalf for CUtlVector), keeps the ones that happen while cs2kz is on the
// stack, and records each live block against the call site that asked for it - so
// `kz_meminfo leaks` can answer "what did we allocate and never free".

namespace KZMem
{
	static constexpr uint32_t INVALID_CALLSITE = 0xFFFFFFFFu;

	//-------------------------------------------------------------------------------------------
	// Lifecycle
	//-------------------------------------------------------------------------------------------

	// Resolves the IMemAlloc vtable indices via SourceHook, validates them against tier0, and
	// installs the swap. Fails closed: on any validation failure it logs a warning, installs
	// nothing, and returns false, leaving the plugin to run normally.
	// Must be called after modules::Initialize().
	bool Install();

	// Restores the original vtable pointers, then blocks until no thread is still inside one of
	// our thunks. Skipping the drain would let a thread execute freed code when the module unmaps.
	void Uninstall();

	// Human-readable reason why Install() failed, or why tracking is inactive.
	const char *GetStatusString();

	// Install() runs before the logging channels exist, so it stays silent and this reports the
	// outcome once logging is up.
	void LogStatus();

	//-------------------------------------------------------------------------------------------
	// Bookkeeping, called from the hook thunks
	//-------------------------------------------------------------------------------------------

	void OnAlloc(void *ptr, size_t size, const void *callsite);
	void OnFree(void *ptr);
	void NoteSampledWalk();

	// Reserves the side table's address space and caches this module's address range.
	// Called by Install() before the vtable swap goes in.
	bool AllocateTable();
	void ReleaseTable();

	// True when the address lies inside this module.
	bool IsSelfAddress(const void *address);
	uintptr_t GetSelfBase();

	//-------------------------------------------------------------------------------------------
	// Reporting
	//-------------------------------------------------------------------------------------------

	struct Totals
	{
		int64_t liveBytes;
		int64_t peakBytes;
		int64_t liveBlocks;
		uint64_t totalBytes;
		uint64_t totalAllocs;
		uint64_t totalFrees;
		uint64_t untrackedFrees; // freed by us but never recorded (allocated before install)
		uint64_t orphanedFrees;  // ours, but freed by another module - detected on pointer reuse
		uint64_t tableEntries;   // live entries in the side table
		uint64_t tableCapacity;
		uint64_t sampledWalks; // how many stack walks the sampler actually performed
		bool overflowed;       // side table hit capacity; counts are now incomplete
	};

	struct CallsiteStats
	{
		const void *address;
		int64_t liveBytes;
		int64_t liveBlocks;
		uint64_t totalBytes;
		uint64_t totalAllocs;
	};

	Totals GetTotals();

	// Fills out[] with up to maxCount call sites sorted by live bytes, descending.
	uint32_t GetTopCallsites(CallsiteStats *out, uint32_t maxCount);

	// Same, but restricted to blocks allocated after the last SetMark() and still live -
	// i.e. leak candidates.
	uint32_t GetLeaksSinceMark(CallsiteStats *out, uint32_t maxCount);

	// Snapshots the current allocation serial. GetLeaksSinceMark() reports blocks newer than it.
	void SetMark();
	bool HasMark();

	// Clears peak and churn counters. Live blocks and the side table are left alone.
	void ResetCounters();

	// Converts an address to "cs2kz+0x1234" style text in the caller's buffer.
	void FormatAddress(const void *address, char *buffer, size_t bufferSize);

	//-------------------------------------------------------------------------------------------
	// Call-site sampling
	//
	// Allocations that arrive without a baton - CUtlVector growth via tier0's
	// UtlVectorMemory_Alloc, CUtlString via memoverride's malloc - present a return address that
	// belongs to the plumbing rather than to the code that wanted the memory. Recovering the real
	// caller there needs a stack walk, which costs ~400ns and cannot be paid on every allocation.
	//
	// So it is sampled: one walk every N such allocations. Leak *volume* stays exact because every
	// block is recorded either way; only call-site attribution for those paths is statistical.
	// Set the rate to 1 while actively hunting a leak to make it exact.
	//-------------------------------------------------------------------------------------------

	void SetSampleRate(uint32_t oneInN);
	uint32_t GetSampleRate();
} // namespace KZMem

#else // KZ_MEMTRACK_ENABLED

// Stubs for the mode and style binaries, which share headers with the main plugin but do not
// compile the tracker.
namespace KZMem
{
	inline bool Install()
	{
		return false;
	}

	inline void Uninstall() {}

	inline void LogStatus() {}

	inline bool IsActive()
	{
		return false;
	}

	inline const char *GetStatusString()
	{
		return "not built into this binary";
	}
} // namespace KZMem

#endif // KZ_MEMTRACK_ENABLED
