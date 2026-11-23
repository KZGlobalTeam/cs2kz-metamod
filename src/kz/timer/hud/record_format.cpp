#include "kz/timer/hud/record_format.h"
#include "kz/language/kz_language.h"
#include "utils/uuid.h"

#include <chrono>

#include "tier0/memdbgon.h"

std::string KZ::records::FormatThousands(f64 value)
{
	std::string digits = std::to_string((u64)floor(value));
	for (i32 i = (i32)digits.size() - 3; i > 0; i -= 3)
	{
		digits.insert(i, ",");
	}
	return digits;
}

std::string KZ::records::FormatRankingTime(f64 time)
{
	const u64 ms = (u64)(time * 1000.0 + 0.5);
	const u64 hours = ms / 3600000;
	const u64 minutes = ms / 60000 % 60;
	const u64 seconds = ms / 1000 % 60;
	if (hours > 0)
	{
		return tfm::format("%02llu:%02llu:%02llu.%03llu", hours, minutes, seconds, ms % 1000);
	}
	if (minutes > 0)
	{
		return tfm::format("%02llu:%02llu.%03llu", minutes, seconds, ms % 1000);
	}
	return tfm::format("%02llu.%03llu", seconds, ms % 1000);
}

struct AgoUnit
{
	i64 seconds;
	const char *one;
	const char *many;
};

static_global const AgoUnit agoUnits[] = {
	{31557600, "Course Top - Year Ago", "Course Top - Years Ago"}, {2629800, "Course Top - Month Ago", "Course Top - Months Ago"},
	{86400, "Course Top - Day Ago", "Course Top - Days Ago"},      {3600, "Course Top - Hour Ago", "Course Top - Hours Ago"},
	{60, "Course Top - Minute Ago", "Course Top - Minutes Ago"},   {1, "Course Top - Second Ago", "Course Top - Seconds Ago"},
};

// Record IDs are UUIDv7 and start with the submission time.
std::string KZ::records::FormatAgo(KZPlayer *player, const std::string &id)
{
	UUID_t uuid(false);
	if (id.empty() || !UUID_t::FromString(id.c_str(), &uuid))
	{
		return "-";
	}
	const i64 now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	const i64 elapsed = now - (i64)uuid.GetTimestamp();
	const i64 age = elapsed > 0 ? elapsed / 1000 : 0;
	// Uses the largest unit that fits and rounds it.
	for (const AgoUnit &unit : agoUnits)
	{
		if (age < unit.seconds && unit.seconds > 1)
		{
			continue;
		}
		const i64 count = (age + unit.seconds / 2) / unit.seconds;
		return player->languageService->PrepareMessage(count == 1 ? unit.one : unit.many, (i32)count);
	}
	return "-";
}

// The time is in UTC because the server doesn't know the player's time zone.
std::string KZ::records::FormatDate(const std::string &id)
{
	UUID_t uuid(false);
	if (id.empty() || !UUID_t::FromString(id.c_str(), &uuid))
	{
		return "-";
	}
	const i64 seconds = (i64)(uuid.GetTimestamp() / 1000);
	const i64 secondOfDay = seconds % 86400;
	// Days since 1970-01-01 to a civil date (Howard Hinnant's civil_from_days).
	const i64 z = seconds / 86400 + 719468;
	const i64 era = z / 146097;
	const i64 dayOfEra = z - era * 146097;
	const i64 yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
	const i64 dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
	const i64 mp = (5 * dayOfYear + 2) / 153;
	const i64 day = dayOfYear - (153 * mp + 2) / 5 + 1;
	const i64 month = mp < 10 ? mp + 3 : mp - 9;
	const i64 year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
	return tfm::format("%04lld-%02lld-%02lld %02lld:%02lld:%02lld UTC", year, month, day, secondOfDay / 3600, secondOfDay / 60 % 60,
					   secondOfDay % 60);
}
