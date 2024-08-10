#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "metamod_oslink.h"

// Needs to be here because later MSVC versions don't need this anymore
#if !defined __linux__ && !defined __APPLE__ && defined _MSC_VER && _MSC_VER > 1500
#undef snprintf
#endif

struct Section
{
	std::string m_szName;
	void *m_pBase;
	size_t m_iSize;
};

#if defined(_WIN32)
#define FASTCALL __fastcall
#define THISCALL __thiscall
#else
#define THISCALL
#define strtok_s strtok_r
#endif

struct Module
{
#ifndef _WIN32
	void *pHandle;
#endif
	uint8_t *pBase;
	unsigned int nSize;
};

#ifndef _WIN32
int GetModuleInformation(HINSTANCE module, void **base, size_t *length, std::vector<Section> &m_sections);
#endif

#ifdef _WIN32
#define MODULE_PREFIX ""
#define MODULE_EXT    ".dll"
#else
#define MODULE_PREFIX "lib"
#define MODULE_EXT    ".so"
#endif

void Plat_WriteMemory(void *pPatchAddress, uint8_t *pPatch, int iPatchSize);
