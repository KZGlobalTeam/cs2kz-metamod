#include "common.h"
#include "utils/memtracker.h"

// Public shim mirroring IMemAlloc vtable layout; used for pointer-to-member decoding and compatibility helpers.
struct IMemAllocShim
{
	virtual ~IMemAllocShim() {}

	virtual void *Alloc(size_t nSize) = 0;
	virtual void *Realloc(void *pMem, size_t nSize) = 0;
	virtual void Free(void *pMem) = 0;
};

#include "tier0/memalloc.h"

#if defined(POSIX)
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <cxxabi.h>
#include <stdio.h>
#include <string.h>
#include "funchook.h"

// Detour the exported allocator entry points (or fall back to the vtable) with funchook.

namespace
{
	constexpr int kTrackerFrames = 8;
	constexpr int kTrackerSkipFrames = 4;
	constexpr int kMaxTrackedStacks = 4096;
	constexpr int kMaxTrackedPointers = 131072;

	static bool FrameInModule(void *addr);
	static void EnsureModuleRangesInitialized();

	struct StackKey
	{
		void *frames[kTrackerFrames] = {};
	};

	struct ModuleRange
	{
		uintptr_t start = 0;
		uintptr_t end = 0;
	};

	struct StackStat
	{
		StackKey key {};
		size_t currentBytes = 0;
		size_t totalBytes = 0;
		uint64_t allocationCount = 0;
		bool inUse = false;
	};

	struct StackCheckpoint
	{
		size_t currentBytes = 0;
		uint64_t allocationCount = 0;
	};

	struct PointerEntry
	{
		void *ptr = nullptr;
		size_t size = 0;
		int stackIndex = -1;
		bool inUse = false;
		bool everUsed = false;
	};

	class SimpleLock
	{
	public:
		void Lock()
		{
			pthread_mutex_lock(&m_mutex);
		}

		void Unlock()
		{
			pthread_mutex_unlock(&m_mutex);
		}

	private:
		pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
	};

	class LockGuard
	{
	public:
		explicit LockGuard(SimpleLock &lock) : m_lock(lock)
		{
			m_lock.Lock();
		}

		~LockGuard()
		{
			m_lock.Unlock();
		}

		LockGuard(const LockGuard &) = delete;
		LockGuard &operator=(const LockGuard &) = delete;

	private:
		SimpleLock &m_lock;
	};

	class TrackerScope
	{
	public:
		TrackerScope() : m_skip(g_inTracker)
		{
			if (!m_skip)
			{
				g_inTracker = true;
			}
		}

		TrackerScope(const TrackerScope &) = delete;
		TrackerScope &operator=(const TrackerScope &) = delete;

		~TrackerScope()
		{
			if (!m_skip)
			{
				g_inTracker = false;
			}
		}

		bool Skip() const
		{
			return m_skip;
		}

	private:
		bool m_skip;
		static thread_local bool g_inTracker;
	};

	thread_local bool TrackerScope::g_inTracker = false;

	class AllocationTracker
	{
	public:
		void RecordAlloc(void *ptr, size_t size)
		{
			if (!ptr || size == 0)
			{
				return;
			}

			TrackerScope scope;
			if (scope.Skip())
			{
				return;
			}

			StackKey key {};
			if (!CaptureStack(key))
			{
				return;
			}

			LockGuard lock(m_lock);
			int stackIndex = FindOrInsertStack(key);
			if (stackIndex < 0)
			{
				return;
			}

			StackStat &stat = m_stats[stackIndex];
			stat.currentBytes += size;
			stat.totalBytes += size;
			++stat.allocationCount;
			m_totalCurrentBytes += size;

			InsertPointer(ptr, size, stackIndex);
		}

