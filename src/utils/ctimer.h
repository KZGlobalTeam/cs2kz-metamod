#pragma once
#include <functional>
#include "../../hl2sdk-cs2/public/tier1/utlvector.h"
#include "../../src/common.h"

class CTimerBase {
public:
	CTimerBase(float flInitialInterval, bool bPreserveMapChange) :
		m_flInterval(flInitialInterval), m_bPreserveMapChange(bPreserveMapChange) {};

	virtual bool Execute() = 0;

	float m_flInterval;
	float m_flLastExecute = -1;
	bool m_bPreserveMapChange;
};

extern CUtlVector<CTimerBase*> g_timers;

template<typename... Args>
class CTimer : CTimerBase {
	using Fn = void(*)(Args... args);

	Fn m_fn;
	Args m_args;

	void Execute() override {
		m_fn(m_args...);
	}
};

template<typename... Args>
void StartTimer(typename CTimer<Args...>::Fn, Args...);
