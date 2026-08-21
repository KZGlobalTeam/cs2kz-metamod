#include "kz_memtrack.h"

#include "utils/plat.h"
#include "utils/utils.h"

#include "convar.h"
#include "strtools.h"

#include "tier0/memdbgon.h"

static_function void OnSampleRateChanged(CConVar<int32> *ref, CSplitScreenSlot slot, const int32 *newValue, const int32 *oldValue)
{
	KZMem::SetSampleRate((uint32_t)(*newValue < 0 ? 0 : *newValue));
}

CConVar<int32> kz_meminfo_sample_rate("kz_meminfo_sample_rate", FCVAR_GAMEDLL,
									  "For allocations that reach the allocator through tier0 or the CRT, how often to stack-walk for the real "
									  "call site: 1 = every one (exact, ~400ns each), 0 = never. Leak volume is exact regardless.",
									  64, OnSampleRateChanged);

namespace
{
	constexpr uint32_t DEFAULT_TOP_COUNT = 15;
	constexpr uint32_t MAX_TOP_COUNT = 64;

	void FormatBytes(int64_t bytes, char *buffer, size_t bufferSize)
	{
		bool negative = bytes < 0;
		double value = (double)(negative ? -bytes : bytes);

		const char *unit = "B";
		if (value >= 1024.0 * 1024.0 * 1024.0)
		{
			value /= 1024.0 * 1024.0 * 1024.0;
			unit = "GB";
		}
		else if (value >= 1024.0 * 1024.0)
		{
			value /= 1024.0 * 1024.0;
			unit = "MB";
		}
		else if (value >= 1024.0)
		{
			value /= 1024.0;
			unit = "KB";
		}

		V_snprintf(buffer, (int)bufferSize, "%s%.1f %s", negative ? "-" : "", value, unit);
	}

	uint32_t ParseCount(const CCommand &args, int argIndex)
	{
		if (args.ArgC() <= argIndex)
		{
			return DEFAULT_TOP_COUNT;
		}

		int requested = V_StringToInt32(args.Arg(argIndex), (int)DEFAULT_TOP_COUNT);
		if (requested <= 0)
		{
			return DEFAULT_TOP_COUNT;
		}
		return (uint32_t)(requested > (int)MAX_TOP_COUNT ? MAX_TOP_COUNT : requested);
	}

	void PrintSummary()
	{
		KZMem::Totals totals = KZMem::GetTotals();

		char live[32];
		char peak[32];
		FormatBytes(totals.liveBytes, live, sizeof(live));
		FormatBytes(totals.peakBytes, peak, sizeof(peak));

		KZ_LOG_INFO(LogChannel::General, "=== CS2KZ memory ===\n");
		KZ_LOG_INFO(LogChannel::General, "  Tracking       : %s\n", KZMem::GetStatusString());

		size_t rss = Plat_GetProcessRSS();
		if (rss)
		{
			char rssText[32];
			FormatBytes((int64_t)rss, rssText, sizeof(rssText));
			KZ_LOG_INFO(LogChannel::General, "  Process RSS    : %s (whole server, not just CS2KZ)\n", rssText);
		}

		KZ_LOG_INFO(LogChannel::General, "  Plugin live    : %s in %lld blocks\n", live, (long long)totals.liveBlocks);
		KZ_LOG_INFO(LogChannel::General, "  Peak           : %s\n", peak);
		KZ_LOG_INFO(LogChannel::General, "  Churn          : %llu allocs / %llu frees\n", (unsigned long long)totals.totalAllocs,
					(unsigned long long)totals.totalFrees);
		KZ_LOG_INFO(LogChannel::General, "  Table          : %llu / %llu entries\n", (unsigned long long)totals.tableEntries,
					(unsigned long long)totals.tableCapacity);
		KZ_LOG_INFO(LogChannel::General, "  Sampled walks  : %llu (1 in %u)\n", (unsigned long long)totals.sampledWalks, KZMem::GetSampleRate());

		if (totals.untrackedFrees || totals.orphanedFrees)
		{
			KZ_LOG_INFO(LogChannel::General, "  Untracked frees: %llu (allocated before the hook went in)\n",
						(unsigned long long)totals.untrackedFrees);
			KZ_LOG_INFO(LogChannel::General, "  Orphaned frees : %llu (ours, released by another module)\n",
						(unsigned long long)totals.orphanedFrees);
		}

		if (totals.overflowed)
		{
			KZ_LOG_WARN(LogChannel::General, "  Tracking table hit capacity - the figures above are incomplete.\n");
		}
	}

