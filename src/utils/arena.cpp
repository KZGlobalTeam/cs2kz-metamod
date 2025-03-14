#include "common.h"
#include "arena.h"
#include "plat.h"

#include "tier0/memdbgon.h"

Arena::Arena(size_t maxSize)
{
	maxSize = (maxSize + 4095) & ~4095;
	this->base = (uintptr_t)Plat_MemReserve(nullptr, maxSize);
	this->end = this->base + maxSize;
	this->committed = this->base;
	this->current = this->base;
}

void *Arena::Alloc(size_t size, size_t align)
{
	assert(!(align & (align - 1)));
	uintptr_t result = (this->current + (align - 1)) & ~(align - 1);
	if (result + size >= this->committed)
	{
		if (result + size >= this->end)
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
		this->committed = result + commit;
	}
	this->current = result + size;
	return (void *)result;
}

void Arena::FreeAll()
{
	this->current = this->base;
}
