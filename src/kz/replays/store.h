#pragma once
#include "common.h"
#include "kz_replay.h"
#include "utils/uuid.h"

#include <functional>
#include <vector>

struct ReplayFilterCriteria;

// The HTTP replay store set up in the "store" block of the replay options.
// docs/replay-store.md describes the protocol.
namespace KZ::replaysystem::store
{
	using QueryCallback = std::function<void(bool success, u64 total, std::vector<std::pair<UUID_t, ReplayHeader>> &&replays)>;

	// Whether the "store" block has an upload URL.
	bool IsEnabled();
	// Resumes the uploads the ledger still has pending.
	void Init();
	// Starts the uploads that are due at most once a second.
	void ProcessUploads();
	// `data` is the replay file if it is still in memory.
	void QueueUpload(const UUID_t &uuid, std::vector<char> &&data = {});

	bool HasQueryEndpoint();
	void QueryReplays(const ReplayFilterCriteria &criteria, u64 offset, u64 limit, QueryCallback callback);
	void QueryReplaysByUUIDSubstring(const std::string &text, QueryCallback callback);
	void QueryReplaysByUUID(const std::vector<UUID_t> &uuids, QueryCallback callback);
} // namespace KZ::replaysystem::store
