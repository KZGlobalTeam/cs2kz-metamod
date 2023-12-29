#include "mempatch.h"
#include "plat.h"

#include "tier0/memdbgon.h"

MemPatch::MemPatch(Signature sig, CModule *module, u32 patchLength) : sig(sig), module(module), patchLength(patchLength)
{
	if (!this->address)
	{
		this->address = (void*)this->module->FindSignature((const byte *)this->sig.data, this->sig.length); 
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
	delete patchBytes;
	return true;
}


void MemPatch::Unpatch()
{
	Plat_WriteMemory(this->address, this->oldBytes, this->patchLength);
}