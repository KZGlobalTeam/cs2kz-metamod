#include "kz_memtrack.h"
#include "utils/plat.h"
#include "utils/addresses.h"
#include "utils/module.h"

#include "tier0/memalloc.h"
#include "tier0/platform.h"
#include "strtools.h"

#include "sourcehook.h"

#include <atomic>
#include <cstring>
#include <cstdlib>

#ifndef _WIN32
#include <sched.h>
#include <unwind.h>
#endif

// NOTE: no "tier0/memdbgon.h" in this file. Its macros rewrite the allocation calls we are trying
// to intercept, and this translation unit must reach the allocator by exactly one route.

//===================================================================================================
// Where CS2KZ allocations actually go
//
//   cs2kz code
//     |
//     +- new / delete ---------> operator new (ours, kz_memoverride.cpp) --+
//     +- std::vector / string -> MemAlloc_Alloc (inline)  ----------------+
//     +- CUtlVector::Grow -----> UtlVectorMemory_Alloc (tier0 export) ----+
//     +- CUtlString -----------> malloc (memoverride, Windows only) ------+
//                                                                         |
//                                                                         v
//                                        g_pMemAlloc->Alloc / Realloc / Free   <- intercepted here
//                                                         ^
//     engine, sql_mm, other plugins --------------------- +   (filtered out)
//
// Everything funnels through IMemAlloc, so one interception point sees all of it - including the
// allocations tier0 performs on our behalf, which is why no separate CUtlVector detour is needed.
//
// The hard part is not seeing the allocations, it is deciding which are OURS, cheaply. These
// thunks run for every allocation in the entire process; a stack walk to look for cs2kz costs
// ~400ns against ~5ns for the checks below, which at server allocation rates is the difference
// between a rounding error and a fifth of a core. So ownership is answered in O(1) by a
// thread-local depth counter that the module's entry points maintain (KZ_MEM_MODULE_SCOPE), and
// stack walking is reserved for recovering call sites, sampled.
//===================================================================================================

namespace KZMem
{
	namespace
	{
		//---------------------------------------------------------------------------------------
		// Calling convention note: on x86-64 there is only one native calling convention per
		// platform, so a plain free function with an explicit leading "this" parameter is
		// ABI-compatible with a virtual member on both MSVC and SysV.
		//---------------------------------------------------------------------------------------

		using AllocFn = void *(*)(void *self, size_t size);
		using ReallocFn = void *(*)(void *self, void *mem, size_t size);
		using FreeFn = void (*)(void *self, void *mem);
		using AllocAlignedFn = void *(*)(void *self, size_t size, size_t align);
		using ReallocAlignedFn = void *(*)(void *self, void *mem, size_t size, size_t align);
		using FreeAlignedFn = void (*)(void *self, void *mem);

		AllocFn s_origAlloc;
		ReallocFn s_origRealloc;
		FreeFn s_origFree;
		AllocAlignedFn s_origAllocAligned;
		ReallocAlignedFn s_origReallocAligned;
		FreeAlignedFn s_origFreeAligned;

		void **s_vtable;
		uint32_t s_slotAlloc;
		std::atomic<bool> s_active;
		const char *s_status = "not installed";

		// Diagnostics for the one step that is genuinely compiler/ABI dependent.
		SourceHook::MemFuncInfo s_funcInfo;
		char s_statusBuffer[192];

		//---------------------------------------------------------------------------------------
		// In-flight tracking.
		//
		// A global allocator hook that outlives its own module is a crash: a thread can be
		// executing one of our thunks at the moment cs2kz unmaps. So every thunk brackets itself
		// with a depth counter, and Uninstall() waits for all of them to hit zero.
		//
		// The counter is deliberately PER THREAD rather than a single shared atomic. This runs on
		// every allocation in the entire server process, and a shared counter would ping-pong one
		// cache line between every allocating core.
		//---------------------------------------------------------------------------------------

