/*
	Credit to CS2Fixes: https://github.com/Source2ZE/CS2Fixes/blob/0eae82a64c71466ca498e3719f1cf3e61b174dee/src/ctimer.h
*/

#pragma once
#include <functional>
#include "utllinkedlist.h"

class CTimerBase {
public:
	CTimerBase(float flInitialInterval, bool bPreserveMapChange) :
		m_flInterval(flInitialInterval), m_bPreserveMapChange(bPreserveMapChange) {};

    virtual bool Execute() = 0;

    float m_flInterval;
    float m_flLastExecute = -1;
    bool m_bPreserveMapChange;
};

extern CUtlLinkedList<CTimerBase*> g_timers;

// Timer functions should return the time until next execution, or a negative value like -1.0f to stop
// Having an interval of 0 is fine, in this case it will run on every game frame
class CTimer : public CTimerBase
{
public:
    CTimer(float flInitialInterval, bool bPreserveMapChange, std::function<float()> func) :
		CTimerBase(flInitialInterval, bPreserveMapChange), m_func(func)
    {
        g_timers.AddToTail(this);
    };

    inline bool Execute() override
    {
	    m_flInterval = m_func();

        return m_flInterval >= 0;
	}

    std::function<float()> m_func;
};


void RemoveTimers();
void RemoveMapTimers();