		void RecordFree(void *ptr)
		{
			if (!ptr)
			{
				return;
			}

			TrackerScope scope;
			if (scope.Skip())
			{
				return;
			}

			LockGuard lock(m_lock);
			int pointerIndex = FindPointer(ptr);
			if (pointerIndex < 0)
			{
				return;
			}

			PointerEntry &entry = m_pointers[pointerIndex];
			if (!entry.inUse)
			{
				return;
			}

			if (entry.stackIndex >= 0 && entry.stackIndex < kMaxTrackedStacks && m_stats[entry.stackIndex].inUse)
			{
				StackStat &stat = m_stats[entry.stackIndex];
				stat.currentBytes = (stat.currentBytes > entry.size) ? stat.currentBytes - entry.size : 0;
			}

			if (m_totalCurrentBytes > entry.size)
			{
				m_totalCurrentBytes -= entry.size;
			}
			else
			{
				m_totalCurrentBytes = 0;
			}
			++m_totalFrees;

			entry.inUse = false;
			entry.ptr = nullptr;
			entry.size = 0;
			entry.stackIndex = -1;
			m_pointerSaturated = false;
		}

		void RecordRealloc(void *oldPtr, void *newPtr, size_t newSize)
		{
			TrackerScope scope;
			if (scope.Skip())
			{
				return;
			}

			LockGuard lock(m_lock);

			if (oldPtr)
			{
				int pointerIndex = FindPointer(oldPtr);
				if (pointerIndex >= 0 && m_pointers[pointerIndex].inUse)
				{
					PointerEntry &entry = m_pointers[pointerIndex];
					if (entry.stackIndex >= 0 && entry.stackIndex < kMaxTrackedStacks && m_stats[entry.stackIndex].inUse)
					{
						StackStat &stat = m_stats[entry.stackIndex];
						stat.currentBytes = (stat.currentBytes > entry.size) ? stat.currentBytes - entry.size : 0;
					}
					if (m_totalCurrentBytes > entry.size)
					{
						m_totalCurrentBytes -= entry.size;
					}
					else
					{
						m_totalCurrentBytes = 0;
					}
					entry.inUse = false;
					entry.ptr = nullptr;
					entry.size = 0;
					entry.stackIndex = -1;
					m_pointerSaturated = false;
				}
			}

			if (!newPtr || newSize == 0)
			{
				return;
			}

			StackKey key {};
			if (!CaptureStack(key))
			{
				return;
			}
			int stackIndex = FindOrInsertStack(key);
			if (stackIndex < 0)
			{
				return;
			}

			StackStat &stat = m_stats[stackIndex];
			stat.currentBytes += newSize;
			stat.totalBytes += newSize;
			++stat.allocationCount;
			m_totalCurrentBytes += newSize;

			InsertPointer(newPtr, newSize, stackIndex);
		}

		void PrintTop(int topCount)
		{
			TrackerScope scope;
			if (scope.Skip())
			{
				return;
			}

			LockGuard lock(m_lock);
			META_CONPRINTF("[alloc-tracker] total_current=%zu frees=%llu\n", m_totalCurrentBytes, static_cast<unsigned long long>(m_totalFrees));
			bool visited[kMaxTrackedStacks] = {};

			for (int rank = 0; rank < topCount; ++rank)
			{
				int bestIndex = -1;
				size_t bestBytes = 0;
				for (int i = 0; i < kMaxTrackedStacks; ++i)
				{
					if (!m_stats[i].inUse || visited[i])
					{
						continue;
					}

					if (m_stats[i].currentBytes > bestBytes)
					{
						bestBytes = m_stats[i].currentBytes;
						bestIndex = i;
					}
				}

				if (bestIndex < 0 || bestBytes == 0)
				{
					break;
				}

				visited[bestIndex] = true;
				const StackStat &stat = m_stats[bestIndex];
				META_CONPRINTF("[alloc-tracker] #%d bytes=%zu allocs=%llu\n", rank + 1, stat.currentBytes,
							   static_cast<unsigned long long>(stat.allocationCount));
				PrintStack(stat.key);
			}
		}

		void SetCheckpoint()
		{
			TrackerScope scope;
			if (scope.Skip())
			{
				return;
			}

			LockGuard lock(m_lock);
			for (int i = 0; i < kMaxTrackedStacks; ++i)
			{
				m_checkpoint[i].currentBytes = m_stats[i].inUse ? m_stats[i].currentBytes : 0;
				m_checkpoint[i].allocationCount = m_stats[i].inUse ? m_stats[i].allocationCount : 0;
			}
			m_checkpointTotalCurrent = m_totalCurrentBytes;
			m_checkpointTotalFrees = m_totalFrees;
			m_hasCheckpoint = true;
		}

