#pragma once
#include <functional>
#include <tuple>
#include "utils/utils.h"
#include "../../hl2sdk-cs2/public/tier1/utlvector.h"
#include "../../src/common.h"

/*
* Credit to Szwagi
*/

class CTimerBase {
public:
	CTimerBase(float initialInterval) :
		interval(initialInterval) {};

	virtual bool Execute() = 0;

	float interval;
	float lastExecute = -1;

	static_global void ProcessTimers();
	static_global void RemoveNonPersistentTimers();
};

extern CUtlVector<CTimerBase *> g_PersistentTimers;
extern CUtlVector<CTimerBase *> g_NonPersistentTimers;

template<typename... Args>
class CTimer : public CTimerBase {
public:
	using Fn = float(__cdecl*)(Args... args);

	Fn m_fn;
	std::tuple<Args...> m_args;

	explicit CTimer(Fn fn, Args... args) :
		CTimerBase(0.0f),
		m_fn(fn),
		m_args(std::make_tuple(std::move(args)...))
	{
	}

	bool Execute() override
	{
		interval = std::apply(m_fn, m_args);
		return interval > 0;
	}
};

/* Creates a timer for the given function, the function must return a float that represents the interval in seconds; 0 or less to stop the timer */
template<typename... Args>
void StartTimer(bool preserveMapChange, typename CTimer<Args...>::Fn fn, Args... args) {
	auto timer = new CTimer<Args...>(fn, args...);
	if (preserveMapChange)
	{
		g_PersistentTimers.AddToTail(timer);
	}
	else
	{
		g_NonPersistentTimers.AddToTail(timer);
	}
}