		constexpr uint32_t MAX_TRACKED_THREADS = 256;

		struct alignas(64) ThreadDepth
		{
			std::atomic<int32_t> depth;
		};

		ThreadDepth s_threadDepths[MAX_TRACKED_THREADS];
		std::atomic<uint32_t> s_threadSlotCount;

		// Spare slot used when more than MAX_TRACKED_THREADS threads show up.
		ThreadDepth s_overflowDepth;

		// Worth being explicit about, because getting it wrong would recurse forever: the first
		// touch of a thread_local in a dynamically loaded module allocates that thread's TLS block.
		// On Windows the loader does that with RtlAllocateHeap against the process heap, and on
		// Linux __tls_get_addr uses glibc malloc - neither routes through the operator new we
		// replaced, so this cannot re-enter the hook.
		thread_local ThreadDepth *tl_depth = nullptr;

		inline ThreadDepth *AcquireDepthSlot()
		{
			uint32_t slot = s_threadSlotCount.fetch_add(1, std::memory_order_relaxed);
			if (slot >= MAX_TRACKED_THREADS)
			{
				return &s_overflowDepth;
			}
			return &s_threadDepths[slot];
		}

		struct InFlightGuard
		{
			ThreadDepth *slot;

			InFlightGuard()
			{
				slot = tl_depth;
				if (!slot)
				{
					slot = AcquireDepthSlot();
					tl_depth = slot;
				}
				// The line is owned by this thread alone, so the lock-prefixed RMW stays in L1 in
				// exclusive state and costs a handful of cycles.
				slot->depth.fetch_add(1, std::memory_order_acquire);
			}

			~InFlightGuard()
			{
				slot->depth.fetch_sub(1, std::memory_order_release);
			}
		};

		bool AnyInFlight()
		{
			uint32_t count = s_threadSlotCount.load(std::memory_order_relaxed);
			if (count > MAX_TRACKED_THREADS)
			{
				count = MAX_TRACKED_THREADS;
			}

			for (uint32_t i = 0; i < count; i++)
			{
				if (s_threadDepths[i].depth.load(std::memory_order_acquire) != 0)
				{
					return true;
				}
			}
			return s_overflowDepth.depth.load(std::memory_order_acquire) != 0;
		}

		//---------------------------------------------------------------------------------------
		// Allocator plumbing filter
		//
		// Several routes into the allocator are themselves inside cs2kz: our operator new family,
		// and (on Windows) the C malloc/free that the SDK's memoverride.cpp compiles into this
		// module. A return address pointing at one of those passes the "is it in cs2kz" test but
		// says nothing about which code wanted the memory, so every allocation on those paths
		// would collapse onto one address.
		//
		// Their entry points are registered here so call-site resolution can step past them.
		//---------------------------------------------------------------------------------------

		constexpr uint32_t MAX_PLUMBING = 16;
		// Generous enough to cover a small forwarding function, tight enough not to swallow
		// unrelated code that happens to be laid out next to it.
		constexpr uintptr_t PLUMBING_SPAN = 512;

		uintptr_t s_plumbing[MAX_PLUMBING];
		uint32_t s_plumbingCount;

		// Taking the address of a function under /INCREMENTAL yields its incremental-link-table
		// entry - a bare `jmp rel32` - not the function itself. Registering the thunk would make
		// the filter miss, because the return addresses on the stack point into the real code.
		const void *ResolveJumpThunks(const void *address)
		{
			const uint8_t *code = (const uint8_t *)address;
			for (uint32_t hops = 0; code && hops < 8; hops++)
			{
				if (code[0] == 0xE9) // jmp rel32
				{
					int32_t displacement = 0;
					memcpy(&displacement, code + 1, sizeof(displacement));
					code = code + 5 + displacement;
					continue;
				}

				if (code[0] == 0xEB) // jmp rel8
				{
					code = code + 2 + (int8_t)code[1];
					continue;
				}

				break;
			}
			return code;
		}