		void PrintTopDelta(int topCount)
		{
			TrackerScope scope;
			if (scope.Skip())
			{
				return;
			}
			if (!m_hasCheckpoint)
			{
				META_CONPRINTF("[alloc-tracker] no checkpoint set\n");
				return;
			}

			LockGuard lock(m_lock);
			long long deltaTotal = static_cast<long long>(m_totalCurrentBytes) - static_cast<long long>(m_checkpointTotalCurrent);
			long long deltaFrees = static_cast<long long>(m_totalFrees) - static_cast<long long>(m_checkpointTotalFrees);
			META_CONPRINTF("[alloc-tracker][delta] total_current=%lld frees=%lld\n", static_cast<long long>(deltaTotal),
						   static_cast<long long>(deltaFrees));

			bool visited[kMaxTrackedStacks] = {};
			for (int rank = 0; rank < topCount; ++rank)
			{
				int bestIndex = -1;
				size_t bestDelta = 0;
				for (int i = 0; i < kMaxTrackedStacks; ++i)
				{
					if (!m_stats[i].inUse || visited[i])
					{
						continue;
					}
					size_t base = m_checkpoint[i].currentBytes;
					size_t delta = (m_stats[i].currentBytes > base) ? (m_stats[i].currentBytes - base) : 0;
					if (delta > bestDelta)
					{
						bestDelta = delta;
						bestIndex = i;
					}
				}

				if (bestIndex < 0 || bestDelta == 0)
				{
					break;
				}

				visited[bestIndex] = true;
				const StackStat &stat = m_stats[bestIndex];
				const StackCheckpoint &chk = m_checkpoint[bestIndex];
				uint64_t deltaAllocs = (stat.allocationCount > chk.allocationCount) ? (stat.allocationCount - chk.allocationCount) : 0;
				META_CONPRINTF("[alloc-tracker][delta] #%d bytes=%zu allocs=%llu\n", rank + 1, bestDelta,
							   static_cast<unsigned long long>(deltaAllocs));
				PrintStack(stat.key);
			}
		}

	private:
		static uint32_t HashKey(const StackKey &key)
		{
			uint32_t hash = 2166136261u;
			for (int i = 0; i < kTrackerFrames; ++i)
			{
				uintptr_t value = reinterpret_cast<uintptr_t>(key.frames[i]);
				hash ^= static_cast<uint32_t>(value);
				hash *= 16777619u;
			}
			return hash;
		}

		static bool KeysEqual(const StackKey &a, const StackKey &b)
		{
			for (int i = 0; i < kTrackerFrames; ++i)
			{
				if (a.frames[i] != b.frames[i])
				{
					return false;
				}
			}
			return true;
		}

		bool CaptureStack(StackKey &outKey)
		{
			for (int i = 0; i < kTrackerFrames; ++i)
			{
				outKey.frames[i] = nullptr;
			}

			void *frames[kTrackerFrames + kTrackerSkipFrames] = {};
			int captured = backtrace(frames, static_cast<int>(kTrackerFrames + kTrackerSkipFrames));
			int outIndex = 0;
			bool anyModule = false;
			EnsureModuleRangesInitialized();
			for (int i = kTrackerSkipFrames; i < captured && outIndex < kTrackerFrames; ++i)
			{
				if (!FrameInModule(frames[i]))
				{
					continue;
				}
				anyModule = true;
				outKey.frames[outIndex++] = frames[i];
			}
			return anyModule;
		}

