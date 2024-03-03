#pragma once
#include <functional>
#include <tuple>
#include "../../hl2sdk-cs2/public/tier1/utlvector.h"
#include "../../src/common.h"

class CTimerBase {
public:
	CTimerBase(float flInitialInterval, bool bPreserveMapChange) :
		m_flInterval(flInitialInterval), m_bPreserveMapChange(bPreserveMapChange) {};

	CTimerBase() {}
	virtual bool Execute() = 0;

	float m_flInterval;
	float m_flLastExecute = -1;
	bool m_bPreserveMapChange;
};

extern CUtlVector<CTimerBase*> g_timers;

template<typename... Args>
class CTimer : public CTimerBase {
public:
	using Fn = float(__cdecl*)(Args... args);

	Fn m_fn;
	std::tuple<Args...> m_args;

	explicit CTimer(bool bPreserveMapChange, Fn fn, Args... args) :
		CTimerBase(0.0f, bPreserveMapChange),
		m_fn(fn),
		m_args(std::make_tuple(std::move(args)...))
	{
	}

	bool Execute() override
	{
		m_flInterval = std::apply(m_fn, m_args);
		return m_flInterval >= 0;
	}
};

template<typename... Args>
void StartTimer(bool bPreserveMapChange, typename CTimer<Args...>::Fn fn, Args... args) {
	auto timer = new CTimer<Args...>(bPreserveMapChange, fn, args...);
	g_timers.AddToTail(timer);
}

void RemoveTimers();
void RemoveMapTimers();

