
#ifndef UTLSLOT_H
#define UTLSLOT_H

#ifdef _WIN32
#pragma once
#endif

#include "basetypes.h"
#include "utlstring.h"
#include "utldelegateimpl.h"
#include "utlvector.h"
#include "threadtools.h"

enum CopiedLockState_t : int32
{
	CLS_NOCOPY = 0,
	CLS_UNLOCKED = 1,
	CLS_LOCKED_BY_COPYING_THREAD = 2,
};

template<class MUTEX, CopiedLockState_t L = CLS_UNLOCKED>
class CCopyableLock : public MUTEX
{
	typedef MUTEX BaseClass;

public:
	// ...
};

class CUtlSlot;

class CUtlSignaller_Base
{
public:
	using Delegate_t = CUtlDelegate<void(CUtlSlot *)>;

	CUtlSignaller_Base(const Delegate_t &other) : m_SlotDeletionDelegate(other) {}

	CUtlSignaller_Base(Delegate_t &&other) : m_SlotDeletionDelegate(Move(other)) {}

private:
	Delegate_t m_SlotDeletionDelegate;
};

class CUtlSlot
{
public:
	using MTElement_t = CUtlVector<CUtlSignaller_Base *>;

	CUtlSlot() : m_ConnectedSignallers(0, 1) {}

private:
	CCopyableLock<CThreadFastMutex> m_Mutex;
	CUtlVector<CUtlSignaller_Base *> m_ConnectedSignallers;
};

#endif // UTLSLOT_H
