#pragma once
#include <functional>
#include <tuple>
#include "utils/utils.h"
#include "../../hl2sdk-cs2/public/tier1/utlvector.h"
#include "interfaces.h"

/*
* Credit to Szwagi
*/

void ProcessTimers();
void RemoveNonPersistentTimers();

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
CTimer<Args...>* StartTimer(bool preserveMapChange, typename CTimer<Args...>::Fn fn, Args... args) {
	auto timer = new CTimer<Args...>(fn, args...);
	g_pKZUtils->AddTimer(preserveMapChange, timer);
	return timer;
}

