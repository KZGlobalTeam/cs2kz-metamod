#pragma once
#include <cstdint>

struct Arena
{
	// No copying allowed
	Arena(const Arena &) = delete;
	Arena &operator=(const Arena &) = delete;

	uintptr_t base;
	uintptr_t end;
	uintptr_t committed;
	uintptr_t current;

	Arena(size_t maxSize);
	~Arena();
	void *Alloc(size_t size, size_t align);
	void FreeAll();
};
