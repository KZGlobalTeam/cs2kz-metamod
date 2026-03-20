#ifndef GLOBALSYMBOL_H
#define GLOBALSYMBOL_H

#ifdef _WIN32
#pragma once
#endif

#include "platform.h"

class CGlobalSymbol
{
public:
	CGlobalSymbol() = default;

	explicit CGlobalSymbol(uint64 handle) : m_handle(handle) {}

	operator uint64()
	{
		return Get();
	}

	uint64 Get()
	{
		return m_handle;
	}

private:
	uint64 m_handle;
};

using CGlobalSymbolCaseSensitive = CGlobalSymbol;

PLATFORM_INTERFACE uint64 _FindGlobalSymbolByHash(uint32 hash);
PLATFORM_INTERFACE uint64 _FindGlobalSymbol(const char *str);
PLATFORM_INTERFACE uint64 _MakeGlobalSymbol(const char *str);
PLATFORM_INTERFACE uint64 _MakeGlobalSymbolCaseSensitive(const char *str);

inline CGlobalSymbol FindGlobalSymbolByHash(uint32 hash)
{
	return CGlobalSymbol(_FindGlobalSymbolByHash(hash));
}

inline CGlobalSymbol FindGlobalSymbol(const char *str)
{
	return CGlobalSymbol(_FindGlobalSymbol(str));
}

inline CGlobalSymbol MakeGlobalSymbol(const char *str)
{
	return CGlobalSymbol(_MakeGlobalSymbol(str));
}

inline CGlobalSymbol MakeGlobalSymbolCaseSensitive(const char *str)
{
	return CGlobalSymbol(_MakeGlobalSymbolCaseSensitive(str));
}

#endif // GLOBALSYMBOL_H
