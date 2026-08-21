#include "kz_memtrack.h"
#include "utils/plat.h"
#include "strtools.h"

#include <atomic>
#include <cstring>

#ifdef _WIN32
#include <emmintrin.h>
#endif

// NOTE: deliberately no "tier0/memdbgon.h" here. This file must not have its allocation calls
// rewritten, and it must not allocate through g_pMemAlloc at all - every byte it owns comes from
// Plat_ReservePages/Plat_CommitPages so that bookkeeping can never recurse into the allocator it
// is measuring.

namespace KZMem
{
	thread_local uint32_t tl_inKZ = 0;
	thread_local const void *tl_callsite = nullptr;
	thread_local bool tl_inHook = false;

	namespace
	{
		//---------------------------------------------------------------------------------------
		// Sizing
		//---------------------------------------------------------------------------------------

		// 64 shards x 16384 entries x 32 bytes = 32 MB of address space, committed one 512 KB
		// shard at a time on first use. That holds ~734k live blocks at the 0.7 load factor;
		// past that the tracker fails soft rather than growing without bound.
		constexpr uint32_t SHARD_COUNT = 64;
		constexpr uint32_t SHARD_MASK = SHARD_COUNT - 1;
		constexpr uint32_t ENTRIES_PER_SHARD = 1u << 14;
		constexpr uint32_t ENTRY_MASK = ENTRIES_PER_SHARD - 1;
		constexpr uint32_t MAX_LOAD_PER_SHARD = (ENTRIES_PER_SHARD * 7) / 10;

		constexpr uint32_t CALLSITE_COUNT = 1u << 12;
		constexpr uint32_t CALLSITE_MASK = CALLSITE_COUNT - 1;

		struct Entry
		{
			void *ptr; // nullptr == empty slot
			uint64_t size;
			uint64_t serial;
			uint32_t callsiteIdx;
			uint32_t reserved;
		};

		static_assert(sizeof(Entry) == 32, "Entry should stay at 32 bytes - two per cache line");

		constexpr size_t SHARD_BYTES = (size_t)ENTRIES_PER_SHARD * sizeof(Entry);
		constexpr size_t TABLE_BYTES = SHARD_BYTES * SHARD_COUNT;

		// alignas(64) on the struct already rounds sizeof up to a whole cache line, which is the
		// point: several threads allocate concurrently and must not share a line.
		struct alignas(64) Shard
		{
			std::atomic<uint32_t> lock;
			Entry *entries;
			uint32_t count;
		};

		static_assert(sizeof(Shard) == 64, "Shard must occupy exactly one cache line");

		struct CallsiteEntry
		{
			std::atomic<uintptr_t> addr; // 0 == empty
			std::atomic<int64_t> liveBytes;
			std::atomic<int64_t> liveBlocks;
			std::atomic<uint64_t> totalBytes;
			std::atomic<uint64_t> totalAllocs;
		};

		//---------------------------------------------------------------------------------------
		// State. All statically zero-initialised POD: the hook can fire during another translation
		// unit's static init, long before any constructor of ours would have run.
		//---------------------------------------------------------------------------------------

		Shard s_shards[SHARD_COUNT];
		CallsiteEntry s_callsites[CALLSITE_COUNT];

		// Global totals. Unlike the per-subsystem layout this replaced, these are touched only for
		// allocations that are actually ours, so the shared cache line is not in the process-wide
		// hot path.
		alignas(64) std::atomic<int64_t> s_liveBytes;
		alignas(64) std::atomic<int64_t> s_peakBytes;
		alignas(64) std::atomic<int64_t> s_liveBlocks;
		alignas(64) std::atomic<uint64_t> s_totalBytes;
		alignas(64) std::atomic<uint64_t> s_totalAllocs;
		alignas(64) std::atomic<uint64_t> s_totalFrees;

		void *s_tableBase;
		std::atomic<uint64_t> s_serial;
		std::atomic<uint64_t> s_untrackedFrees;
		std::atomic<uint64_t> s_orphanedFrees;
		std::atomic<uint64_t> s_liveEntries;
		std::atomic<uint64_t> s_sampledWalks;
		std::atomic<bool> s_overflowed;

