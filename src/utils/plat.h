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

// Resident set size of the whole server process, in bytes. Returns 0 if unavailable.
size_t Plat_GetProcessRSS();

// Address range of the module this code is compiled into (i.e. cs2kz itself).
// Used to decide whether a return address belongs to us. Returns false on failure.
bool Plat_GetSelfModuleRange(uintptr_t *pBase, size_t *pSize);

// Raw virtual memory, deliberately bypassing g_pMemAlloc / the CRT. The memory tracker's own
// bookkeeping must not allocate through the allocator it is measuring, or it recurses forever.
// Reserve is address space only; pages must be committed before use.
void *Plat_ReservePages(size_t bytes);
bool Plat_CommitPages(void *pBase, size_t bytes);
void Plat_ReleasePages(void *pBase, size_t bytes);

// Temporarily make the pages spanning [pAddr, pAddr + bytes) writable, so an aligned pointer-sized
// store can be issued directly. Plat_WriteMemory is not usable for patching a live vtable: it
// copies bytewise, and a reader on another thread could observe a torn function pointer.
bool Plat_UnprotectPages(void *pAddr, size_t bytes, uint32_t *pOldProtect);
void Plat_ReprotectPages(void *pAddr, size_t bytes, uint32_t oldProtect);

// Name the loaded module owning a code address ("tier0.dll" / "libtier0.so") and report its base,
// so a raw pointer can be turned into a module-relative offset for addr2line or a .pdb.
// False if the address does not belong to any loaded module.
bool Plat_GetModuleForAddress(const void *pAddr, char *moduleName, size_t moduleNameSize, uintptr_t *pModuleBase);
