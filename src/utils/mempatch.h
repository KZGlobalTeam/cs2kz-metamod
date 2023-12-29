#include "common.h"
#include "module.h"
#include "addresses.h"

class MemPatch
{
public:
	MemPatch(Signature sig, CModule *module, u32 patchLength);
	
	bool Patch();
	void Unpatch();
private:
	Signature sig;
	u32 patchLength;
	CModule *module;
	void *address{};
	byte *oldBytes{};
};