		uint64_t s_markSerial;
		bool s_hasMark;

		uintptr_t s_selfBase;
		size_t s_selfSize;

		// Scratch for report aggregation. The reporting entry points are console commands on the
		// main thread; a spin flag keeps two of them from interleaving.
		std::atomic<uint32_t> s_reportLock;
		int64_t s_scratchBytes[CALLSITE_COUNT];
		int64_t s_scratchBlocks[CALLSITE_COUNT];

		//---------------------------------------------------------------------------------------
		// Primitives
		//---------------------------------------------------------------------------------------

		inline void CpuRelax()
		{
#ifdef _WIN32
			_mm_pause();
#else
			__builtin_ia32_pause();
#endif
		}

		inline void SpinLock(std::atomic<uint32_t> &lock)
		{
			for (;;)
			{
				uint32_t expected = 0;
				if (lock.compare_exchange_weak(expected, 1u, std::memory_order_acquire, std::memory_order_relaxed))
				{
					return;
				}
				CpuRelax();
			}
		}

		inline void SpinUnlock(std::atomic<uint32_t> &lock)
		{
			lock.store(0u, std::memory_order_release);
		}

		// Pointers out of a general-purpose allocator are at least 16-byte aligned, so the low bits
		// carry no information - shift them out before mixing (splitmix64 finaliser).
		inline uint64_t HashPtr(const void *p)
		{
			uint64_t x = (uint64_t)(uintptr_t)p >> 4;
			x ^= x >> 33;
			x *= 0xff51afd7ed558ccdULL;
			x ^= x >> 33;
			x *= 0xc4ceb9fe1a85ec53ULL;
			x ^= x >> 33;
			return x;
		}

		inline uint32_t ShardOf(uint64_t hash)
		{
			return (uint32_t)(hash & SHARD_MASK);
		}

		inline uint32_t SlotOf(uint64_t hash)
		{
			return (uint32_t)((hash >> 6) & ENTRY_MASK);
		}

		//---------------------------------------------------------------------------------------
		// Counters
		//---------------------------------------------------------------------------------------

		void CountAlloc(uint64_t size)
		{
			int64_t live = s_liveBytes.fetch_add((int64_t)size, std::memory_order_relaxed) + (int64_t)size;
			s_liveBlocks.fetch_add(1, std::memory_order_relaxed);
			s_totalBytes.fetch_add(size, std::memory_order_relaxed);
			s_totalAllocs.fetch_add(1, std::memory_order_relaxed);

			// Relaxed read first so the compare-exchange loop is skipped on the overwhelming
			// majority of allocations.
			int64_t peak = s_peakBytes.load(std::memory_order_relaxed);
			while (live > peak)
			{
				if (s_peakBytes.compare_exchange_weak(peak, live, std::memory_order_relaxed, std::memory_order_relaxed))
				{
					break;
				}
			}
		}

		void CountFree(uint64_t size)
		{
			s_liveBytes.fetch_sub((int64_t)size, std::memory_order_relaxed);
			s_liveBlocks.fetch_sub(1, std::memory_order_relaxed);
			s_totalFrees.fetch_add(1, std::memory_order_relaxed);
		}

		void CallsiteAdd(uint32_t idx, uint64_t size)
		{
			if (idx == INVALID_CALLSITE)
			{
				return;
			}
			CallsiteEntry &c = s_callsites[idx];
			c.liveBytes.fetch_add((int64_t)size, std::memory_order_relaxed);
			c.liveBlocks.fetch_add(1, std::memory_order_relaxed);
			c.totalBytes.fetch_add(size, std::memory_order_relaxed);
			c.totalAllocs.fetch_add(1, std::memory_order_relaxed);
		}

		void CallsiteSub(uint32_t idx, uint64_t size)
		{
			if (idx == INVALID_CALLSITE)
			{
				return;
			}
			CallsiteEntry &c = s_callsites[idx];
			c.liveBytes.fetch_sub((int64_t)size, std::memory_order_relaxed);
			c.liveBlocks.fetch_sub(1, std::memory_order_relaxed);
		}