		void RegisterPlumbing(const void *fn)
		{
			if (!fn || s_plumbingCount >= MAX_PLUMBING)
			{
				return;
			}

			fn = ResolveJumpThunks(fn);
			if (!IsSelfAddress(fn))
			{
				return;
			}

			s_plumbing[s_plumbingCount++] = (uintptr_t)fn;
		}

		inline bool IsPlumbing(const void *address)
		{
			uintptr_t addr = (uintptr_t)address;
			for (uint32_t i = 0; i < s_plumbingCount; i++)
			{
				if (addr - s_plumbing[i] < PLUMBING_SPAN)
				{
					return true;
				}
			}
			return false;
		}

		//---------------------------------------------------------------------------------------
		// Sampled stack walking
		//---------------------------------------------------------------------------------------

		std::atomic<uint32_t> s_sampleRate {64};
		thread_local uint32_t tl_sampleCounter = 0;

		// The unwinder can itself allocate (loading unwind tables, loader bookkeeping). If that
		// allocation came back through the hook it would try to walk again, and so on. This guard
		// makes the nested attempt fall straight through to the cheap paths.
		thread_local bool tl_inWalk = false;

#ifdef _WIN32
		const void *FindCallerByStackWalk()
		{
			void *frames[24];
			USHORT captured = RtlCaptureStackBackTrace(1, (DWORD)(sizeof(frames) / sizeof(frames[0])), frames, nullptr);

			const void *fallback = nullptr;
			for (USHORT i = 0; i < captured; i++)
			{
				if (!IsSelfAddress(frames[i]))
				{
					continue;
				}
				if (!IsPlumbing(frames[i]))
				{
					return frames[i];
				}
				if (!fallback)
				{
					fallback = frames[i];
				}
			}
			return fallback;
		}
#else
		// DWARF-based unwind rather than a frame-pointer walk.
		//
		// -fno-omit-frame-pointer only covers our own code. tier0, server and libc are built
		// without frame pointers, so a manual rbp walk breaks the moment it steps out of our thunk
		// into tier0 - which is exactly the case this function exists to resolve, and why call
		// sites on those paths came back as bare foreign addresses. _Unwind_Backtrace reads the
		// CFI instead and walks through them.
		//
		// It is slower than the Windows path and can take a loader lock, so it stays sampled and
		// is pre-warmed at install time (see PrewarmUnwinder).
		struct UnwindState
		{
			const void *found;
			const void *fallback;
			uint32_t depth;
		};

		_Unwind_Reason_Code UnwindFrame(struct _Unwind_Context *context, void *arg)
		{
			UnwindState *state = (UnwindState *)arg;

			if (++state->depth > 24)
			{
				return _URC_END_OF_STACK;
			}

			const void *ip = (const void *)_Unwind_GetIP(context);
			if (!ip)
			{
				return _URC_END_OF_STACK;
			}

			if (IsSelfAddress(ip))
			{
				if (!IsPlumbing(ip))
				{
					state->found = ip;
					return _URC_END_OF_STACK; // stops the unwind
				}
				if (!state->fallback)
				{
					state->fallback = ip;
				}
			}

			return _URC_NO_REASON;
		}

		const void *FindCallerByStackWalk()
		{
			UnwindState state {};
			_Unwind_Backtrace(UnwindFrame, &state);
			return state.found ? state.found : state.fallback;
		}

		// First use of the unwinder can load unwind tables and take loader locks. Doing that once
		// here keeps it out of the first allocation that happens to be sampled.
		void PrewarmUnwinder()
		{
			UnwindState state {};
			_Unwind_Backtrace(UnwindFrame, &state);
		}
#endif

		//---------------------------------------------------------------------------------------
		// Ownership and call-site resolution
		//---------------------------------------------------------------------------------------