		int FindOrInsertStack(const StackKey &key)
		{
			int firstFree = -1;
			uint32_t hash = HashKey(key);
			int start = static_cast<int>(hash % kMaxTrackedStacks);
			for (int i = 0; i < kMaxTrackedStacks; ++i)
			{
				int idx = (start + i) % kMaxTrackedStacks;
				if (m_stats[idx].inUse)
				{
					if (KeysEqual(m_stats[idx].key, key))
					{
						return idx;
					}
				}
				else if (firstFree < 0)
				{
					firstFree = idx;
				}
			}

			if (firstFree < 0)
			{
				return -1;
			}

			m_stats[firstFree].key = key;
			m_stats[firstFree].inUse = true;
			m_stats[firstFree].currentBytes = 0;
			m_stats[firstFree].totalBytes = 0;
			m_stats[firstFree].allocationCount = 0;
			return firstFree;
		}

		int FindPointer(void *ptr) const
		{
			uintptr_t raw = reinterpret_cast<uintptr_t>(ptr);
			uint32_t hash = static_cast<uint32_t>((raw >> 4) ^ raw);
			int start = static_cast<int>(hash % kMaxTrackedPointers);
			for (int i = 0; i < kMaxTrackedPointers; ++i)
			{
				int idx = (start + i) % kMaxTrackedPointers;
				const PointerEntry &entry = m_pointers[idx];
				if (!entry.everUsed)
				{
					return -1;
				}
				if (entry.inUse && entry.ptr == ptr)
				{
					return idx;
				}
			}
			return -1;
		}

		void InsertPointer(void *ptr, size_t size, int stackIndex)
		{
			if (m_pointerSaturated)
			{
				return;
			}

			uintptr_t raw = reinterpret_cast<uintptr_t>(ptr);
			uint32_t hash = static_cast<uint32_t>((raw >> 4) ^ raw);
			int start = static_cast<int>(hash % kMaxTrackedPointers);
			int freeSlot = -1;
			for (int i = 0; i < kMaxTrackedPointers; ++i)
			{
				int idx = (start + i) % kMaxTrackedPointers;
				PointerEntry &entry = m_pointers[idx];
				if (!entry.inUse)
				{
					if (freeSlot < 0)
					{
						freeSlot = idx;
					}
					if (!entry.everUsed)
					{
						break;
					}
				}
			}

			if (freeSlot >= 0)
			{
				PointerEntry &entry = m_pointers[freeSlot];
				entry.ptr = ptr;
				entry.size = size;
				entry.stackIndex = stackIndex;
				entry.inUse = true;
				entry.everUsed = true;
				m_pointerSaturated = false;
				return;
			}

			m_pointerSaturated = true;
		}

		static void PrintStack(const StackKey &key)
		{
			for (int i = 0; i < kTrackerFrames; ++i)
			{
				if (!key.frames[i])
				{
					break;
				}
				void *addr = key.frames[i];
				Dl_info info {};
				const char *mod = "?";
				const char *sym = nullptr;
				char demangled[512];
				demangled[0] = '\0';
				uintptr_t modOff = 0;
				uintptr_t symOff = 0;

				if (dladdr(addr, &info))
				{
					if (info.dli_fname)
					{
						mod = info.dli_fname;
						modOff = reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(info.dli_fbase);
					}
					if (info.dli_sname)
					{
						sym = info.dli_sname;
						symOff = reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(info.dli_saddr);
						int status = 0;
						char *out = abi::__cxa_demangle(sym, demangled, nullptr, &status);
						if (status == 0 && out)
						{
							sym = out;
						}
					}
				}

				META_CONPRINTF("    frame[%d]: %p %s+0x%lx", i, addr, mod, static_cast<unsigned long>(modOff));
				if (sym)
				{
					META_CONPRINTF(" (%s+0x%lx)", sym, static_cast<unsigned long>(symOff));
				}
				META_CONPRINTF("\n");
			}
		}

		SimpleLock m_lock;
		StackStat m_stats[kMaxTrackedStacks] = {};
		StackCheckpoint m_checkpoint[kMaxTrackedStacks] = {};
		PointerEntry m_pointers[kMaxTrackedPointers] = {};
		bool m_pointerSaturated = false;
		size_t m_totalCurrentBytes = 0;
		uint64_t m_totalFrees = 0;
		bool m_hasCheckpoint = false;
		size_t m_checkpointTotalCurrent = 0;
		uint64_t m_checkpointTotalFrees = 0;
	};