		uint32_t InternCallsite(const void *callsite)
		{
			uintptr_t key = (uintptr_t)callsite;
			if (!key)
			{
				return INVALID_CALLSITE;
			}

			uint32_t i = (uint32_t)(HashPtr(callsite) & CALLSITE_MASK);
			for (uint32_t probes = 0; probes <= CALLSITE_MASK; probes++)
			{
				uintptr_t cur = s_callsites[i].addr.load(std::memory_order_acquire);
				if (cur == key)
				{
					return i;
				}
				if (cur == 0)
				{
					uintptr_t expected = 0;
					if (s_callsites[i].addr.compare_exchange_strong(expected, key, std::memory_order_acq_rel, std::memory_order_acquire))
					{
						return i;
					}
					if (expected == key)
					{
						return i;
					}
				}
				i = (i + 1) & CALLSITE_MASK;
			}

			// Table saturated - 4096 distinct call sites is already far past useful.
			return INVALID_CALLSITE;
		}

		//---------------------------------------------------------------------------------------
		// Side table
		//---------------------------------------------------------------------------------------

		// Caller holds the shard lock. Linear probing with backward-shift deletion, so there are no
		// tombstones to accumulate over a long-running server.
		void RemoveAt(Shard &shard, uint32_t i)
		{
			shard.entries[i].ptr = nullptr;

			uint32_t j = i;
			for (;;)
			{
				j = (j + 1) & ENTRY_MASK;
				if (!shard.entries[j].ptr)
				{
					break;
				}

				uint32_t k = SlotOf(HashPtr(shard.entries[j].ptr));

				// Is k cyclically inside (i, j]? If not, entry j may shift back into slot i.
				bool inRange = (i <= j) ? (i < k && k <= j) : (i < k || k <= j);
				if (!inRange)
				{
					shard.entries[i] = shard.entries[j];
					shard.entries[j].ptr = nullptr;
					i = j;
				}
			}
		}

		bool TableInsert(void *ptr, uint64_t size, uint64_t serial, uint32_t callsiteIdx)
		{
			uint64_t hash = HashPtr(ptr);
			Shard &shard = s_shards[ShardOf(hash)];

			SpinLock(shard.lock);

			if (!shard.entries)
			{
				size_t offset = (size_t)(&shard - s_shards) * SHARD_BYTES;
				void *base = (char *)s_tableBase + offset;
				if (!s_tableBase || !Plat_CommitPages(base, SHARD_BYTES))
				{
					s_overflowed.store(true, std::memory_order_relaxed);
					SpinUnlock(shard.lock);
					return false;
				}
				// Freshly committed pages are zero-filled on both platforms, which is exactly the
				// "all slots empty" state.
				shard.entries = (Entry *)base;
			}

			if (shard.count >= MAX_LOAD_PER_SHARD)
			{
				s_overflowed.store(true, std::memory_order_relaxed);
				SpinUnlock(shard.lock);
				return false;
			}

			uint32_t i = SlotOf(hash);
			while (shard.entries[i].ptr)
			{
				if (shard.entries[i].ptr == ptr)
				{
					// The allocator handed back a pointer we still think is live, which means
					// somebody outside this module freed our block. Retire the stale accounting so
					// the table self-heals instead of drifting forever.
					const Entry &old = shard.entries[i];
					CountFree(old.size);
					CallsiteSub(old.callsiteIdx, old.size);
					s_orphanedFrees.fetch_add(1, std::memory_order_relaxed);
					s_liveEntries.fetch_sub(1, std::memory_order_relaxed);
					shard.count--;
					break;
				}
				i = (i + 1) & ENTRY_MASK;
			}

			shard.entries[i].ptr = ptr;
			shard.entries[i].size = size;
			shard.entries[i].serial = serial;
			shard.entries[i].callsiteIdx = callsiteIdx;
			shard.count++;

			SpinUnlock(shard.lock);

			s_liveEntries.fetch_add(1, std::memory_order_relaxed);
			return true;
		}

