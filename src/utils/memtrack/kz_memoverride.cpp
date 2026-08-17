// Global operator new/delete for the cs2kz binary.
//
// The SDK's own memoverride.cpp normally supplies these (routing everything to g_pMemAlloc); we
// take over via the NO_MEMOVERRIDE_NEW_DELETE define it already provides, and behave identically
// except for one addition: each entry point records its own return address in the callsite baton
// before calling through.
//
// That is the whole reason this file exists. The allocator hook sits on IMemAlloc, so without the
// baton the return address it observes for every C++ allocation in the plugin is the inside of
// operator new - a single fixed address, useless for finding which line leaked. Setting the baton
// costs one thread-local store and gives the hook the caller's real address with no stack walking.
//
// Constraints for this translation unit:
//   - never include tier0/memdbgon.h; its macros rewrite the very functions defined here
//   - include nothing that allocates
//   - define the COMPLETE set of overloads. A missing one silently falls back to the CRT's
//     version, which would hand a tier0 pointer to the wrong deallocator.

#include "kz_memtrack_tls.h"

#include "tier0/memalloc.h"

#include <new>

namespace
{
	// Kept out of line so the baton is only touched when the hook can actually consume it. Cheap
	// either way: a relaxed load of a single bool.
	inline void SetBaton(const void *returnAddress)
	{
		if (KZMem::IsActive())
		{
			KZMem::tl_callsite = returnAddress;
		}
	}

	// Always cleared after the call returns. If the hook did not run - because it is not installed,
	// or because the allocation took a path that bypassed it - a leftover baton would mis-attribute
	// the next allocation on this thread to an unrelated call site.
	inline void ClearBaton()
	{
		KZMem::tl_callsite = nullptr;
	}

	inline void *TrackedAlloc(size_t size, const void *returnAddress)
	{
		SetBaton(returnAddress);
		void *mem = MemAlloc_Alloc(size);
		ClearBaton();
		return mem;
	}

	inline void *TrackedAllocAligned(size_t size, size_t alignment, const void *returnAddress)
	{
		SetBaton(returnAddress);
		void *mem = MemAlloc_AllocAlignedUnattributed(size, alignment);
		ClearBaton();
		return mem;
	}

	inline void TrackedFree(void *mem, const void *returnAddress)
	{
		if (!mem)
		{
			return;
		}
		SetBaton(returnAddress);
		g_pMemAlloc->Free(mem);
		ClearBaton();
	}

	inline void TrackedFreeAligned(void *mem, const void *returnAddress)
	{
		if (!mem)
		{
			return;
		}
		SetBaton(returnAddress);
		MemAlloc_FreeAligned(mem);
		ClearBaton();
	}
} // namespace

//---------------------------------------------------------------------------------------------------
// operator new
//
// The build sets -fno-exceptions / _HAS_EXCEPTIONS=0, so these must not throw on failure. Returning
// null matches what the SDK's version does; g_pMemAlloc reports the failure and terminates the
// process on a genuine out-of-memory anyway.
//---------------------------------------------------------------------------------------------------

void *operator new(size_t size)
{
	return TrackedAlloc(size, KZ_RETURN_ADDRESS());
}

void *operator new[](size_t size)
{
	return TrackedAlloc(size, KZ_RETURN_ADDRESS());
}

void *operator new(size_t size, const std::nothrow_t &) noexcept
{
	return TrackedAlloc(size, KZ_RETURN_ADDRESS());
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
	return TrackedAlloc(size, KZ_RETURN_ADDRESS());
}

void *operator new(size_t size, std::align_val_t alignment)
{
	return TrackedAllocAligned(size, (size_t)alignment, KZ_RETURN_ADDRESS());
}

void *operator new[](size_t size, std::align_val_t alignment)
{
	return TrackedAllocAligned(size, (size_t)alignment, KZ_RETURN_ADDRESS());
}

void *operator new(size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
	return TrackedAllocAligned(size, (size_t)alignment, KZ_RETURN_ADDRESS());
}

void *operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
	return TrackedAllocAligned(size, (size_t)alignment, KZ_RETURN_ADDRESS());
}

// Debug-block forms. A build configured with --enable-debug defines _DEBUG, which turns on
// USE_MEM_DEBUG in memdbgon.h and rewrites `new` to `new(_NORMAL_BLOCK, __FILE__, __LINE__)`.
// Without these the debug configuration would fail to link.
void *operator new(size_t size, int, const char *, int)
{
	return TrackedAlloc(size, KZ_RETURN_ADDRESS());
}

void *operator new[](size_t size, int, const char *, int)
{
	return TrackedAlloc(size, KZ_RETURN_ADDRESS());
}

void operator delete(void *mem, int, const char *, int) noexcept
{
	TrackedFree(mem, KZ_RETURN_ADDRESS());
}

void operator delete[](void *mem, int, const char *, int) noexcept
{
	TrackedFree(mem, KZ_RETURN_ADDRESS());
}

//---------------------------------------------------------------------------------------------------
// operator delete
//
// The sized and aligned forms matter: C++17 compilers emit calls to them, and if we left any of
// them undefined the call would bind to the CRT's implementation and free a tier0 pointer with the
// wrong allocator.
//---------------------------------------------------------------------------------------------------

void operator delete(void *mem) noexcept
{
	TrackedFree(mem, KZ_RETURN_ADDRESS());
}

void operator delete[](void *mem) noexcept
{
	TrackedFree(mem, KZ_RETURN_ADDRESS());
}

void operator delete(void *mem, size_t) noexcept
{
	TrackedFree(mem, KZ_RETURN_ADDRESS());
}

void operator delete[](void *mem, size_t) noexcept
{
	TrackedFree(mem, KZ_RETURN_ADDRESS());
}

void operator delete(void *mem, const std::nothrow_t &) noexcept
{
	TrackedFree(mem, KZ_RETURN_ADDRESS());
}

void operator delete[](void *mem, const std::nothrow_t &) noexcept
{
	TrackedFree(mem, KZ_RETURN_ADDRESS());
}

void operator delete(void *mem, std::align_val_t) noexcept
{
	TrackedFreeAligned(mem, KZ_RETURN_ADDRESS());
}

void operator delete[](void *mem, std::align_val_t) noexcept
{
	TrackedFreeAligned(mem, KZ_RETURN_ADDRESS());
}

void operator delete(void *mem, size_t, std::align_val_t) noexcept
{
	TrackedFreeAligned(mem, KZ_RETURN_ADDRESS());
}

void operator delete[](void *mem, size_t, std::align_val_t) noexcept
{
	TrackedFreeAligned(mem, KZ_RETURN_ADDRESS());
}

void operator delete(void *mem, std::align_val_t, const std::nothrow_t &) noexcept
{
	TrackedFreeAligned(mem, KZ_RETURN_ADDRESS());
}

void operator delete[](void *mem, std::align_val_t, const std::nothrow_t &) noexcept
{
	TrackedFreeAligned(mem, KZ_RETURN_ADDRESS());
}
