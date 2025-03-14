#include "plat.h"
#include "module.h"

#include "tier0/memdbgon.h"

void Plat_WriteMemory(void *pPatchAddress, uint8_t *pPatch, int iPatchSize)
{
	WriteProcessMemory(GetCurrentProcess(), pPatchAddress, (void *)pPatch, iPatchSize, nullptr);
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

void *Plat_MemReserve(void *pAddress, size_t size)
{
	return VirtualAlloc(pAddress, size, MEM_RESERVE, PAGE_NOACCESS);
}

void *Plat_MemCommit(void *pAddress, size_t size)
{
	return VirtualAlloc(pAddress, size, MEM_COMMIT, PAGE_READWRITE);
}

void Plat_MemRelease(void *pAddress, size_t size)
{
	VirtualFree(pAddress, 0, MEM_RELEASE);
}