		bool TableRemove(void *ptr, Entry *out)
		{
			uint64_t hash = HashPtr(ptr);
			Shard &shard = s_shards[ShardOf(hash)];

			SpinLock(shard.lock);

			if (!shard.entries)
			{
				SpinUnlock(shard.lock);
				return false;
			}

			bool found = false;
			uint32_t i = SlotOf(hash);
			for (uint32_t probes = 0; probes <= ENTRY_MASK; probes++)
			{
				if (!shard.entries[i].ptr)
				{
					break;
				}
				if (shard.entries[i].ptr == ptr)
				{
					*out = shard.entries[i];
					RemoveAt(shard, i);
					shard.count--;
					found = true;
					break;
				}
				i = (i + 1) & ENTRY_MASK;
			}

			SpinUnlock(shard.lock);

			if (found)
			{
				s_liveEntries.fetch_sub(1, std::memory_order_relaxed);
			}
			return found;
		}
	} // namespace

	//-----------------------------------------------------------------------------------------------
	// Bookkeeping entry points
	//-----------------------------------------------------------------------------------------------

	void OnAlloc(void *ptr, size_t size, const void *callsite)
	{
		if (tl_inHook)
		{
			return;
		}
		tl_inHook = true;

		uint32_t callsiteIdx = InternCallsite(callsite);
		uint64_t serial = s_serial.fetch_add(1, std::memory_order_relaxed);

		// Counters are only touched when the block actually made it into the table, so a full table
		// cannot inflate live bytes without a matching free ever arriving.
		if (TableInsert(ptr, size, serial, callsiteIdx))
		{
			CountAlloc(size);
			CallsiteAdd(callsiteIdx, size);
		}

		tl_inHook = false;
	}

	void OnFree(void *ptr)
	{
		if (tl_inHook)
		{
			return;
		}
		tl_inHook = true;

		Entry entry;
		if (TableRemove(ptr, &entry))
		{
			CountFree(entry.size);
			CallsiteSub(entry.callsiteIdx, entry.size);
		}
		else
		{
			// Allocated before the hook went in, or by a module we do not track.
			s_untrackedFrees.fetch_add(1, std::memory_order_relaxed);
		}

		tl_inHook = false;
	}

	void NoteSampledWalk()
	{
		s_sampledWalks.fetch_add(1, std::memory_order_relaxed);
	}

	//-----------------------------------------------------------------------------------------------
	// Table lifecycle
	//-----------------------------------------------------------------------------------------------

	bool AllocateTable()
	{
		if (s_tableBase)
		{
			return true;
		}

		if (!Plat_GetSelfModuleRange(&s_selfBase, &s_selfSize))
		{
			return false;
		}

		s_tableBase = Plat_ReservePages(TABLE_BYTES);
		return s_tableBase != nullptr;
	}

	void ReleaseTable()
	{
		for (uint32_t i = 0; i < SHARD_COUNT; i++)
		{
			s_shards[i].entries = nullptr;
			s_shards[i].count = 0;
		}

		if (s_tableBase)
		{
			Plat_ReleasePages(s_tableBase, TABLE_BYTES);
			s_tableBase = nullptr;
		}

		// Drop every counter too. Leaving them populated with an empty table would report live
		// bytes that no longer have a single tracked block behind them, which matters because
		// metamod can unload and reload the plugin inside one server session.
		s_liveBytes.store(0, std::memory_order_relaxed);
		s_peakBytes.store(0, std::memory_order_relaxed);
		s_liveBlocks.store(0, std::memory_order_relaxed);
		s_totalBytes.store(0, std::memory_order_relaxed);
		s_totalAllocs.store(0, std::memory_order_relaxed);
		s_totalFrees.store(0, std::memory_order_relaxed);

		for (uint32_t i = 0; i < CALLSITE_COUNT; i++)
		{
			s_callsites[i].addr.store(0, std::memory_order_relaxed);
			s_callsites[i].liveBytes.store(0, std::memory_order_relaxed);
			s_callsites[i].liveBlocks.store(0, std::memory_order_relaxed);
			s_callsites[i].totalBytes.store(0, std::memory_order_relaxed);
			s_callsites[i].totalAllocs.store(0, std::memory_order_relaxed);
		}

		s_liveEntries.store(0, std::memory_order_relaxed);
		s_untrackedFrees.store(0, std::memory_order_relaxed);
		s_orphanedFrees.store(0, std::memory_order_relaxed);
		s_sampledWalks.store(0, std::memory_order_relaxed);
		s_overflowed.store(false, std::memory_order_relaxed);
		s_hasMark = false;
	}

