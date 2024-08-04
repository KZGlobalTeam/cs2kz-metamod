#pragma once
#include "common.h"
#include <functional>
#include <tuple>
#include "utils/utils.h"
#include "../../hl2sdk-cs2/public/tier1/utlvector.h"
#include "interfaces.h"

/*
 * Credit to Szwagi
 */

class CTimerBase
{
public:
	CTimerBase(f64 initialInterval, bool useRealTime) : interval(initialInterval), useRealTime(useRealTime) {};

	virtual bool Execute() = 0;

	f64 interval {};
	f64 lastExecute = -1;
	bool useRealTime {};
};

void ProcessTimers();
void RemoveNonPersistentTimers();

extern CUtlVector<CTimerBase *> g_NonPersistentTimers;
extern CUtlVector<CTimerBase *> g_PersistentTimers;

template<typename... Args>
class CTimer : public CTimerBase
{
public:
	using Fn = f64(__cdecl *)(Args... args);

	Fn m_fn;
	std::tuple<Args...> m_args;

	explicit CTimer(bool useRealTime, f32 initialDelay, Fn fn, Args... args)
		: CTimerBase(initialDelay, useRealTime), m_fn(fn), m_args(std::make_tuple(std::move(args)...))
	{
	}

	bool Execute() override
	{
		interval = std::apply(m_fn, m_args);
		return interval > 0;
	}
};

/* Creates a timer for the given function, the function must return a f64 that represents the interval in seconds; 0 or less to stop the timer */
template<typename... Args>
CTimer<Args...> *StartTimer(typename CTimer<Args...>::Fn fn, Args... args, f64 initialDelay, bool preserveMapChange = true, bool useRealTime = false)
{
	auto timer = new CTimer<Args...>(useRealTime, initialDelay, fn, args...);
	g_pKZUtils->AddTimer(timer, preserveMapChange);
	return timer;
}
