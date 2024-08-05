#pragma once
#include "common.h"
#include "igamesystemfactory.h"

class IEntityResourceManifest
{
public:
	virtual void AddResource(const char *) = 0;
	virtual void AddResource(const char *, void *) = 0;
	virtual void AddResource(const char *, void *, void *, void *) = 0;
	virtual void unk_04() = 0;
	virtual void unk_05() = 0;
	virtual void unk_06() = 0;
	virtual void unk_07() = 0;
	virtual void unk_08() = 0;
	virtual void unk_09() = 0;
	virtual void unk_10() = 0;
};