		// Is this allocation ours? O(1) - no stack walk. Non-zero depth means the engine is
		// currently executing inside cs2kz, which is the same question "is cs2kz in the stack
		// trace" asks. The return-address test additionally catches our own allocation plumbing
		// on paths that were reached without passing a module entry point.
		inline bool IsOurs(const void *returnAddress)
		{
			return tl_inKZ != 0 || IsSelfAddress(returnAddress);
		}

		// Best available call site, cheapest first.
		inline const void *ResolveCallsite(const void *returnAddress)
		{
			// 1. operator new left us the caller's address directly.
			const void *baton = tl_callsite;
			if (baton)
			{
				tl_callsite = nullptr;
				return baton;
			}

			// 2. The immediate caller is our own code and is not allocator plumbing.
			if (IsSelfAddress(returnAddress) && !IsPlumbing(returnAddress))
			{
				return returnAddress;
			}

			// 3. Otherwise the real caller is further up - through tier0's UtlVectorMemory_Alloc,
			//    or through memoverride's malloc. Finding it needs a walk, so only a sample of
			//    these get one. Blocks are still recorded either way, so leak volume stays exact;
			//    only call-site attribution on these paths is statistical.
			uint32_t rate = s_sampleRate.load(std::memory_order_relaxed);
			if (rate != 0 && !tl_inWalk && ++tl_sampleCounter >= rate)
			{
				tl_sampleCounter = 0;
				tl_inWalk = true;
				NoteSampledWalk();
				const void *caller = FindCallerByStackWalk();
				tl_inWalk = false;

				if (caller)
				{
					return caller;
				}
			}

			// 4. Nothing better available - the plumbing address at least identifies the path.
			return returnAddress;
		}

		//---------------------------------------------------------------------------------------
		// Thunks
		//---------------------------------------------------------------------------------------

		void *Hook_Alloc(void *self, size_t size)
		{
			void *ra = KZ_RETURN_ADDRESS();
			InFlightGuard guard;

			bool ours = IsOurs(ra);
			const void *callsite = ours ? ResolveCallsite(ra) : nullptr;

			void *mem = s_origAlloc(self, size);
			if (mem && ours)
			{
				OnAlloc(mem, size, callsite);
			}
			return mem;
		}

		void *Hook_Realloc(void *self, void *mem, size_t size)
		{
			void *ra = KZ_RETURN_ADDRESS();
			InFlightGuard guard;

			bool ours = IsOurs(ra);
			const void *callsite = ours ? ResolveCallsite(ra) : nullptr;

			// Retire the old block before calling through, not after. The table is keyed on the
			// pointer value, and once realloc returns, the old address may already have been handed
			// to another thread - retiring it afterwards could delete that thread's entry. The cost
			// is that a failed realloc leaves the still-live old block untracked, which is the
			// better trade: this allocator terminates the process on allocation failure.
			if (mem && ours)
			{
				OnFree(mem);
			}

			void *result = s_origRealloc(self, mem, size);
			if (result && ours)
			{
				OnAlloc(result, size, callsite);
			}
			return result;
		}

		void Hook_Free(void *self, void *mem)
		{
			void *ra = KZ_RETURN_ADDRESS();
			InFlightGuard guard;

			// A block of ours that some other module frees will not be retired here; TableInsert
			// notices when the allocator hands the same pointer back and self-heals, counting it as
			// an orphaned free.
			if (mem && IsOurs(ra))
			{
				tl_callsite = nullptr;
				OnFree(mem);
			}
			s_origFree(self, mem);
		}

		void *Hook_AllocAligned(void *self, size_t size, size_t align)
		{
			void *ra = KZ_RETURN_ADDRESS();
			InFlightGuard guard;

			bool ours = IsOurs(ra);
			const void *callsite = ours ? ResolveCallsite(ra) : nullptr;

			void *mem = s_origAllocAligned(self, size, align);
			if (mem && ours)
			{
				OnAlloc(mem, size, callsite);
			}
			return mem;
		}

