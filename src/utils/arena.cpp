#include "common.h"
#include "plat.h"

#include "tier0/memdbgon.h"

Arena *Plat_ArenaNew(size_t maxSize)
{
	Arena *arena = (Arena *)malloc(sizeof(Arena));
	arena->base = (uintptr_t)Plat_MemReserve(nullptr, maxSize);
	arena->end = arena->base + maxSize;
	arena->committed = arena->base;
	arena->current = arena->base;
	return arena;
}

void *Plat_ArenaAlloc(Arena *arena, size_t size, size_t align)
{
	assert(!(align & (align - 1)));
	uintptr_t result = (arena->current + (align - 1)) & ~(align - 1);
	if (result + size >= arena->committed)
	{
		if (result + size >= arena->end)
		{
			return nullptr;
		}
		size_t overcommit = 4096 * 50;
		size_t commit = size + overcommit;
		if (!Plat_MemCommit((void *)result, commit))
		{
			return nullptr;
		}
		// Not actually true, it's a bit further than that, but doesn't matter.
		arena->committed = result + commit;
	}
	arena->current = result + size;
	return (void *)result;
}

void Plat_ArenaFreeAll(Arena *arena)
{
	arena->current = arena->base;
}
