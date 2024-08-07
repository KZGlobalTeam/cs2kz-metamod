#include "common.h"
#include "module.h"
#include "addresses.h"

class MemPatch
{
public:
	MemPatch(byte *patch, CModule *module, u32 patchLength);

	bool Patch();
	void Unpatch();

private:
	byte *patch;
	CModule *module;
	u32 patchLength;
	void *address {};
	byte *oldBytes {};
};
