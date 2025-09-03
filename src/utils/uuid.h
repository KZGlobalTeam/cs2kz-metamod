#pragma once
#include "common.h"
#include "random.h"

DLL_IMPORT void Plat_CreateUUIDImpl(void *uuid, int (*randomFunc)(int min, int max));

struct UUID_t
{
	u64 low {};
	u64 high {};

	void Init()
	{
		Plat_CreateUUIDImpl(this, RandomInt);
	}

	bool operator==(const UUID_t &other) const
	{
		return low == other.low && high == other.high;
	}
};