		void *Hook_ReallocAligned(void *self, void *mem, size_t size, size_t align)
		{
			void *ra = KZ_RETURN_ADDRESS();
			InFlightGuard guard;

			bool ours = IsOurs(ra);
			const void *callsite = ours ? ResolveCallsite(ra) : nullptr;

			if (mem && ours)
			{
				OnFree(mem);
			}

			void *result = s_origReallocAligned(self, mem, size, align);
			if (result && ours)
			{
				OnAlloc(result, size, callsite);
			}
			return result;
		}

		void Hook_FreeAligned(void *self, void *mem)
		{
			void *ra = KZ_RETURN_ADDRESS();
			InFlightGuard guard;

			if (mem && IsOurs(ra))
			{
				tl_callsite = nullptr;
				OnFree(mem);
			}
			s_origFreeAligned(self, mem);
		}

		//---------------------------------------------------------------------------------------
		// vtable index derivation
		//---------------------------------------------------------------------------------------

		// Hand this to SourceHook rather than decoding the pointer-to-member ourselves.
		//
		// The slot indices are not the same on both platforms - IMemAlloc declares Alloc directly
		// after a pure virtual destructor, and the Itanium ABI gives a virtual destructor two vtable
		// slots where MSVC gives one. Alloc is also private, so its address cannot be taken at all.
		//
		// SourceHook::GetFuncInfo is the project's existing tool for exactly this and covers
		// strictly more cases than a hand-rolled decoder: both ABIs, the 32- and 64-bit MSVC vcall
		// thunk forms, vararg thunks, multiple-inheritance this-pointer adjustments, and the
		// `jmp rel32` indirection that /INCREMENTAL puts in front of every function address.
		bool DeriveReallocSlot(uint32_t *pSlot)
		{
			SourceHook::MemFuncInfo info = {};
			info.vtblindex = -1;

			SourceHook::GetFuncInfo(&IMemAlloc::Realloc, info);
			s_funcInfo = info;

			// A non-virtual result, a this-pointer adjustment or a non-zero vtable offset all mean
			// the layout is not what the rest of this file assumes.
			if (!info.isVirtual || info.vtblindex < 1 || info.thisptroffs != 0 || info.vtbloffs != 0)
			{
				return false;
			}

			*pSlot = (uint32_t)info.vtblindex;
			return true;
		}

		bool IsPlausibleTier0Code(void *address)
		{
			if (!address || !modules::tier0)
			{
				return false;
			}
			uintptr_t base = (uintptr_t)modules::tier0->m_base;
			return ((uintptr_t)address - base) < modules::tier0->m_size;
		}

		// Writes one vtable slot as a single aligned pointer-sized store, which is atomic on x86-64.
		// Bytewise copying here would let another thread observe a torn function pointer.
		void WriteSlot(void **vtable, uint32_t index, void *value)
		{
			*(void *volatile *)&vtable[index] = value;
		}
	} // namespace

	bool IsActive()
	{
		return s_active.load(std::memory_order_relaxed);
	}

	const char *GetStatusString()
	{
		return s_status;
	}

	void SetSampleRate(uint32_t oneInN)
	{
		s_sampleRate.store(oneInN, std::memory_order_relaxed);
	}

	uint32_t GetSampleRate()
	{
		return s_sampleRate.load(std::memory_order_relaxed);
	}

