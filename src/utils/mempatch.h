
#ifndef MEMPATCH_H
#define MEMPATCH_H

#include "common.h"
#include "module.h"
#include "addresses.h"
class CGameConfig;

class MemPatch
{
	public:
	// the signature of the patch and the patch itself have to have the same name
	MemPatch(CGameConfig *gameConfig, const char *patchName);
	
	bool Patch();
	void Unpatch();

private:
	i64 patchLength;
	CModule *module;
	void *address {};
	byte *patchBytes {};
	byte *oldBytes {};
};

#endif // MEMPATCH_H
