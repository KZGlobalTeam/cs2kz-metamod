#pragma once

// Linux-only allocation tracker that detours IMemAlloc entry points.
// On non-POSIX platforms the functions are stubbed.
namespace memtracker
{
	void Init();
	void Shutdown();
} // namespace memtracker

// Exposed command helper
void PrintTopAllocatingStacks(int topCount);
void PrintTopAllocatingStacksDelta(int topCount);
void SetAllocCheckpoint();
