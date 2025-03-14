#pragma once
#include <cstdint>

struct Arena
{
	uintptr_t base;
	uintptr_t end;
	uintptr_t committed;
	uintptr_t current;

	Arena(size_t maxSize);
	void *Alloc(size_t size, size_t align);
	void FreeAll();
};