	bool IsSelfAddress(const void *address)
	{
		return ((uintptr_t)address - s_selfBase) < s_selfSize;
	}

	uintptr_t GetSelfBase()
	{
		return s_selfBase;
	}

	//-----------------------------------------------------------------------------------------------
	// Reporting
	//-----------------------------------------------------------------------------------------------

	Totals GetTotals()
	{
		Totals t {};
		t.liveBytes = s_liveBytes.load(std::memory_order_relaxed);
		t.peakBytes = s_peakBytes.load(std::memory_order_relaxed);
		t.liveBlocks = s_liveBlocks.load(std::memory_order_relaxed);
		t.totalBytes = s_totalBytes.load(std::memory_order_relaxed);
		t.totalAllocs = s_totalAllocs.load(std::memory_order_relaxed);
		t.totalFrees = s_totalFrees.load(std::memory_order_relaxed);
		t.untrackedFrees = s_untrackedFrees.load(std::memory_order_relaxed);
		t.orphanedFrees = s_orphanedFrees.load(std::memory_order_relaxed);
		t.tableEntries = s_liveEntries.load(std::memory_order_relaxed);
		t.tableCapacity = (uint64_t)MAX_LOAD_PER_SHARD * SHARD_COUNT;
		t.sampledWalks = s_sampledWalks.load(std::memory_order_relaxed);
		t.overflowed = s_overflowed.load(std::memory_order_relaxed);
		return t;
	}

	namespace
	{
		// Insertion into a descending-by-liveBytes window. maxCount is small (a console top-N), so
		// this stays allocation-free and simple.
		void PushSorted(CallsiteStats *out, uint32_t &used, uint32_t maxCount, const CallsiteStats &candidate)
		{
			if (used < maxCount)
			{
				uint32_t i = used++;
				while (i > 0 && out[i - 1].liveBytes < candidate.liveBytes)
				{
					out[i] = out[i - 1];
					i--;
				}
				out[i] = candidate;
				return;
			}

			if (candidate.liveBytes <= out[used - 1].liveBytes)
			{
				return;
			}

			uint32_t i = used - 1;
			while (i > 0 && out[i - 1].liveBytes < candidate.liveBytes)
			{
				out[i] = out[i - 1];
				i--;
			}
			out[i] = candidate;
		}
	} // namespace

	uint32_t GetTopCallsites(CallsiteStats *out, uint32_t maxCount)
	{
		if (!out || maxCount == 0)
		{
			return 0;
		}

		uint32_t used = 0;
		for (uint32_t i = 0; i < CALLSITE_COUNT; i++)
		{
			uintptr_t addr = s_callsites[i].addr.load(std::memory_order_relaxed);
			if (!addr)
			{
				continue;
			}

			int64_t liveBytes = s_callsites[i].liveBytes.load(std::memory_order_relaxed);
			if (liveBytes <= 0)
			{
				continue;
			}

			CallsiteStats c {};
			c.address = (const void *)addr;
			c.liveBytes = liveBytes;
			c.liveBlocks = s_callsites[i].liveBlocks.load(std::memory_order_relaxed);
			c.totalBytes = s_callsites[i].totalBytes.load(std::memory_order_relaxed);
			c.totalAllocs = s_callsites[i].totalAllocs.load(std::memory_order_relaxed);
			PushSorted(out, used, maxCount, c);
		}

		return used;
	}

