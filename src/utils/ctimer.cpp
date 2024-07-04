#include "ctimer.h"

CUtlVector<CTimerBase *> g_NonPersistentTimers;
CUtlVector<CTimerBase *> g_PersistentTimers;

static_function void ProcessTimerList(CUtlVector<CTimerBase *> &timers)
{
	for (int i = timers.Count() - 1; i >= 0; i--)
	{
		auto timer = timers[i];
		f64 currentTime = timer->useRealTime ? g_pKZUtils->GetGlobals()->realtime : g_pKZUtils->GetGlobals()->curtime;
		if (timer->lastExecute == -1)
		{
			timer->lastExecute = currentTime;
		}

		if (timer->lastExecute + timer->interval <= currentTime)
		{
			if (!timer->Execute())
			{
				delete timer;
				timers.Remove(i);
			}
			else
			{
				timer->lastExecute = currentTime;
			}
		}
	}
}

void ProcessTimers()
{
	ProcessTimerList(g_PersistentTimers);
	ProcessTimerList(g_NonPersistentTimers);
}

void RemoveNonPersistentTimers()
{
	g_NonPersistentTimers.PurgeAndDeleteElements();
}