	bool Install()
	{
		if (s_active.load(std::memory_order_relaxed))
		{
			return true;
		}

		if (!g_pMemAlloc)
		{
			s_status = "g_pMemAlloc is null";
			return false;
		}

		if (!modules::tier0)
		{
			s_status = "modules::Initialize() has not run yet";
			return false;
		}

		uint32_t reallocSlot = 0;
		if (!DeriveReallocSlot(&reallocSlot))
		{
			V_snprintf(s_statusBuffer, sizeof(s_statusBuffer),
					   "SourceHook could not resolve IMemAlloc::Realloc "
					   "(isVirtual=%d vtblindex=%d thisptroffs=%d vtbloffs=%d)",
					   s_funcInfo.isVirtual ? 1 : 0, s_funcInfo.vtblindex, s_funcInfo.thisptroffs, s_funcInfo.vtbloffs);
			s_status = s_statusBuffer;
			return false;
		}

		// Declared order is: ~IMemAlloc, Alloc, Realloc, Free, AllocAligned, ReallocAligned,
		// FreeAligned. Realloc therefore sits one past Alloc.
		uint32_t slotAlloc = reallocSlot - 1;
		void **vtable = *(void ***)g_pMemAlloc;
		if (!vtable)
		{
			s_status = "g_pMemAlloc has a null vtable pointer";
			return false;
		}

		// Every slot we are about to take over must point at real code inside tier0. This is the
		// check that keeps a wrong index from corrupting the process allocator, and it is why
		// Install() is allowed to fail without taking the server down - see memalloc.h, which warns
		// that parts of this interface are reconstructed guesswork.
		for (uint32_t i = 0; i < 6; i++)
		{
			if (!IsPlausibleTier0Code(vtable[slotAlloc + i]))
			{
				s_status = "an IMemAlloc vtable slot does not point into tier0";
				return false;
			}
		}

		// Alloc, Realloc and Free must be distinct functions. If the linker folded them, the index
		// derivation landed somewhere unexpected.
		if (vtable[slotAlloc] == vtable[slotAlloc + 1] || vtable[slotAlloc + 1] == vtable[slotAlloc + 2])
		{
			s_status = "IMemAlloc vtable slots are not distinct";
			return false;
		}

		if (!AllocateTable())
		{
			s_status = "could not reserve the tracking table";
			return false;
		}

		// Must happen after AllocateTable, which is what caches this module's address range.
		RegisterPlumbing((const void *)(void *(*)(size_t)) & ::operator new);
		RegisterPlumbing((const void *)(void *(*)(size_t)) & ::operator new[]);
		RegisterPlumbing((const void *)(void (*)(void *)) & ::operator delete);
		RegisterPlumbing((const void *)(void (*)(void *)) & ::operator delete[]);
		RegisterPlumbing((const void *)&malloc);
		RegisterPlumbing((const void *)&free);
		RegisterPlumbing((const void *)&realloc);
		RegisterPlumbing((const void *)&calloc);

#ifndef _WIN32
		// Load the unwinder's tables now, while we are still on a plain call path, rather than
		// inside whichever allocation happens to be sampled first.
		PrewarmUnwinder();
#endif

		s_origAlloc = (AllocFn)vtable[slotAlloc + 0];
		s_origRealloc = (ReallocFn)vtable[slotAlloc + 1];
		s_origFree = (FreeFn)vtable[slotAlloc + 2];
		s_origAllocAligned = (AllocAlignedFn)vtable[slotAlloc + 3];
		s_origReallocAligned = (ReallocAlignedFn)vtable[slotAlloc + 4];
		s_origFreeAligned = (FreeAlignedFn)vtable[slotAlloc + 5];

		s_vtable = vtable;
		s_slotAlloc = slotAlloc;

		// Patch the live vtable in place rather than swapping in a copy. The concrete allocator
		// class derives from IMemAlloc and its vtable is longer than the interface declares by an
		// unknown amount, so copying a fixed number of slots would drop whatever tier0 has past the
		// end and crash the first time it called one of them.
		uint32_t oldProtect = 0;
		void *patchBase = &vtable[slotAlloc];
		size_t patchBytes = 6 * sizeof(void *);

		if (!Plat_UnprotectPages(patchBase, patchBytes, &oldProtect))
		{
			ReleaseTable();
			s_status = "could not make the IMemAlloc vtable writable";
			return false;
		}

		WriteSlot(vtable, slotAlloc + 0, (void *)&Hook_Alloc);
		WriteSlot(vtable, slotAlloc + 1, (void *)&Hook_Realloc);
		WriteSlot(vtable, slotAlloc + 2, (void *)&Hook_Free);
		WriteSlot(vtable, slotAlloc + 3, (void *)&Hook_AllocAligned);
		WriteSlot(vtable, slotAlloc + 4, (void *)&Hook_ReallocAligned);
		WriteSlot(vtable, slotAlloc + 5, (void *)&Hook_FreeAligned);

		Plat_ReprotectPages(patchBase, patchBytes, oldProtect);

		s_active.store(true, std::memory_order_release);
		s_status = "active";
		return true;
	}

