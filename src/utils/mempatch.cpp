#include "mempatch.h"
#include "plat.h"

#include "tier0/memdbgon.h"

MemPatch::MemPatch(byte *patch, CModule *module, u32 patchLength) : patch(patch), module(module), patchLength(patchLength)
{
	if (!this->address)
	{
		int sigError;
		this->address = (void *)this->module->FindSignature(patch, sizeof(patch) - 1, sigError);
		if (!this->address)
		{
			return;
		}
	}
	this->oldBytes = new byte[this->patchLength];
	V_memcpy(this->oldBytes, this->address, this->patchLength);
}

bool MemPatch::Patch()
{
	if (!this->address)
	{
		return false;
	}

	byte *patchBytes = new byte[this->patchLength];
	for (u32 i = 0; i < this->patchLength; i++)
	{
		patchBytes[i] = 0x90;
	}
	Plat_WriteMemory(this->address, patchBytes, this->patchLength);
	delete[] patchBytes;
	return true;
}

void MemPatch::Unpatch()
{
	Plat_WriteMemory(this->address, this->oldBytes, this->patchLength);
}