	uint32_t GetLeaksSinceMark(CallsiteStats *out, uint32_t maxCount)
	{
		if (!out || maxCount == 0 || !s_hasMark)
		{
			return 0;
		}

		SpinLock(s_reportLock);

		memset(s_scratchBytes, 0, sizeof(s_scratchBytes));
		memset(s_scratchBlocks, 0, sizeof(s_scratchBlocks));

		uint64_t mark = s_markSerial;

		// Walk every shard, taking each lock only for the length of its own scan.
		for (uint32_t shardIdx = 0; shardIdx < SHARD_COUNT; shardIdx++)
		{
			Shard &shard = s_shards[shardIdx];
			SpinLock(shard.lock);

			if (shard.entries)
			{
				for (uint32_t i = 0; i < ENTRIES_PER_SHARD; i++)
				{
					const Entry &e = shard.entries[i];
					if (!e.ptr || e.serial < mark || e.callsiteIdx == INVALID_CALLSITE)
					{
						continue;
					}
					s_scratchBytes[e.callsiteIdx] += (int64_t)e.size;
					s_scratchBlocks[e.callsiteIdx]++;
				}
			}

			SpinUnlock(shard.lock);
		}

		uint32_t used = 0;
		for (uint32_t i = 0; i < CALLSITE_COUNT; i++)
		{
			if (s_scratchBytes[i] <= 0)
			{
				continue;
			}

			CallsiteStats c {};
			c.address = (const void *)s_callsites[i].addr.load(std::memory_order_relaxed);
			c.liveBytes = s_scratchBytes[i];
			c.liveBlocks = s_scratchBlocks[i];
			c.totalBytes = s_callsites[i].totalBytes.load(std::memory_order_relaxed);
			// Left at zero on purpose: Live and Blocks above are since-mark, and an
			// allocs-since-mark count cannot be reconstructed - blocks allocated and freed since
			// the mark are no longer in the table. Reporting the lifetime counter here would sit a
			// number next to them that means something entirely different.
			c.totalAllocs = 0;
			PushSorted(out, used, maxCount, c);
		}

		SpinUnlock(s_reportLock);
		return used;
	}

	void SetMark()
	{
		s_markSerial = s_serial.load(std::memory_order_relaxed);
		s_hasMark = true;
	}

	bool HasMark()
	{
		return s_hasMark;
	}

	void ResetCounters()
	{
		// Peak collapses back to whatever is live right now; live bytes and blocks are left alone
		// because the underlying allocations are still out there.
		s_peakBytes.store(s_liveBytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
		s_totalBytes.store(0, std::memory_order_relaxed);
		s_totalAllocs.store(0, std::memory_order_relaxed);
		s_totalFrees.store(0, std::memory_order_relaxed);

		for (uint32_t i = 0; i < CALLSITE_COUNT; i++)
		{
			s_callsites[i].totalBytes.store(0, std::memory_order_relaxed);
			s_callsites[i].totalAllocs.store(0, std::memory_order_relaxed);
		}

		s_untrackedFrees.store(0, std::memory_order_relaxed);
		s_orphanedFrees.store(0, std::memory_order_relaxed);
		s_sampledWalks.store(0, std::memory_order_relaxed);
	}

	void FormatAddress(const void *address, char *buffer, size_t bufferSize)
	{
		if (!buffer || bufferSize == 0)
		{
			return;
		}

		uintptr_t address_uint = (uintptr_t)address;

		if (s_selfBase && IsSelfAddress(address))
		{
			V_snprintf(buffer, (int)bufferSize, "cs2kz+0x%llX", (unsigned long long)((uintptr_t)address - s_selfBase));
		}
		else
		{
			// Not our code: tier0, libc or the engine allocating on our behalf while cs2kz was on
			// the stack. Name the owning module so the offset can be resolved against the right
			// binary instead of being an opaque pointer.
			char moduleName[64] = {};
			uintptr_t moduleBase = 0;
			if (Plat_GetModuleForAddress(address, moduleName, sizeof(moduleName), &moduleBase))
			{
				V_snprintf(buffer, (int)bufferSize, "%s+0x%llX", moduleName, (unsigned long long)(address_uint - moduleBase));
			}
			else
			{
				V_snprintf(buffer, (int)bufferSize, "0x%llX", (unsigned long long)(uintptr_t)address);
			}
		}
	}
} // namespace KZMem