	void LogStatus()
	{
		if (s_active.load(std::memory_order_relaxed))
		{
			KZ_LOG_INFO(LogChannel::General, "Memory tracking active (IMemAlloc::Alloc at vtable slot %u). Use kz_meminfo.\n", s_slotAlloc);
		}
		else
		{
			KZ_LOG_WARN(LogChannel::General, "Memory tracking unavailable: %s. kz_meminfo will report no data.\n", s_status);
		}
	}

	void Uninstall()
	{
		if (!s_active.load(std::memory_order_relaxed))
		{
			return;
		}

		// Stop new work from entering the bookkeeping path first.
		s_active.store(false, std::memory_order_release);

		uint32_t oldProtect = 0;
		void *patchBase = &s_vtable[s_slotAlloc];
		size_t patchBytes = 6 * sizeof(void *);

		if (Plat_UnprotectPages(patchBase, patchBytes, &oldProtect))
		{
			// Only restore slots we still own. If another plugin patched on top of ours after we
			// installed, writing our saved originals back would silently unhook them too.
			void *ours[6] = {(void *)&Hook_Alloc,        (void *)&Hook_Realloc,        (void *)&Hook_Free,
							 (void *)&Hook_AllocAligned, (void *)&Hook_ReallocAligned, (void *)&Hook_FreeAligned};
			void *originals[6] = {(void *)s_origAlloc,        (void *)s_origRealloc,        (void *)s_origFree,
								  (void *)s_origAllocAligned, (void *)s_origReallocAligned, (void *)s_origFreeAligned};

			for (uint32_t i = 0; i < 6; i++)
			{
				if (s_vtable[s_slotAlloc + i] == ours[i])
				{
					WriteSlot(s_vtable, s_slotAlloc + i, originals[i]);
				}
				else
				{
					KZ_LOG_WARN(LogChannel::General,
								"IMemAlloc vtable slot %u was replaced by another module; leaving it alone. "
								"Unloading CS2KZ may be unsafe.\n",
								s_slotAlloc + i);
				}
			}

			Plat_ReprotectPages(patchBase, patchBytes, oldProtect);
		}
		else
		{
			KZ_LOG_ERROR(LogChannel::General, "Could not make the IMemAlloc vtable writable to uninstall the memory hook.\n");
		}

		// Wait for every thread that is still inside one of our thunks to leave. Without this a
		// thread can be executing our code at the moment the module unmaps.
		//
		// This narrows the window rather than closing it: a thread can read the vtable slot, get
		// descheduled, and enter the thunk after the drain has already finished. Closing that
		// properly needs RCU-style grace periods the engine does not give us. In practice the
		// exposed window is a handful of instructions against a drain that yields for as long as it
		// takes, which is why the timeout below is loud rather than silent.
		for (uint32_t spins = 0; spins < 200000 && AnyInFlight(); spins++)
		{
#ifdef _WIN32
			Sleep(0);
#else
			sched_yield();
#endif
		}

		if (AnyInFlight())
		{
			// Deliberately loud: continuing to unload from here risks unmapping code that another
			// thread is running.
			KZ_LOG_ERROR(LogChannel::General, "Timed out draining in-flight memory hook calls. Unload is unsafe.\n");
		}

		ReleaseTable();
		s_status = "not installed";
	}
} // namespace KZMem