	AllocationTracker g_tracker;
	ModuleRange g_moduleRanges[8];
	int g_moduleRangeCount = 0;

	struct PmfLayout
	{
		ptrdiff_t ptr;
		ptrdiff_t adj;
	};

	template<typename MemFn>
	int DecodeVtableIndex(MemFn memfn)
	{
		PmfLayout pmf = *reinterpret_cast<PmfLayout *>(&memfn);
		if ((pmf.ptr & 1) == 0)
		{
			return -1; // non-virtual (should not happen here)
		}
		ptrdiff_t offset = pmf.ptr - 1;
		return static_cast<int>(offset / static_cast<ptrdiff_t>(sizeof(void *)));
	}

	const int kAllocIndex = DecodeVtableIndex(&IMemAllocShim::Alloc);
	const int kReallocIndex = DecodeVtableIndex(&IMemAllocShim::Realloc);
	const int kFreeIndex = DecodeVtableIndex(&IMemAllocShim::Free);

	bool g_indicesValid = (kAllocIndex >= 0 && kReallocIndex >= 0 && kFreeIndex >= 0 && kAllocIndex < 128 && kReallocIndex < 128 && kFreeIndex < 128);

	constexpr const char *kAllocSymbol = "_ZN9CMemAlloc5AllocEm";
	constexpr const char *kReallocSymbol = "_ZN9CMemAlloc7ReallocEPvm";
	constexpr const char *kFreeSymbol = "_ZN9CMemAlloc4FreeEPv";

	bool g_hooksAttached = false;

	using AllocFn = void *(*)(void *, size_t);
	using ReallocFn = void *(*)(void *, void *, size_t);
	using FreeFn = void (*)(void *, void *);

	AllocFn g_origAlloc = nullptr;
	ReallocFn g_origRealloc = nullptr;
	FreeFn g_origFree = nullptr;
	funchook_t *g_funchook = nullptr;

	static bool FrameInModule(void *addr)
	{
		uintptr_t p = reinterpret_cast<uintptr_t>(addr);
		for (int i = 0; i < g_moduleRangeCount; ++i)
		{
			if (p >= g_moduleRanges[i].start && p < g_moduleRanges[i].end)
			{
				return true;
			}
		}
		return false;
	}

	static void EnsureModuleRangesInitialized()
	{
		if (g_moduleRangeCount > 0)
		{
			return;
		}

		Dl_info info {};
		if (!dladdr(reinterpret_cast<void *>(&EnsureModuleRangesInitialized), &info) || !info.dli_fname)
		{
			return;
		}

		FILE *fp = fopen("/proc/self/maps", "r");
		if (!fp)
		{
			return;
		}

		char line[512];
		while (fgets(line, sizeof(line), fp))
		{
			unsigned long start = 0, end = 0, offset = 0, inode = 0;
			char perms[5] = {};
			char dev[6] = {};
			char path[256] = {};
			int parsed = sscanf(line, "%lx-%lx %4s %lx %5s %lu %255s", &start, &end, perms, &offset, dev, &inode, path);
			if (parsed < 6)
			{
				continue;
			}
			int capacity = static_cast<int>(sizeof(g_moduleRanges) / sizeof(g_moduleRanges[0]));
			if (parsed == 7 && strcmp(path, info.dli_fname) == 0 && g_moduleRangeCount < capacity)
			{
				g_moduleRanges[g_moduleRangeCount].start = start;
				g_moduleRanges[g_moduleRangeCount].end = end;
				++g_moduleRangeCount;
			}
		}

		fclose(fp);
	}

	void *ResolveSymbolOrVtable(const char *symbol, int vtableIndex)
	{
		void *addr = dlsym(RTLD_DEFAULT, symbol);
		if (addr)
		{
			return addr;
		}

		if (!g_pMemAlloc || !g_indicesValid || vtableIndex < 0)
		{
			return nullptr;
		}

		void **vtbl = *reinterpret_cast<void ***>(g_pMemAlloc);
		return vtbl[vtableIndex];
	}

