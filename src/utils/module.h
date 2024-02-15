#pragma once
#include "dbg.h"
#include "interface.h"
#include "strtools.h"
#include "plat.h"

#ifdef _WIN32
#include <Psapi.h>
#endif

enum SigError
{
	SIG_OK,
	SIG_NOT_FOUND,
	SIG_FOUND_MULTIPLE,
};

class CModule
{
public:
	CModule(const char *path, const char *module) :
		m_pszModule(module), m_pszPath(path)
	{
		char szModule[MAX_PATH];

		V_snprintf(szModule, MAX_PATH, "%s%s%s%s%s", Plat_GetGameDirectory(), path, MODULE_PREFIX, m_pszModule, MODULE_EXT);

		m_hModule = dlmount(szModule);

		if (!m_hModule)
			Error("Could not find %s\n", szModule);

#ifdef _WIN32
		MODULEINFO m_hModuleInfo;
		GetModuleInformation(GetCurrentProcess(), m_hModule, &m_hModuleInfo, sizeof(m_hModuleInfo));

		m_base = (void *)m_hModuleInfo.lpBaseOfDll;
		m_size = m_hModuleInfo.SizeOfImage;
#else
		if (int e = GetModuleInformation(m_hModule, &m_base, &m_size))
			Error("Failed to get module info for %s, error %d\n", szModule, e);
#endif
	}

	void *FindSignature(const byte *pData, size_t length, int &error)
	{
		unsigned char *pMemory;
		void *return_addr = nullptr;
		error = 0;
		
		pMemory = (byte*)m_base;
		
		for (size_t i = 0; i < m_size; i++)
		{
			size_t Matches = 0;
			while (*(pMemory + i + Matches) == pData[Matches] || pData[Matches] == '\x2A')
			{
				Matches++;
				if (Matches == length)
				{
					if (return_addr)
					{
						error = SIG_FOUND_MULTIPLE;
						return return_addr;
					}
					
					return_addr = (void *)(pMemory + i);
				}
			}
		}
		
		if (!return_addr)
		{
			error = SIG_NOT_FOUND;
		}
		
		return return_addr;
	}

	const char *m_pszModule;
	const char* m_pszPath;
	HINSTANCE m_hModule;
	void* m_base;
	size_t m_size;
};
