#include "ctimer.h"

std::vector<CTimerBase *> g_NonPersistentTimers;
std::vector<CTimerBase *> g_PersistentTimers;

static_function void ProcessTimerList(std::vector<CTimerBase *> &timers)
{
	for (size_t i = timers.size(); i-- > 0;)
	{
		auto timer = timers[i];
		// The server's realtime and curtime both start over with each map, which a persistent timer would otherwise wait out.
		f64 currentTime = timer->useRealTime ? Plat_FloatTime() : g_pKZUtils->GetGlobals()->curtime;
		if (timer->lastExecute == -1 || currentTime < timer->lastExecute)
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
