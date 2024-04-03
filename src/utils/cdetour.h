#pragma once

#include "funchook.h"
#include "module.h"
#include "utlvector.h"
#include "gameconfig.h"

class CDetourBase
{
public:
	virtual const char *GetName() = 0;
	virtual bool CreateDetour(CGameConfig *gameConfig) = 0;
	virtual void EnableDetour() = 0;
	virtual void DisableDetour() = 0;
	virtual void FreeDetour() = 0;
};

template<typename T>
class CDetour : public CDetourBase
{
public:
	CDetour(T *pfnDetour, const char *pszName) : m_pfnDetour(pfnDetour), m_pszName(pszName)
	{
		m_hook = nullptr;
		m_bInstalled = false;
		m_pSignature = nullptr;
		m_pSymbol = nullptr;
		m_pModule = nullptr;
	}

	bool CreateDetour(CGameConfig *gameConfig);
	void EnableDetour() override;
	void DisableDetour() override;
	void FreeDetour() override;

	const char *GetName() override
	{
		return m_pszName;
	}

	T *GetFunc()
	{
		return m_pfnFunc;
	}

	// Shorthand for calling original.
	template<typename... Args>
	auto operator()(Args &&...args)
	{
		return std::invoke(m_pfnFunc, std::forward<Args>(args)...);
	}

private:
	CModule **m_pModule;
	T *m_pfnDetour;
	const char *m_pszName;
	byte *m_pSignature;
	const char *m_pSymbol;
	T *m_pfnFunc;
	funchook_t *m_hook;
	bool m_bInstalled;
};

extern CUtlVector<CDetourBase *> g_vecDetours;

template<typename T>
bool CDetour<T>::CreateDetour(CGameConfig *gameConfig)
{
	m_pfnFunc = (T *)gameConfig->ResolveSignature(m_pszName);
	if (!m_pfnFunc)
	{
		Warning("Could not find address for function for %s\n", m_pszName);
		return false;
	}

	m_hook = funchook_create();
	funchook_prepare(m_hook, (void **)&m_pfnFunc, (void *)m_pfnDetour);

	g_vecDetours.AddToTail(this);
	return true;
}

template<typename T>
void CDetour<T>::EnableDetour()
{
	if (!m_hook)
	{
		Warning("Could not create detour for %s\n", m_pszName);
		return;
	}
	funchook_install(m_hook, 0);
}

template<typename T>
void CDetour<T>::DisableDetour()
{
	funchook_uninstall(m_hook, 0);
}

template<typename T>
void CDetour<T>::FreeDetour()
{
	DisableDetour();
	funchook_destroy(m_hook);
}

#define DECLARE_DETOUR(name, detour) CDetour<decltype(detour)> name(detour, #name)

#define INIT_DETOUR(config, name) \
	name.CreateDetour(config); \
	name.EnableDetour();

void FlushAllDetours();
