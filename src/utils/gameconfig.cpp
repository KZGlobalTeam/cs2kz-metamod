/*
	Credit to CS2Fixes: https://github.com/Source2ZE/CS2Fixes/blame/main/src/gameconfig.cpp
*/

#include <algorithm>
#include "gameconfig.h"
#include "addresses.h"
#include "khook.hpp"

CGameConfig::CGameConfig(const std::string &gameDir, const std::string &path)
{
	this->m_szGameDir = gameDir;
	this->m_szPath = path;
	this->m_pKeyValues = new KeyValues("Games");
}

CGameConfig::~CGameConfig()
{
	delete m_pKeyValues;
}

bool CGameConfig::Init(IFileSystem *filesystem, char *conf_error, int conf_error_size)
{
	if (!m_pKeyValues->LoadFromFile(filesystem, m_szPath.c_str(), nullptr))
	{
		snprintf(conf_error, conf_error_size, "Failed to load gamedata file");
		return false;
	}

	KeyValues *game = m_pKeyValues->FindKey(m_szGameDir.c_str());
	if (game)
	{
#if defined _LINUX
		const char *platform = "linux";
#else
		const char *platform = "windows";
#endif

		KeyValues *offsets = game->FindKey("Offsets");
		if (offsets)
		{
			FOR_EACH_SUBKEY(offsets, it)
			{
				m_umOffsets[it->GetName()] = it->GetInt(platform, -1);
			}
		}

		KeyValues *signatures = game->FindKey("Signatures");
		if (signatures)
		{
			FOR_EACH_SUBKEY(signatures, it)
			{
				m_umLibraries[it->GetName()] = std::string(it->GetString("library"));
				m_umSignatures[it->GetName()] = std::string(it->GetString(platform));
				m_umAllowMultiMatch[it->GetName()] = it->GetBool("allow_multi_match", false);
			}
		}

		KeyValues *patches = game->FindKey("Patches");
		if (patches)
		{
			FOR_EACH_SUBKEY(patches, it)
			{
				m_umPatches[it->GetName()] = std::string(it->GetString(platform));
			}
		}
	}
	else
	{
		snprintf(conf_error, conf_error_size, "Failed to find game: %s", m_szGameDir.c_str());
		return false;
	}
	return true;
}

const std::string CGameConfig::GetPath()
{
	return m_szPath;
}

const char *CGameConfig::GetSignature(const std::string &name)
{
	auto it = m_umSignatures.find(name);
	if (it == m_umSignatures.end())
	{
		return nullptr;
	}
	return it->second.c_str();
}

const char *CGameConfig::GetPatch(const std::string &name)
{
	auto it = m_umPatches.find(name);
	if (it == m_umPatches.end())
	{
		return nullptr;
	}
	return it->second.c_str();
}

int CGameConfig::GetOffset(const std::string &name)
{
	auto it = m_umOffsets.find(name);
	if (it == m_umOffsets.end())
	{
		return -1;
	}
	return it->second;
}

const char *CGameConfig::GetLibrary(const std::string &name)
{
	auto it = m_umLibraries.find(name);
	if (it == m_umLibraries.end())
	{
		return nullptr;
	}
	return it->second.c_str();
}

CModule **CGameConfig::GetModule(const char *name)
{
	const char *library = this->GetLibrary(name);
	if (!library)
	{
		return nullptr;
	}

	if (strcmp(library, "engine") == 0)
	{
		return &modules::engine;
	}
	else if (strcmp(library, "server") == 0)
	{
		return &modules::server;
	}
	else if (strcmp(library, "tier0") == 0)
	{
		return &modules::tier0;
	}
	else if (strcmp(library, "schemasystem") == 0)
	{
		return &modules::schemasystem;
	}
	else if (strcmp(library, "steamnetworkingsockets") == 0)
	{
		return &modules::steamnetworkingsockets;
	}
	return nullptr;
}

bool CGameConfig::IsSymbol(const char *name)
{
	const char *sigOrSymbol = this->GetSignature(name);
	if (!sigOrSymbol || strlen(sigOrSymbol) <= 0)
	{
		Warning("Missing signature or symbol %s\n", name);
		return false;
	}
	return sigOrSymbol[0] == '@';
}

const char *CGameConfig::GetSymbol(const char *name)
{
	const char *symbol = this->GetSignature(name);

	if (!symbol || strlen(symbol) <= 1)
	{
		Warning("Missing symbol %s\n", name);
		return nullptr;
	}
	return symbol + 1;
}

void *CGameConfig::ResolveSignature(const char *name)
{
	CModule **module = this->GetModule(name);
	if (!module || !(*module))
	{
		Warning("Invalid Module %s\n", name);
		return nullptr;
	}
	void *address = nullptr;
	if (this->IsSymbol(name))
	{
		const char *symbol = this->GetSymbol(name);
		if (!symbol)
		{
			Warning("Invalid symbol for %s\n", name);
			return nullptr;
		}
		address = dlsym((*module)->m_hModule, symbol);
	}
	else
	{
		const char *signature = this->GetSignature(name);
		if (!signature || !signature[0])
		{
			Warning("Failed to find signature for %s\n", name);
			return nullptr;
		}

		byte *base = (byte *)(*module)->m_base;
		size_t sigBytes = std::count(signature, signature + strlen(signature), ' ') + 1;
		size_t scanSize = (*module)->m_size - sigBytes + 1;
		address = KHook::LookupSignature(base, scanSize, signature);
		// LookupSignature stops at the first match, so scan the rest of the module to reject ambiguous signatures.
		if (address && !m_umAllowMultiMatch[name])
		{
			byte *next = (byte *)address + 1;
			if (KHook::LookupSignature(next, scanSize - (next - base), signature))
			{
				Warning("Multiple addresses found for %s, defaulting to nullptr\n", name);
				return nullptr;
			}
		}
	}

	if (!address)
	{
		Warning("Failed to find address for %s\n", name);
		return nullptr;
	}
	return address;
}

void *CGameConfig::ResolveSignatureFromMov(const char *name)
{
	// Convoluted way of having GameEventManager regardless of lateloading
	u8 *ptr = (u8 *)this->ResolveSignature(name);

	if (!ptr)
	{
		return nullptr;
	}

	ptr += 3;
	// Grab the offset as 4 bytes
	u32 offset = *(u32 *)ptr;

	// Go to the next instruction, which is the starting point of the relative jump
	ptr += 4;

	// Now grab our pointer
	return (void *)(ptr + offset);
}

// Static functions
std::string CGameConfig::GetDirectoryName(const std::string &directoryPathInput)
{
	std::string directoryPath = std::string(directoryPathInput);

	size_t found = std::string(directoryPath).find_last_of("/\\");
	if (found != std::string::npos)
	{
		return std::string(directoryPath, found + 1);
	}
	return "";
}
