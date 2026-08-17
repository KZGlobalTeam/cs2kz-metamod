#include "plat.h"
#include "module.h"

#include "tier0/memdbgon.h"

void Plat_WriteMemory(void *pPatchAddress, uint8_t *pPatch, int iPatchSize)
{
	WriteProcessMemory(GetCurrentProcess(), pPatchAddress, (void *)pPatch, iPatchSize, nullptr);
}

size_t Plat_GetProcessRSS()
{
	PROCESS_MEMORY_COUNTERS pmc {};
	pmc.cb = sizeof(pmc);
	if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
	{
		return 0;
	}
	return (size_t)pmc.WorkingSetSize;
}

bool Plat_GetSelfModuleRange(uintptr_t *pBase, size_t *pSize)
{
	// Take the address of a function in this module as the anchor.
	HMODULE hSelf = nullptr;
	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Plat_WriteMemory, &hSelf)
		|| !hSelf)
	{
		return false;
	}

	MODULEINFO info {};
	if (!GetModuleInformation(GetCurrentProcess(), hSelf, &info, sizeof(info)))
	{
		return false;
	}

	*pBase = (uintptr_t)info.lpBaseOfDll;
	*pSize = (size_t)info.SizeOfImage;
	return true;
}

void *Plat_ReservePages(size_t bytes)
{
	return VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE);
}

bool Plat_CommitPages(void *pBase, size_t bytes)
{
	return VirtualAlloc(pBase, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

void Plat_ReleasePages(void *pBase, size_t bytes)
{
	// MEM_RELEASE requires the size to be 0 and the base to be the original reservation.
	(void)bytes;
	VirtualFree(pBase, 0, MEM_RELEASE);
}

bool Plat_UnprotectPages(void *pAddr, size_t bytes, uint32_t *pOldProtect)
{
	DWORD oldProtect = 0;
	if (!VirtualProtect(pAddr, bytes, PAGE_READWRITE, &oldProtect))
	{
		return false;
	}
	*pOldProtect = (uint32_t)oldProtect;
	return true;
}

void Plat_ReprotectPages(void *pAddr, size_t bytes, uint32_t oldProtect)
{
	DWORD ignored = 0;
	VirtualProtect(pAddr, bytes, (DWORD)oldProtect, &ignored);
}

void CModule::InitializeSections()
{
	IMAGE_DOS_HEADER *pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER *>(m_hModule);
	IMAGE_NT_HEADERS *pNtHeader = reinterpret_cast<IMAGE_NT_HEADERS64 *>(reinterpret_cast<uintptr_t>(m_hModule) + pDosHeader->e_lfanew);

	IMAGE_SECTION_HEADER *pSectionHeader = IMAGE_FIRST_SECTION(pNtHeader);

	for (int i = 0; i < pNtHeader->FileHeader.NumberOfSections; i++)
	{
		Section section;
		section.m_szName = (char *)pSectionHeader[i].Name;
		section.m_pBase = (void *)((uint8_t *)m_base + pSectionHeader[i].VirtualAddress);
		section.m_iSize = pSectionHeader[i].SizeOfRawData;

		m_sections.push_back(std::move(section));
	}
}

void *CModule::FindVirtualTable(const std::string &name)
{
	auto runTimeData = GetSection(".data");
	auto readOnlyData = GetSection(".rdata");

	if (!runTimeData || !readOnlyData)
	{
		Warning("Failed to find .data or .rdata section\n");
		return nullptr;
	}

	std::string decoratedTableName = ".?AV" + name + "@@";

	SignatureIterator sigIt(runTimeData->m_pBase, runTimeData->m_iSize, (const byte *)decoratedTableName.c_str(), decoratedTableName.size() + 1);
	void *typeDescriptor = sigIt.FindNext(false);

	if (!typeDescriptor)
	{
		Warning("Failed to find type descriptor for %s\n", name.c_str());
		return nullptr;
	}

	typeDescriptor = (void *)((uintptr_t)typeDescriptor - 0x10);

	const uint32_t rttiTDRva = (uintptr_t)typeDescriptor - (uintptr_t)m_base;

	DevMsg("RTTI Type Descriptor RVA: 0x%p\n", rttiTDRva);

	SignatureIterator sigIt2(readOnlyData->m_pBase, readOnlyData->m_iSize, (const byte *)&rttiTDRva, sizeof(uint32_t));

	while (void *completeObjectLocator = sigIt2.FindNext(false))
	{
		auto completeObjectLocatorHeader = (uintptr_t)completeObjectLocator - 0xC;
		// check RTTI Complete Object Locator header, always 0x1
		if (*(int32_t *)(completeObjectLocatorHeader) != 1)
		{
			continue;
		}

		// check RTTI Complete Object Locator vtable offset
		if (*(int32_t *)((uintptr_t)completeObjectLocator - 0x8) != 0)
		{
			continue;
		}

		SignatureIterator sigIt3(readOnlyData->m_pBase, readOnlyData->m_iSize, (const byte *)&completeObjectLocatorHeader, sizeof(void *));
		void *vtable = sigIt3.FindNext(false);

		if (!vtable)
		{
			Warning("Failed to find vtable for %s\n", name.c_str());
			return nullptr;
		}

		return (void *)((uintptr_t)vtable + 0x8);
	}

	Warning("Failed to find RTTI Complete Object Locator for %s\n", name.c_str());
	return nullptr;
}
