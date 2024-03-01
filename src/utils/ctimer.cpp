#include "ctimer.h"

CUtlVector<CTimerBase*> g_timers;

template<typename... Args>
void StartTimer(CTimer<Args...>::Fn fn, Args... args) {
	auto timer = new CTimer<Args...>{ fn, ...args };
	g_timers.AddToTail(timer);
}
