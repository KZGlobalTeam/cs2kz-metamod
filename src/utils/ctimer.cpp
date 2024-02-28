/*
	Credit to CS2Fixes: https://github.com/Source2ZE/CS2Fixes/blob/0eae82a64c71466ca498e3719f1cf3e61b174dee/src/ctimer.cpp
*/

#include "ctimer.h"

CUtlLinkedList<CTimerBase*> g_timers;

void RemoveTimers()
{
	g_timers.PurgeAndDeleteElements();
}

void RemoveMapTimers()
{
    for (int i = g_timers.Tail(); i != g_timers.InvalidIndex();)
    {
        int prevIndex = i;
        i = g_timers.Previous(i);

        if(g_timers[prevIndex]->m_bPreserveMapChange)
            continue;

        delete g_timers[prevIndex];
        g_timers.Remove(prevIndex);
    }
}
