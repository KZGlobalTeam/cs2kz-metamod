#pragma once
#include "kz/kz.h"

#include <string>

// Record formatting shared by the windows that list records.
namespace KZ::records
{
	// 9812.5 becomes "9,812".
	std::string FormatThousands(f64 value);
	// 05.123, 01:11.719 and 01:02:03.456.
	std::string FormatRankingTime(f64 time);
	// "3 days ago" from the timestamp in a record's UUIDv7 or "-" if it has none.
	std::string FormatAgo(KZPlayer *player, const std::string &id);
	// "2025-03-12 18:04:51 UTC" from the timestamp in a record's UUIDv7 or "-" if it has none.
	std::string FormatDate(const std::string &id);
} // namespace KZ::records
