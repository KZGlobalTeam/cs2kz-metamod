#pragma once

// Deliberately dependency-free.
//
// This header is included by kz_memoverride.cpp, which defines the global operator new/delete
// family. That translation unit must not pull in anything that allocates, and must never see
// tier0/memdbgon.h (which would macro-rewrite the very functions being defined here).

#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#define KZ_RETURN_ADDRESS() _ReturnAddress()
#else
#define KZ_RETURN_ADDRESS() __builtin_return_address(0)
#endif

// KZ_MEMTRACK_ENABLED is defined for the main cs2kz binary only (see AMBuilder). The mode and
// style plugins are separate modules that do not compile the tracker, so the declarations below
// would be unresolved symbols there - and these headers are reachable from shared code.
#ifdef KZ_MEMTRACK_ENABLED

namespace KZMem
{
	// "Is cs2kz on the stack?", answered in O(1).
	//
	// The allocator hook runs for every allocation in the whole server process, so the ownership
	// test has to be nearly free - a stack walk costs ~400ns against ~5ns for this, and at server
	// allocation rates that is the difference between a rounding error and a fifth of a core.
	//
	// Instead of walking, we mark the boundary: every point where the engine calls into this
	// module bumps this counter (see KZ_MEM_MODULE_SCOPE). Non-zero means execution is somewhere
	// inside cs2kz, which is the same question a stack walk would answer.
	//
	// Note this deliberately includes allocations the engine makes on our behalf while we are
	// inside one of our own hooks - if our code caused it, it is our footprint.
	extern thread_local uint32_t tl_inKZ;

	// The "callsite baton".
	//
	// The hook sits on IMemAlloc, so the return address it sees for a C++ allocation is the inside
	// of our own operator new - one fixed address, useless for finding which line leaked. operator
	// new therefore stashes its own return address here first; the hook consumes it and clears it.
	// One TLS store in, one TLS load out - no stack walking on the common path.
	//
	// Always cleared by the setter after the call returns, so a stale baton cannot leak into an
	// unrelated allocation if the hook is not installed.
	extern thread_local const void *tl_callsite;

	// Reentrancy guard. The tracker's own bookkeeping must never recurse into the allocator it is
	// measuring.
	extern thread_local bool tl_inHook;

	// True once the vtable swap is live. Relaxed load; only used to skip baton stores.
	bool IsActive();

	// RAII marker for a point where the engine enters this module: SourceHook hook bodies,
	// funchook detours, console command callbacks, our own thread entry points.
	struct ModuleScope
	{
		ModuleScope()
		{
			tl_inKZ++;
		}

		~ModuleScope()
		{
			tl_inKZ--;
		}

		ModuleScope(const ModuleScope &) = delete;
		ModuleScope &operator=(const ModuleScope &) = delete;
	};

	// Suppresses ownership for a nested region - use where we hand control back to the engine for
	// work that is genuinely the engine's, so its allocations do not get charged to us.
	struct ModuleSuppressScope
	{
		uint32_t saved;

		ModuleSuppressScope() : saved(tl_inKZ)
		{
			tl_inKZ = 0;
		}

		~ModuleSuppressScope()
		{
			tl_inKZ = saved;
		}

		ModuleSuppressScope(const ModuleSuppressScope &) = delete;
		ModuleSuppressScope &operator=(const ModuleSuppressScope &) = delete;
	};
} // namespace KZMem

#define KZ_MEM_JOIN2(a, b) a##b
#define KZ_MEM_JOIN(a, b)  KZ_MEM_JOIN2(a, b)

// Put this at the top of any function the engine calls into us through. Everything allocated
// below it, including inside tier0 helpers like UtlVectorMemory_Alloc, counts as ours.
#define KZ_MEM_MODULE_SCOPE()   KZMem::ModuleScope KZ_MEM_JOIN(kzMemModule_, __LINE__)
#define KZ_MEM_SUPPRESS_SCOPE() KZMem::ModuleSuppressScope KZ_MEM_JOIN(kzMemSuppress_, __LINE__)

#else // KZ_MEMTRACK_ENABLED

#define KZ_MEM_MODULE_SCOPE()   ((void)0)
#define KZ_MEM_SUPPRESS_SCOPE() ((void)0)

#endif // KZ_MEMTRACK_ENABLED
