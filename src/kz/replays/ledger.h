#pragma once
#include "common.h"
#include "utils/uuid.h"

#include <functional>
#include <unordered_map>

// What is left to do with the replays this server recorded.
// It is saved next to the replays so archive times and pending uploads survive a restart.
namespace KZ::replaysystem::ledger
{
	struct Entry
	{
		// The unix time the retention rules archived the replay at or 0 if they keep it.
		u64 archivedAt {};
		bool uploadToStore {};
		bool uploadToAPI {};
		// A run whose replay may still be renamed to the UUID the API gives it.
		bool waitingForUUID {};
		// A confirmed upload means this server no longer needs the file.
		bool offloaded {};

		bool IsPending() const
		{
			return this->uploadToStore || this->uploadToAPI || this->waitingForUUID;
		}

		bool IsEmpty() const
		{
			return !this->archivedAt && !this->IsPending() && !this->offloaded;
		}

		bool operator==(const Entry &other) const
		{
			return this->archivedAt == other.archivedAt && this->uploadToStore == other.uploadToStore && this->uploadToAPI == other.uploadToAPI
				   && this->waitingForUUID == other.waitingForUUID && this->offloaded == other.offloaded;
		}
	};

	void Load();
	// Writes the ledger if it changed since the last save.
	void Save();

	std::unordered_map<UUID_t, Entry> GetEntries();
	// An entry left with nothing to track is dropped.
	void UpdateEntry(const UUID_t &uuid, const std::function<void(Entry &entry)> &update);
	void RemoveEntry(const UUID_t &uuid);
} // namespace KZ::replaysystem::ledger
