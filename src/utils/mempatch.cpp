#include "mempatch.h"
#include "plat.h"
#include "gameconfig.h"

#include "tier0/memdbgon.h"

MemPatch::MemPatch(CGameConfig *gameConfig, const char *patchName)
{
	if (!patchName || !gameConfig)
	{
		return;
	}
	
	this->address = gameConfig->ResolveSignature(patchName);
	if (!this->address)
	{
		Warning("Failed to find address for %s!\n", patchName);
		return;
	}
	
	const char *patchHex = gameConfig->GetPatch(patchName);
	if (!patchHex)
	{
		Warning("Failed to find patch \"%s\"!\n", patchName);
		return;
	}
	
	size_t patchHexLength = V_strlen(patchHex);
	this->patchBytes = new byte[patchHexLength / 4];
	this->patchLength = gameConfig->HexStringToUint8Array(patchHex, this->patchBytes, patchHexLength);
	if (this->patchLength <= 0)
	{
		delete this->patchBytes;
		this->patchBytes = NULL;
		this->patchLength = 0;
		return;
	}
	this->oldBytes = new byte[this->patchLength];
	V_memcpy(this->oldBytes, this->address, this->patchLength);
}

bool MemPatch::Patch()
{
	if (!this->address || !this->patchBytes || this->patchLength <= 0)
	{
		return false;
	}

	Plat_WriteMemory(this->address, this->patchBytes, this->patchLength);
	return true;
}

void MemPatch::Unpatch()
{
	if (!this->address || !this->oldBytes || this->patchLength <= 0)
	{
		return;
	}
	
	Plat_WriteMemory(this->address, this->oldBytes, this->patchLength);
}