	static void *Hook_Alloc(IMemAlloc *self, size_t nSize)
	{
		static thread_local bool inHook = false;
		if (inHook || !g_origAlloc)
		{
			return g_origAlloc ? g_origAlloc(self, nSize) : nullptr;
		}
		inHook = true;
		void *ret = g_origAlloc(self, nSize);
		if (ret)
		{
			g_tracker.RecordAlloc(ret, nSize);
		}
		inHook = false;
		return ret;
	}

	static void *Hook_Realloc(IMemAlloc *self, void *pMem, size_t nSize)
	{
		static thread_local bool inHook = false;
		if (inHook || !g_origRealloc)
		{
			return g_origRealloc ? g_origRealloc(self, pMem, nSize) : nullptr;
		}
		inHook = true;
		void *ret = g_origRealloc(self, pMem, nSize);
		g_tracker.RecordRealloc(pMem, ret, nSize);
		inHook = false;
		return ret;
	}

	static void Hook_Free(IMemAlloc *self, void *pMem)
	{
		static thread_local bool inHook = false;
		if (!g_origFree)
		{
			return;
		}
		if (!inHook)
		{
			inHook = true;
			g_tracker.RecordFree(pMem);
			inHook = false;
		}
		g_origFree(self, pMem);
	}
} // namespace

namespace memtracker
{
	void Init()
	{
		if (g_hooksAttached)
		{
			return;
		}

		void *allocTarget = ResolveSymbolOrVtable(kAllocSymbol, kAllocIndex);
		void *reallocTarget = ResolveSymbolOrVtable(kReallocSymbol, kReallocIndex);
		void *freeTarget = ResolveSymbolOrVtable(kFreeSymbol, kFreeIndex);
		if (!allocTarget || !reallocTarget || !freeTarget)
		{
			return;
		}

		g_origAlloc = reinterpret_cast<AllocFn>(allocTarget);
		g_origRealloc = reinterpret_cast<ReallocFn>(reallocTarget);
		g_origFree = reinterpret_cast<FreeFn>(freeTarget);

		g_funchook = funchook_create();
		if (!g_funchook)
		{
			g_origAlloc = nullptr;
			g_origRealloc = nullptr;
			g_origFree = nullptr;
			return;
		}

		if (funchook_prepare(g_funchook, reinterpret_cast<void **>(&g_origAlloc), reinterpret_cast<void *>(&Hook_Alloc)) != 0
			|| funchook_prepare(g_funchook, reinterpret_cast<void **>(&g_origRealloc), reinterpret_cast<void *>(&Hook_Realloc)) != 0
			|| funchook_prepare(g_funchook, reinterpret_cast<void **>(&g_origFree), reinterpret_cast<void *>(&Hook_Free)) != 0
			|| funchook_install(g_funchook, 0) != 0)
		{
			funchook_destroy(g_funchook);
			g_funchook = nullptr;
			g_origAlloc = nullptr;
			g_origRealloc = nullptr;
			g_origFree = nullptr;
			return;
		}

		g_hooksAttached = true;
	}

	void Shutdown()
	{
		if (!g_hooksAttached)
		{
			return;
		}

		if (g_funchook)
		{
			funchook_uninstall(g_funchook, 0);
			funchook_destroy(g_funchook);
			g_funchook = nullptr;
		}

		g_origAlloc = nullptr;
		g_origRealloc = nullptr;
		g_origFree = nullptr;
		g_hooksAttached = false;
	}
} // namespace memtracker

void PrintTopAllocatingStacks(int topCount)
{
	g_tracker.PrintTop(topCount);
}

void PrintTopAllocatingStacksDelta(int topCount)
{
	g_tracker.PrintTopDelta(topCount);
}

void SetAllocCheckpoint()
{
	g_tracker.SetCheckpoint();
}

#else

namespace memtracker
{
	void Init() {}

	void Shutdown() {}
} // namespace memtracker

void PrintTopAllocatingStacks(int) {}

void PrintTopAllocatingStacksDelta(int topCount) {}

void SetAllocCheckpoint() {}

#endif
