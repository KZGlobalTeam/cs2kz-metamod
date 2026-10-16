#include "ctimer.h"

std::vector<CTimerBase *> g_NonPersistentTimers;
std::vector<CTimerBase *> g_PersistentTimers;

static_function void ProcessTimerList(std::vector<CTimerBase *> &timers)
{
	for (int i = (i32)timers.size() - 1; i >= 0; i--)
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
				timers.erase(timers.begin() + i);
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

void RemoveAllTimers()
{
	for (CTimerBase *timer : g_PersistentTimers)
	{
		delete timer;
	}
	g_PersistentTimers.clear();
	for (CTimerBase *timer : g_NonPersistentTimers)
	{
		delete timer;
	}
	g_NonPersistentTimers.clear();
}

void RemoveNonPersistentTimers()
{
	for (CTimerBase *timer : g_NonPersistentTimers)
	{
		delete timer;
	}
	g_NonPersistentTimers.clear();
}