	void PrintCallsiteTable(const char *title, const KZMem::CallsiteStats *entries, uint32_t count)
	{
		KZ_LOG_INFO(LogChannel::General, "\n  %s\n", title);
		KZ_LOG_INFO(LogChannel::General, "  %-26s %14s %12s %12s\n", "Call site", "Live", "Blocks", "Allocs");
		KZ_LOG_INFO(LogChannel::General, "  %-26s %14s %12s %12s\n", "--------------------------", "--------------", "------------", "------------");

		for (uint32_t i = 0; i < count; i++)
		{
			char address[40];
			char live[32];
			KZMem::FormatAddress(entries[i].address, address, sizeof(address));
			FormatBytes(entries[i].liveBytes, live, sizeof(live));

			char allocs[24];
			if (entries[i].totalAllocs)
			{
				V_snprintf(allocs, sizeof(allocs), "%llu", (unsigned long long)entries[i].totalAllocs);
			}
			else
			{
				// Not applicable in the leaks view - see GetLeaksSinceMark.
				V_strncpy(allocs, "-", sizeof(allocs));
			}

			KZ_LOG_INFO(LogChannel::General, "  %-26s %14s %12lld %12s\n", address, live, (long long)entries[i].liveBlocks, allocs);
		}

		if (count == 0)
		{
			KZ_LOG_INFO(LogChannel::General, "  (nothing to report)\n");
		}
		else
		{
			KZ_LOG_INFO(LogChannel::General, "\n  Offsets are relative to the cs2kz module base; resolve with addr2line (Linux)\n"
											 "  or the .pdb (Windows).\n");
		}
	}

	void PrintFooter()
	{
		KZ_LOG_INFO(LogChannel::General, "\n  Scope: allocations made while cs2kz is on the stack, which includes memory the\n"
										 "  engine allocates on our behalf inside our own hooks. The mode and style plugins\n"
										 "  are separate modules and are not counted.\n");
	}

	void PrintUsage()
	{
		KZ_LOG_INFO(LogChannel::General, "Usage:\n");
		KZ_LOG_INFO(LogChannel::General, "  kz_meminfo               - summary\n");
		KZ_LOG_INFO(LogChannel::General, "  kz_meminfo callsites [n] - top call sites by live bytes\n");
		KZ_LOG_INFO(LogChannel::General, "  kz_meminfo mark          - snapshot; anything allocated after this is a leak candidate\n");
		KZ_LOG_INFO(LogChannel::General, "  kz_meminfo leaks [n]     - still-live blocks allocated since the mark, by call site\n");
		KZ_LOG_INFO(LogChannel::General, "  kz_meminfo reset         - clear peak and churn counters\n");
		KZ_LOG_INFO(LogChannel::General, "\n");
		KZ_LOG_INFO(LogChannel::General, "To hunt a leak: kz_meminfo mark, reproduce (a few changelevels), then kz_meminfo leaks.\n");
		KZ_LOG_INFO(LogChannel::General, "Set kz_meminfo_sample_rate 1 first for exact call sites on tier0/CRT paths.\n");
	}
} // namespace

CON_COMMAND_F(kz_meminfo, "Report CS2KZ memory usage, allocation hot spots and suspected leaks.", FCVAR_GAMEDLL)
{
	if (!KZMem::IsActive())
	{
		KZ_LOG_WARN(LogChannel::General, "Memory tracking is not active: %s\n", KZMem::GetStatusString());
		return;
	}

	const char *subCommand = args.ArgC() > 1 ? args.Arg(1) : "";

	if (!subCommand[0])
	{
		PrintSummary();
		PrintFooter();
		return;
	}

	if (KZ_STREQI(subCommand, "callsites"))
	{
		uint32_t count = ParseCount(args, 2);
		KZMem::CallsiteStats entries[MAX_TOP_COUNT];
		uint32_t found = KZMem::GetTopCallsites(entries, count);
		PrintCallsiteTable("Top call sites by live bytes", entries, found);
		return;
	}

	if (KZ_STREQI(subCommand, "mark"))
	{
		KZMem::SetMark();
		KZ_LOG_INFO(LogChannel::General, "Mark set. Reproduce the leak, then run: kz_meminfo leaks\n");
		return;
	}

	if (KZ_STREQI(subCommand, "leaks"))
	{
		if (!KZMem::HasMark())
		{
			KZ_LOG_WARN(LogChannel::General, "No mark set. Run kz_meminfo mark first, reproduce, then try again.\n");
			return;
		}

		uint32_t count = ParseCount(args, 2);
		KZMem::CallsiteStats entries[MAX_TOP_COUNT];
		uint32_t found = KZMem::GetLeaksSinceMark(entries, count);
		PrintCallsiteTable("Still live, allocated since the mark", entries, found);
		return;
	}

	if (KZ_STREQI(subCommand, "reset"))
	{
		KZMem::ResetCounters();
		KZ_LOG_INFO(LogChannel::General, "Peak and churn counters cleared. Live blocks are untouched.\n");
		return;
	}

	PrintUsage();
}
