#include "ledger.h"
#include "kz_replay.h"
#include "utils/utils.h"
#include "keyvalues3.h"

#include <mutex>

namespace KZ::replaysystem::ledger
{
	static_global constexpr const char *ledgerPath = KZ_REPLAY_PATH "/archive_index.txt";

	static_global std::mutex mutex;
	static_global std::unordered_map<UUID_t, Entry> entries;
	static_global bool dirty;

	void Load()
	{
		std::lock_guard lock(mutex);
		entries.clear();
		KeyValues3 kv;
		CUtlString error;
		if (!LoadKV3FromFile(&kv, &error, ledgerPath, "GAME", g_KV3Format_Generic, 0))
		{
			return;
		}
		for (i32 i = 0; i < kv.GetMemberCount(); i++)
		{
			const char *name = kv.GetMemberName(i);
			KeyValues3 *member = kv.GetMember(i);
			UUID_t uuid(false);
			if (!member || !name || !member->IsTable() || !UUID_t::FromString(name, &uuid))
			{
				continue;
			}
			Entry entry;
			entry.archivedAt = member->GetMemberUInt64("archived");
			entry.uploadToStore = member->GetMemberBool("store");
			entry.uploadToAPI = member->GetMemberBool("api");
			entry.waitingForUUID = member->GetMemberBool("waiting");
			entry.offloaded = member->GetMemberBool("offloaded");
			if (!entry.IsEmpty())
			{
				entries[uuid] = entry;
			}
		}
	}

	void Save()
	{
		KeyValues3 kv;
		{
			std::lock_guard lock(mutex);
			if (!dirty)
			{
				return;
			}
			dirty = false;
			for (const auto &[uuid, entry] : entries)
			{
				KeyValues3 *member = kv.FindOrCreateMember(uuid.ToString().c_str());
				member->SetToEmptyTable();
				if (entry.archivedAt)
				{
					member->SetMemberUInt64("archived", entry.archivedAt);
				}
				const std::pair<const char *, bool> flags[] = {
					{"store", entry.uploadToStore}, {"api", entry.uploadToAPI}, {"waiting", entry.waitingForUUID}, {"offloaded", entry.offloaded}};
				for (const auto &[name, value] : flags)
				{
					if (value)
					{
						member->SetMemberBool(name, true);
					}
				}
			}
		}

		CUtlString error;
		CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);
		if (!SaveKV3(g_KV3Encoding_Text, g_KV3Format_Generic, &kv, &error, &buf, KV3_SAVE_TEXT_NONE))
		{
			KZ_LOG_WARN(LogChannel::Replays, "Failed to serialize the replay index: %s\n", error.Get());
			return;
		}
		const char *base = static_cast<const char *>(buf.Base());
		if (!utils::WriteBufferToFile(ledgerPath, std::vector<char>(base, base + buf.TellPut())))
		{
			KZ_LOG_WARN(LogChannel::Replays, "Failed to save the replay index to %s\n", ledgerPath);
		}
	}

	std::unordered_map<UUID_t, Entry> GetEntries()
	{
		std::lock_guard lock(mutex);
		return entries;
	}

	void UpdateEntry(const UUID_t &uuid, const std::function<void(Entry &entry)> &update)
	{
		std::lock_guard lock(mutex);
		auto it = entries.find(uuid);
		Entry entry = it == entries.end() ? Entry() : it->second;
		const Entry before = entry;
		update(entry);
		if (before == entry)
		{
			return;
		}
		if (entry.IsEmpty())
		{
			entries.erase(uuid);
		}
		else
		{
			entries[uuid] = entry;
		}
		dirty = true;
	}

	void RemoveEntry(const UUID_t &uuid)
	{
		std::lock_guard lock(mutex);
		if (entries.erase(uuid))
		{
			dirty = true;
		}
	}
} // namespace KZ::replaysystem::ledger
